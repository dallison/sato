#pragma once

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/types/span.h"
#include "toolbelt/hexdump.h"
#include <array>
#include <iostream>
#include <sstream>
#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <string_view>
#include <vector>

namespace sato {

// Provides a statically sized or dynamic ROSBuffer used for serialization
// of messages.
class ROSBuffer {
public:
  // Dynamic ROSBuffer with own memory allocation.
  ROSBuffer(size_t initial_size = 16) : owned_(true), size_(initial_size) {
    if (initial_size < 16) {
      // Need a reasonable size to start with.
      abort();
    }
    start_ = reinterpret_cast<char *>(malloc(size_));
    if (start_ == nullptr) {
      abort();
    }
    addr_ = start_;
    end_ = start_ + size_;
  }

  // Fixed ROSBuffer in non-owned memory.
  ROSBuffer(char *addr, size_t size)
      : owned_(false), start_(addr), size_(size), addr_(addr),
        end_(addr + size) {}

  ~ROSBuffer() {
    if (owned_) {
      free(start_);
    }
  }

  size_t Size() const { return addr_ - start_; }

  size_t size() const { return Size(); }

  template <typename T> T *Data() { return reinterpret_cast<T *>(start_); }

  char *data() { return Data<char>(); }

  std::string AsString() const { return std::string(start_, addr_ - start_); }

  template <typename T> absl::Span<const T> AsSpan() const {
    return absl::Span<T>(reinterpret_cast<const T *>(start_), addr_ - start_);
  }

  void Clear() {
    addr_ = start_;
    end_ = start_;
  }

  void Rewind() { addr_ = start_; }

  absl::Status CheckAtEnd() const {
    if (addr_ != end_) {
      return absl::InternalError(
          absl::StrFormat("Extra data in ROSBuffer: start: %p, addr: %p, end_ %p",
                          start_, addr_, end_));
    }
    return absl::OkStatus();
  }

  char *&Addr() const { return addr_; }

  absl::Status HasSpaceFor(size_t n) {
    char *next = addr_ + n;
    // Off-by-one complexity here.  The end is one past the end of the ROSBuffer.
    if (next > end_) {
      if (owned_) {
        // Expand the ROSBuffer.
        size_t new_size = size_ * 2;

        char *new_start = reinterpret_cast<char *>(realloc(start_, new_size));
        if (new_start == nullptr) {
          abort();
        }
        size_t curr_length = addr_ - start_;
        start_ = new_start;
        addr_ = start_ + curr_length;
        end_ = start_ + new_size;
        size_ = new_size;
        return absl::OkStatus();
      }
      return absl::InternalError(absl::StrFormat(
          "No space in ROSBuffer: length: %d, need: %d", size_, next - start_));
    }
    return absl::OkStatus();
  }

  absl::Status Check(size_t n) const {
    char *next = addr_ + n;
    if (next <= end_) {
      return absl::OkStatus();
    }
    return absl::InternalError(
        absl::StrFormat("ROSBuffer overun when checking for %d bytes; current "
                        "address is %p, end is %p",
                        n, addr_, end_));
  }

private:
  bool owned_ = false;           // Memory is owned by this ROSBuffer.
  char *start_ = nullptr;        // Start of memory.
  size_t size_ = 0;              // Size of memory.
  mutable char *addr_ = nullptr; // Current read/write address.
  char *end_ = nullptr;          // End of ROSBuffer.
  mutable int num_zeroes_ = 0; // Number of zero bytes to write in compact mode.
};

// Alignment is not guaranteed for any copies so to comply with
// norms we use memcpy.  Although all modern CPUs allow non-aligned
// word reads and writes they can come with a performance degradation.
// It won't make any difference anyway since the biggest performance
// issue with serialization is large data sets, like camera images.
template <typename T> inline absl::Status Write(ROSBuffer &b, const T &v) {
  if (absl::Status status = b.HasSpaceFor(sizeof(T)); !status.ok()) {
    return status;
  }
  memcpy(b.Addr(), &v, sizeof(T));
  b.Addr() += sizeof(T);
  return absl::OkStatus();
}

template <typename T> inline absl::Status Read(const ROSBuffer &b, T &v) {
  if (absl::Status status = b.Check(sizeof(T)); !status.ok()) {
    return status;
  }
  memcpy(&v, b.Addr(), sizeof(T));
  b.Addr() += sizeof(T);
  return absl::OkStatus();
}

template <> inline absl::Status Write(ROSBuffer &b, const std::string &v) {
  if (absl::Status status = b.HasSpaceFor(4 + v.size()); !status.ok()) {
    return status;
  }

  uint32_t size = static_cast<uint32_t>(v.size());
  memcpy(b.Addr(), &size, sizeof(size));
  memcpy(b.Addr() + 4, v.data(), v.size());
  b.Addr() += 4 + v.size();
  return absl::OkStatus();
}

template <> inline absl::Status Read(const ROSBuffer &b, std::string &v) {
  if (absl::Status status = b.Check(4); !status.ok()) {
    return status;
  }
  uint32_t size = 0;
  memcpy(&size, b.Addr(), sizeof(size));
  if (absl::Status status = b.Check(size_t(size)); !status.ok()) {
    return status;
  }
  v.resize(size);
  memcpy(v.data(), b.Addr() + 4, size);
  b.Addr() += 4 + v.size();
  return absl::OkStatus();
}

template <typename T>
inline absl::Status Write(ROSBuffer &b, const std::vector<T> &vec) {
  if (absl::Status status = b.HasSpaceFor(4); !status.ok()) {
    return status;
  }
  uint32_t size = static_cast<uint32_t>(vec.size());
  memcpy(b.Addr(), &size, sizeof(size));
  b.Addr() += 4;
  for (auto &v : vec) {
    if (absl::Status status = Write(b, v); !status.ok()) {
      return status;
    }
  }
  return absl::OkStatus();
}

template <typename T>
inline absl::Status Read(const ROSBuffer &b, std::vector<T> &vec) {
  if (absl::Status status = b.Check(4); !status.ok()) {
    return status;
  }
  uint32_t size = 0;
  memcpy(&size, b.Addr(), sizeof(size));
  b.Addr() += 4;
  vec.resize(size);
  for (uint32_t i = 0; i < size; i++) {
    if (absl::Status status = Read(b, vec[i]); !status.ok()) {
      return status;
    }
  }
  return absl::OkStatus();
}

template <typename T, size_t N>
inline absl::Status Write(ROSBuffer &b, const std::array<T, N> &vec) {
  for (auto &v : vec) {
    if (absl::Status status = Write(b, v); !status.ok()) {
      return status;
    }
  }
  return absl::OkStatus();
}

template <size_t N>
inline absl::Status Write(ROSBuffer &b, const std::array<uint8_t, N> &vec) {
  if (absl::Status status = b.HasSpaceFor(N); !status.ok()) {
    return status;
  }
  memcpy(b.Addr(), vec.data(), N);
  b.Addr() += N;
  return absl::OkStatus();
}

template <typename T, size_t N>
inline absl::Status Read(const ROSBuffer &b, std::array<T, N> &vec) {
  for (int i = 0; i < N; i++) {
    if (absl::Status status = Read(b, vec[i]); !status.ok()) {
      return status;
    }
  }
  return absl::OkStatus();
}

template <size_t N>
inline absl::Status Read(const ROSBuffer &b, std::array<uint8_t, N> &vec) {
  if (absl::Status status = b.Check(N); !status.ok()) {
    return status;
  }
  memcpy(vec.data(), b.Addr(), N);
  b.Addr() += N;
  return absl::OkStatus();
}

#if 0
template <> inline absl::Status Write(ROSBuffer &b, const Time &t) {
  if (absl::Status status = Write(b, t.secs); !status.ok()) {
    return status;
  }
  if (absl::Status status = Write(b, t.nsecs); !status.ok()) {
    return status;
  }
  return absl::OkStatus();
}

template <> inline absl::Status Read(const ROSBuffer &b, Time &t) {
  if (absl::Status status = Read(b, t.secs); !status.ok()) {
    return status;
  }
  if (absl::Status status = Read(b, t.nsecs); !status.ok()) {
    return status;
  }
  return absl::OkStatus();
}

template <> inline absl::Status Write(ROSBuffer &b, const Duration &d) {
  if (absl::Status status = Write(b, d.secs); !status.ok()) {
    return status;
  }
  if (absl::Status status = Write(b, d.nsecs); !status.ok()) {
    return status;
  }
  return absl::OkStatus();
}

template <> inline absl::Status Read(const ROSBuffer &b, Duration &d) {
  if (absl::Status status = Read(b, d.secs); !status.ok()) {
    return status;
  }
  if (absl::Status status = Read(b, d.nsecs); !status.ok()) {
    return status;
  }
  return absl::OkStatus();
}
#endif

} // namespace sato
