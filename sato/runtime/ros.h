#pragma once

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/types/span.h"
#include "neutron/common_runtime.h"
#include "toolbelt/hexdump.h"
#include <array>
#include <iostream>
#include <sstream>
#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <string_view>
#include <vector>

namespace neutron::serdes {

// Base class for all messages.
class SerdesMessage {};

constexpr uint8_t kZeroMarker = 0xfa;
// The max number of zeroes in a run is one more than than the zero marker since
// the zero marker is followed by the number of zeroes - 2
constexpr size_t kMaxZeroes = kZeroMarker + 1;

template <typename T> inline size_t SignedLeb128Size(const T &v) {
  size_t size = 0;
  bool more = true;
  T value = v;
  while (more) {
    uint8_t byte = value & 0x7f;
    value >>= 7;
    bool sign_bit = (byte & 0x40) != 0;
    if ((value == 0 && !sign_bit) || (value == -1 && sign_bit)) {
      more = false;
    } else {
      byte |= 0x80;
    }
    size++;
    if (byte == kZeroMarker) {
      // Need to escape kZeroMarker because it is a zero-run marker.
      // kZeroMarker is written as kZeroMarker, kZeroMarker
      size++;
    }
  }
  return size;
}

template <typename T> inline size_t UnsignedLeb128Size(const T &x) {
  size_t size = 0;
  T v = x;
  do {
    size++;
    // If this will be encoded as kZeroMarker we need to escape it as it will be
    // written as kZeroMarker, kZeroMarker.
    if ((v & 0x7f) == (kZeroMarker & 0x7f) && (v >> 7) != 0) {
      size++;
    }
    v >>= 7;
  } while (v != 0);
  return size;
}

template <typename T> inline size_t Leb128Size(const T &v) {
  if constexpr (std::is_unsigned<T>::value) {
    return UnsignedLeb128Size(v);
  } else {
    return SignedLeb128Size(v);
  }
}

template <> inline size_t Leb128Size(const std::string &v) {
  return Leb128Size(v.size()) + v.size();
}

inline size_t Leb128Size(const float &v) { return sizeof(v); }

inline size_t Leb128Size(const double &v) { return sizeof(v); }

inline size_t Leb128Size(const Time &t) {
  return Leb128Size(t.secs) + Leb128Size(t.nsecs);
}

inline size_t Leb128Size(const Duration &d) {
  return Leb128Size(d.secs) + Leb128Size(d.nsecs);
}

template <typename T> inline size_t Leb128Size(const std::vector<T> &v) {
  size_t size = Leb128Size(v.size());
  for (auto &e : v) {
    size += Leb128Size(e);
  }
  return size;
}

template <> inline size_t Leb128Size(const std::vector<uint8_t> &v) {
  // Body is not leb128 encoded so that we can use memcpy.
  size_t size = Leb128Size(v.size());
  return size + v.size();
}

template <typename T, size_t N>
inline size_t Leb128Size(const std::array<T, N> &v) {
  size_t size = 0;
  for (auto &e : v) {
    size += Leb128Size(e);
  }
  return size;
}

template <size_t N> inline size_t Leb128Size(const std::array<uint8_t, N> &v) {
  return N;
}

struct SizeAccumulator {
  void Close() {
    if (num_zeroes_ > 0) {
      if (num_zeroes_ == 1) {
        size_++;
      } else {
        size_ += 2;
      }
      num_zeroes_ = 0;
    }
  }

  size_t Size() const { return size_; }

  size_t size_ = 0;
  int num_zeroes_ = 0;
};

  template <typename T> inline void Accumulate(SizeAccumulator& acc, const T &v) {
    if (v == 0) {
      if (acc.num_zeroes_ == kMaxZeroes) {
        acc.size_ += 2;
        acc.num_zeroes_ = 1;
      } else {
        acc.num_zeroes_++;
      }
    } else {
      if (acc.num_zeroes_ > 0) {
        if (acc.num_zeroes_ == 1) {
          acc.size_++;
        } else {
          acc.size_ += 2;
        }
        acc.num_zeroes_ = 0;
      }
      acc.size_ += Leb128Size(v);
    }
  }

  template <> inline void Accumulate(SizeAccumulator& acc, const float &v) {
    const uint32_t *p = reinterpret_cast<const uint32_t *>(&v);
    Accumulate(acc, *p);
  }

  template <> inline void Accumulate(SizeAccumulator& acc, const double &v) {
    const uint64_t *p = reinterpret_cast<const uint64_t *>(&v);
    Accumulate(acc, *p);
  }

  template <> inline void Accumulate(SizeAccumulator& acc, const std::string &s) {
    Accumulate(acc, s.size());
    acc.size_ += s.size();
  }

  template <> inline void Accumulate(SizeAccumulator& acc, const Time &t) {
    Accumulate(acc, t.secs);
    Accumulate(acc, t.nsecs);
  }

  template <> inline void Accumulate(SizeAccumulator& acc, const Duration &d) {
    Accumulate(acc, d.secs);
    Accumulate(acc, d.nsecs);
  }

  template <typename T> inline void Accumulate(SizeAccumulator& acc, const std::vector<T> &v) {
    Accumulate(acc, v.size());
    for (auto &e : v) {
      Accumulate(acc, e);
    }
  }

  template <> inline void Accumulate(SizeAccumulator& acc, const std::vector<uint8_t> &v) {
    if (v.empty()) {
      // Empty vector has a zero length and no body.  No need to flush
      // the zeroes.
      Accumulate(acc, uint8_t(0));
      return;
    }
    acc.Close();
    Accumulate(acc, v.size());
    acc.size_ += v.size();
  }

  template <typename T, size_t N> inline void Accumulate(SizeAccumulator& acc, const std::array<T, N> &v) {
    for (auto &e : v) {
      Accumulate(acc, e);
    }
  }

  template <size_t N> inline void Accumulate(SizeAccumulator& acc, const std::array<uint8_t, N> &v) {
    acc.Close(); // Flush any pending zeroes.
    acc.size_ += N;
  }

// Provides a statically sized or dynamic buffer used for serialization
// of messages.
class Buffer {
public:
  // Dynamic buffer with own memory allocation.
  Buffer(size_t initial_size = 16) : owned_(true), size_(initial_size) {
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

  // Fixed buffer in non-owned memory.
  Buffer(char *addr, size_t size)
      : owned_(false), start_(addr), size_(size), addr_(addr),
        end_(addr + size) {}

  ~Buffer() {
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
          absl::StrFormat("Extra data in buffer: start: %p, addr: %p, end_ %p",
                          start_, addr_, end_));
    }
    return absl::OkStatus();
  }

  // These are public because we need it for vector expansion.
  template <typename T> absl::Status ReadUnsignedLeb128(T &v) const {
    int shift = 0;
    uint8_t byte;
    v = 0;
    do {
      if (absl::Status status = Get(byte); !status.ok()) {
        return status;
      }
      T b = T(byte & 0x7f);
      v |= b << shift;
      shift += 7;
    } while (byte & 0x80);

    return absl::OkStatus();
  }

  template <typename T> absl::Status WriteUnsignedLeb128(T v) {
    do {
      uint8_t byte = v & 0x7f;
      v >>= 7;
      if (v != 0) {
        byte |= 0x80;
      }
      if (absl::Status status = Put(byte); !status.ok()) {
        return status;
      }
    } while (v != 0);
    return absl::OkStatus();
  }

  absl::Status FlushZeroes() {
    if (num_zeroes_ > 0) {
      if (num_zeroes_ == 1) {
        if (absl::Status status = HasSpaceFor(1); !status.ok()) {
          return status;
        }
        *addr_++ = 0;
      } else {
        if (absl::Status status = HasSpaceFor(2); !status.ok()) {
          return status;
        }
        *addr_++ = kZeroMarker;     // Add zero marker.
        *addr_++ = num_zeroes_ - 2; // Marker is followed by count - 2.
      }
      num_zeroes_ = 0;
    }
    return absl::OkStatus();
  }

  char*& Addr() const { return addr_; }

  absl::Status HasSpaceFor(size_t n) {
    char *next = addr_ + n;
    // Off-by-one complexity here.  The end is one past the end of the buffer.
    if (next > end_) {
      if (owned_) {
        // Expand the buffer.
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
          "No space in buffer: length: %d, need: %d", size_, next - start_));
    }
    return absl::OkStatus();
  }

  absl::Status Check(size_t n) const {
    char *next = addr_ + n;
    if (next <= end_) {
      return absl::OkStatus();
    }
    return absl::InternalError(
        absl::StrFormat("Buffer overun when checking for %d bytes; current "
                        "address is %p, end is %p",
                        n, addr_, end_));
  }

  // There caller must have checked that there is space in the buffer.
  template <typename T> absl::Status WriteSignedLeb128(T value) {
    bool more = true;
    while (more) {
      uint8_t byte = value & 0x7F;
      value >>= 7;

      // Sign bit of byte is second high order bit (0x40)
      if ((value == 0 && (byte & 0x40) == 0) ||
          (value == -1 && (byte & 0x40) != 0)) {
        more = false;
      } else {
        byte |= 0x80;
      }
      if (absl::Status status = Put(byte); !status.ok()) {
        return status;
      }
    }
    return absl::OkStatus();
  }

  // This checks that there is data in the buffer to cover the value.
  template <typename T> absl::Status ReadSignedLeb128(T &value) const {
    int shift = 0;
    uint8_t byte;
    value = 0;

    do {
      if (absl::Status status = Get(byte); !status.ok()) {
        return status;
      }
      T b = T(byte & 0x7f);
      value |= b << shift;
      shift += 7;

      if ((byte & 0x80) == 0 && (byte & 0x40) != 0) {
        value |= -(1 << shift);
      }
    } while (byte & 0x80);
    return absl::OkStatus();
  }

  absl::Status Put(uint8_t ch) {
    if (ch == 0) {
      // Max of kMaxZeroes zeroes in a run.
      if (num_zeroes_ == kMaxZeroes) {
        if (absl::Status status = FlushZeroes(); !status.ok()) {
          return status;
        }
      }
      num_zeroes_++;
      return absl::OkStatus();
    }
    if (absl::Status status = FlushZeroes(); !status.ok()) {
      return status;
    }
    if (ch == kZeroMarker) {
      // Need to escape kZeroMarker because it is a zero-run marker.
      // kZeroMarker is written as kZeroMarker, kZeroMarker
      if (absl::Status status = HasSpaceFor(2); !status.ok()) {
        return status;
      }
      *addr_++ = kZeroMarker;
      *addr_++ = kZeroMarker;
      return absl::OkStatus();
    }
    if (absl::Status status = HasSpaceFor(1); !status.ok()) {
      return status;
    }
    *addr_++ = char(ch);
    return absl::OkStatus();
  }

  absl::Status Get(uint8_t &v) const {
    if (num_zeroes_ > 0) {
      // We are running through a run of zeroes.
      num_zeroes_--;
      v = 0;
      return absl::OkStatus();
    }
    if (absl::Status status = Check(1); !status.ok()) {
      return status;
    }
    // Look at next char.
    uint8_t ch = uint8_t(*addr_++);
    // If we have a zero marker, this means that we have a run of zeroes.  The
    // next byte is the count of zeroes - 2.
    // Also, kZeroMarker followed by kZeroMarker is a literal kZeroMarker.
    if (ch == kZeroMarker) {
      if (absl::Status status = Check(1); !status.ok()) {
        return status;
      }
      ch = uint8_t(*addr_++);
      if (ch == kZeroMarker) {
        // kZeroMarker is written as kZeroMarker, kZeroMarker
        v = kZeroMarker;
        return absl::OkStatus();
      }
      num_zeroes_ = ch + 1; // +1 because we will have consumed one zero.
      v = 0;
      return absl::OkStatus();
    }
    v = ch;
    return absl::OkStatus();
  }

 private:
  bool owned_ = false;           // Memory is owned by this buffer.
  char *start_ = nullptr;        // Start of memory.
  size_t size_ = 0;              // Size of memory.
  mutable char *addr_ = nullptr; // Current read/write address.
  char *end_ = nullptr;          // End of buffer.
  mutable int num_zeroes_ = 0; // Number of zero bytes to write in compact mode.
};

  // Alignment is not guaranteed for any copies so to comply with
  // norms we use memcpy.  Although all modern CPUs allow non-aligned
  // word reads and writes they can come with a performance degradation.
  // It won't make any difference anyway since the biggest performance
  // issue with serialization is large data sets, like camera images.
  template <typename T> inline absl::Status Write(Buffer& b, const T &v) {
    if (absl::Status status = b.HasSpaceFor(sizeof(T)); !status.ok()) {
      return status;
    }
    memcpy(b.Addr(), &v, sizeof(T));
    b.Addr() += sizeof(T);
    return absl::OkStatus();
  }

  template <typename T> inline absl::Status Read(const Buffer& b, T &v) {
    if (absl::Status status = b.Check(sizeof(T)); !status.ok()) {
      return status;
    }
    memcpy(&v, b.Addr(), sizeof(T));
    b.Addr() += sizeof(T);
    return absl::OkStatus();
  }

  template <typename T> inline absl::Status WriteCompact(Buffer& b, const T &v) {

    if (std::is_unsigned<T>::value) {
      if (absl::Status status = b.WriteUnsignedLeb128(v); !status.ok()) {
        return status;
      }
    } else {
      if (absl::Status status = b.WriteSignedLeb128(v); !status.ok()) {
        return status;
      }
    }
    return absl::OkStatus();
  }

  template <typename T> inline absl::Status ReadCompact(const Buffer& b, T &v) {
    if constexpr (std::is_unsigned<T>::value) {
      return b.ReadUnsignedLeb128(v);
    } else {
      return b.ReadSignedLeb128(v);
    }
  }

  template <typename T> inline absl::Status ExpandField(const Buffer& b, Buffer &dest, const T& = {}) {
    if (absl::Status status = dest.HasSpaceFor(sizeof(T)); !status.ok()) {
      return status;
    }
    T v;
    if constexpr (std::is_unsigned<T>::value) {
      if (absl::Status status = b.ReadUnsignedLeb128(v); !status.ok()) {
        return status;
      }
    } else {
      if (absl::Status status = b.ReadSignedLeb128(v); !status.ok()) {
        return status;
      }
    }
    memcpy(dest.Addr(), &v, sizeof(v));
    dest.Addr() += sizeof(T);
    return absl::OkStatus();
  }

  template <typename T> inline absl::Status CompactField(const Buffer& b, Buffer &dest, const T& = {}) {
    // Check that we have a value to compact in the source buffer.
    if (absl::Status status = b.Check(sizeof(T)); !status.ok()) {
      return status;
    }

    T v;
    memcpy(&v, b.Addr(), sizeof(v));
    if constexpr (std::is_unsigned<T>::value) {
      if (absl::Status status = dest.WriteUnsignedLeb128(v); !status.ok()) {
        return status;
      }
    } else {
      if (absl::Status status = dest.WriteSignedLeb128(v); !status.ok()) {
        return status;
      }
    }
    b.Addr() += sizeof(T);
    return absl::OkStatus();
  }

  inline absl::Status WriteCompact(Buffer& b, const float &v) {
    const uint32_t *x = reinterpret_cast<const uint32_t *>(&v);
    return WriteCompact(b, *x);
  }

  inline absl::Status ReadCompact(const Buffer& b, float &v) {
    uint32_t x = 0;
    if (absl::Status status = ReadCompact(b, x); !status.ok()) {
      return status;
    }
    v = *reinterpret_cast<float *>(&x);
    return absl::OkStatus();
  }

  inline absl::Status WriteCompact(Buffer& b, const double &v) {
    const uint64_t *x = reinterpret_cast<const uint64_t *>(&v);
    return WriteCompact(b, *x);
  }

  inline absl::Status ReadCompact(const Buffer& b, double &v) {
    uint64_t x = 0;
    if (absl::Status status = ReadCompact(b, x); !status.ok()) {
      return status;
    }
    v = *reinterpret_cast<double *>(&x);
    return absl::OkStatus();
  }

  template <> inline absl::Status ExpandField(const Buffer& b, Buffer &dest, const float&) {
    uint32_t v = 0;
    if (absl::Status status = ReadCompact(b, v); !status.ok()) {
      return status;
    }
    return Write(dest, v);
  }

  template <> inline absl::Status CompactField(const Buffer& b, Buffer &dest, const float&) {
    uint32_t v;
    memcpy(&v, b.Addr(), sizeof(v));
    b.Addr() += sizeof(float);
    return WriteCompact(dest, v);
  }

  template <> inline absl::Status ExpandField(const Buffer& b, Buffer &dest, const double&) {
    uint64_t v = 0;
    if (absl::Status status = ReadCompact(b, v); !status.ok()) {
      return status;
    }
    return Write(dest, v);
  }

  template <> inline absl::Status CompactField(const Buffer& b, Buffer &dest, const double&) {
    uint64_t v;
    memcpy(&v, b.Addr(), sizeof(v));
    b.Addr() += sizeof(double);
    return WriteCompact(dest, v);
  }

  template <> inline absl::Status Write(Buffer& b, const std::string &v) {
    if (absl::Status status = b.HasSpaceFor(4 + v.size()); !status.ok()) {
      return status;
    }

    uint32_t size = static_cast<uint32_t>(v.size());
    memcpy(b.Addr(), &size, sizeof(size));
    memcpy(b.Addr() + 4, v.data(), v.size());
    b.Addr() += 4 + v.size();
    return absl::OkStatus();
  }

  template <> inline absl::Status Read(const Buffer& b, std::string &v) {
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

  template <> inline absl::Status WriteCompact(Buffer& b, const std::string &v) {
    if (absl::Status status = b.WriteUnsignedLeb128(v.size()); !status.ok()) {
      return status;
    }
    if (absl::Status status = b.HasSpaceFor(v.size()); !status.ok()) {
      return status;
    }
    memcpy(b.Addr(), v.data(), v.size());
    b.Addr() += v.size();
    return absl::OkStatus();
  }

  template <> inline absl::Status ReadCompact(const Buffer& b, std::string &v) {
    uint32_t size = 0;
    if (absl::Status status = b.ReadUnsignedLeb128(size); !status.ok()) {
      return status;
    }

    if (absl::Status status = b.Check(size_t(size)); !status.ok()) {
      return status;
    }
    v.resize(size);
    memcpy(v.data(), b.Addr(), size);
    b.Addr() += size;
    return absl::OkStatus();
  }

  template <> inline absl::Status ExpandField(const Buffer& b, Buffer &dest, const std::string&) {
    uint32_t size = 0;
    if (absl::Status status = b.ReadUnsignedLeb128(size); !status.ok()) {
      return status;
    }
    if (absl::Status status = dest.HasSpaceFor(4 + size); !status.ok()) {
      return status;
    }
    memcpy(dest.Addr(), &size, sizeof(size));
    memcpy(dest.Addr() + 4, b.Addr(), size);
    dest.Addr() += 4 + size;
    b.Addr() += size;
    return absl::OkStatus();
  }

  template <> inline absl::Status CompactField(const Buffer& b, Buffer &dest, const std::string&) {
    uint32_t size;
    memcpy(&size, b.Addr(), sizeof(size));
    if (absl::Status status = b.Check(4 + size); !status.ok()) {
      return status;
    }
    if (absl::Status status = dest.WriteUnsignedLeb128(size); !status.ok()) {
      return status;
    }
    if (absl::Status status = dest.HasSpaceFor(size); !status.ok()) {
      return status;
    }
    memcpy(dest.Addr(), b.Addr() + 4, size);
    b.Addr() += 4 + size;
    dest.Addr() += size;
    return absl::OkStatus();
  }

  template <typename T> inline absl::Status Write(Buffer& b, const std::vector<T> &vec) {
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

  template <typename T> inline absl::Status Read(const Buffer& b, std::vector<T> &vec) {
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

  template <typename T> inline absl::Status WriteCompact(Buffer& b, const std::vector<T> &vec) {
    if (absl::Status status = b.WriteUnsignedLeb128(vec.size()); !status.ok()) {
      return status;
    }
    for (auto &v : vec) {
      if (absl::Status status = WriteCompact(b, v); !status.ok()) {
        return status;
      }
    }
    return absl::OkStatus();
  }

  // Specialization vector of uint8_t so we can use memcpy instead of processing
  // the body byte by byte.
  template <> inline absl::Status WriteCompact(Buffer& b, const std::vector<uint8_t> &vec) {
    if (vec.empty()) {
      // Empty vector has a zero length and no body.  No need to flush
      // the zeroes.
      if (absl::Status status = b.WriteUnsignedLeb128(0); !status.ok()) {
        return status;
      }
      return absl::OkStatus();
    }
    if (absl::Status status = b.FlushZeroes(); !status.ok()) {
      return status;
    }
    if (absl::Status status = b.WriteUnsignedLeb128(vec.size()); !status.ok()) {
      return status;
    }
    if (absl::Status status = b.HasSpaceFor(vec.size()); !status.ok()) {
      return status;
    }
    memcpy(b.Addr(), vec.data(), vec.size());
    b.Addr() += vec.size();
    return absl::OkStatus();
  }

  template <typename T> inline absl::Status ReadCompact(const Buffer& b, std::vector<T> &vec) {
    uint32_t size = 0;
    if (absl::Status status = b.ReadUnsignedLeb128(size); !status.ok()) {
      return status;
    }

    if (absl::Status status = b.Check(size_t(size)); !status.ok()) {
      return status;
    }

    vec.resize(size);
    for (uint32_t i = 0; i < size; i++) {
      if (absl::Status status = ReadCompact(b, vec[i]); !status.ok()) {
        return status;
      }
    }
    return absl::OkStatus();
  }

  // uint8_t specialization for memcpy.
  template <> inline absl::Status ReadCompact(const Buffer& b, std::vector<uint8_t> &vec) {
    uint32_t size = 0;
    if (absl::Status status = b.ReadUnsignedLeb128(size); !status.ok()) {
      return status;
    }

    if (absl::Status status = b.Check(size_t(size)); !status.ok()) {
      return status;
    }

    vec.resize(size);
    memcpy(vec.data(), b.Addr(), size);
    b.Addr() += size;
    return absl::OkStatus();
  }

  template <typename T>
  inline absl::Status ExpandField(const Buffer& b, const std::vector<T> &, Buffer &dest) {
    uint32_t size = 0;
    if (absl::Status status = b.ReadUnsignedLeb128(size); !status.ok()) {
      return status;
    }

    if (absl::Status status = dest.Check(size_t(size)); !status.ok()) {
      return status;
    }
    memcpy(dest.Addr(), &size, sizeof(size));
    dest.Addr() += 4;
    for (uint32_t i = 0; i < size; i++) {
      if (absl::Status status = ExpandField(b, dest, T{}); !status.ok()) {
        return status;
      }
    }
    return absl::OkStatus();
  }

  template <>
  inline absl::Status ExpandField(const Buffer& b, const std::vector<uint8_t> &, Buffer &dest) {
    uint32_t size = 0;
    if (absl::Status status = b.ReadUnsignedLeb128(size); !status.ok()) {
      return status;
    }

    if (absl::Status status = dest.Check(size_t(size)); !status.ok()) {
      return status;
    }
    memcpy(dest.Addr(), &size, sizeof(size));
    dest.Addr() += 4;
    if (absl::Status status = dest.HasSpaceFor(size); !status.ok()) {
      return status;
    }
    memcpy(dest.Addr(), b.Addr(), size);
    b.Addr() += size;
    dest.Addr() += size;
    return absl::OkStatus();
  }

  template <typename T>
  inline absl::Status CompactField(const Buffer& b, const std::vector<T> &, Buffer &dest) {
    if (absl::Status status = b.Check(4); !status.ok()) {
      return status;
    }
    uint32_t size;
    memcpy(&size, b.Addr(), sizeof(size));
    if (absl::Status status = dest.WriteUnsignedLeb128(size); !status.ok()) {
      return status;
    }

    b.Addr() += 4;
    for (uint32_t i = 0; i < size; i++) {
      if (absl::Status status = CompactField(b, dest, T{}); !status.ok()) {
        return status;
      }
    }
    return absl::OkStatus();
  }

  template <>
  inline absl::Status CompactField(const Buffer& b, const std::vector<uint8_t> &, Buffer &dest) {
    if (absl::Status status = b.Check(4); !status.ok()) {
      return status;
    }
    uint32_t size;
    memcpy(&size, b.Addr(), sizeof(size));
    if (size == 0) {
      // Empty vector has a zero length and no body.  No need to flush
      // the zeroes.
      if (absl::Status status = dest.WriteUnsignedLeb128(0); !status.ok()) {
        return status;
      }
      return absl::OkStatus();
    }
    if (absl::Status status = dest.FlushZeroes(); !status.ok()) {
      return status;
    }
    if (absl::Status status = dest.WriteUnsignedLeb128(size); !status.ok()) {
      return status;
    }

    b.Addr() += 4;
    if (absl::Status status = dest.HasSpaceFor(size); !status.ok()) {
      return status;
    }
    memcpy(dest.Addr(), b.Addr(), size);
    b.Addr() += size;
    dest.Addr() += size;
    return absl::OkStatus();
  }

  template <typename T, size_t N>
  inline absl::Status Write(Buffer& b, const std::array<T, N> &vec) {
    for (auto &v : vec) {
      if (absl::Status status = Write(b, v); !status.ok()) {
        return status;
      }
    }
    return absl::OkStatus();
  }

  template <size_t N> inline absl::Status Write(Buffer& b, const std::array<uint8_t, N> &vec) {
    if (absl::Status status = b.HasSpaceFor(N); !status.ok()) {
      return status;
    }
    memcpy(b.Addr(), vec.data(), N);
    b.Addr() += N;
    return absl::OkStatus();
  }

  template <typename T, size_t N>
  inline absl::Status Read(const Buffer& b, std::array<T, N> &vec) {
    for (int i = 0; i < N; i++) {
      if (absl::Status status = Read(b, vec[i]); !status.ok()) {
        return status;
      }
    }
    return absl::OkStatus();
  }

  template <size_t N> inline absl::Status Read(const Buffer& b, std::array<uint8_t, N> &vec) {
    if (absl::Status status = b.Check(N); !status.ok()) {
      return status;
    }
    memcpy(vec.data(), b.Addr(), N);
    b.Addr() += N;
    return absl::OkStatus();
  }

  template <typename T, size_t N>
  inline absl::Status WriteCompact(Buffer& b, const std::array<T, N> &vec) {
    for (auto &v : vec) {
      if (absl::Status status = WriteCompact(b, v); !status.ok()) {
        return status;
      }
    }
    return absl::OkStatus();
  }

  template <size_t N>
  inline absl::Status WriteCompact(Buffer& b, const std::array<uint8_t, N> &vec) {
    if (absl::Status status = b.FlushZeroes(); !status.ok()) {
      return status;
    }
    // Not compacted since want this to be a memcpy.
    return Write(b, vec);
  }

  template <typename T, size_t N>
  inline absl::Status ReadCompact(const Buffer& b, std::array<T, N> &vec) {
    for (int i = 0; i < N; i++) {
      if (absl::Status status = ReadCompact(b, vec[i]); !status.ok()) {
        return status;
      }
    }
    return absl::OkStatus();
  }

  template <size_t N>
  inline absl::Status ReadCompact(const Buffer& b, std::array<uint8_t, N> &vec) {
    return Read(b, vec);
  }

  template <typename T, size_t N>
  inline absl::Status ExpandField(const Buffer& b, const std::array<T, N> &, Buffer &dest) {
    for (int i = 0; i < N; i++) {
      if (absl::Status status = ExpandField(b, dest, T{}); !status.ok()) {
        return status;
      }
    }
    return absl::OkStatus();
  }

  template <size_t N>
  inline absl::Status ExpandField(const Buffer& b, const std::array<uint8_t, N> &, Buffer &dest) {
    if (absl::Status status = b.Check(N); !status.ok()) {
      return status;
    }
    if (absl::Status status = dest.HasSpaceFor(N); !status.ok()) {
      return status;
    }
    memcpy(dest.Addr(), b.Addr(), N);
    b.Addr() += N;
    dest.Addr() += N;
    return absl::OkStatus();
  }

  template <typename T, size_t N>
  inline absl::Status CompactField(const Buffer& b, const std::array<T, N> &, Buffer &dest) {
    for (int i = 0; i < N; i++) {
      if (absl::Status status = CompactField(b, dest, T{}); !status.ok()) {
        return status;
      }
    }
    return absl::OkStatus();
  }

  template <size_t N>
  inline absl::Status CompactField(const Buffer& b, const std::array<uint8_t, N> &, Buffer &dest) {
    if (absl::Status status = dest.FlushZeroes(); !status.ok()) {
      return status;
    }
    if (absl::Status status = dest.HasSpaceFor(N); !status.ok()) {
      return status;
    }
    if (absl::Status status = b.Check(N); !status.ok()) {
      return status;
    }
    memcpy(dest.Addr(), b.Addr(), N);
    b.Addr() += N;
    dest.Addr() += N;
    return absl::OkStatus();
  }

  template <> inline absl::Status Write(Buffer& b, const Time &t) {
    if (absl::Status status = Write(b, t.secs); !status.ok()) {
      return status;
    }
    if (absl::Status status = Write(b, t.nsecs); !status.ok()) {
      return status;
    }
    return absl::OkStatus();
  }

  template <> inline absl::Status Read(const Buffer& b, Time &t) {
    if (absl::Status status = Read(b, t.secs); !status.ok()) {
      return status;
    }
    if (absl::Status status = Read(b, t.nsecs); !status.ok()) {
      return status;
    }
    return absl::OkStatus();
  }

  template <> inline absl::Status WriteCompact(Buffer& b, const Time &t) {
    if (absl::Status status = WriteCompact(b, t.secs); !status.ok()) {
      return status;
    }
    if (absl::Status status = WriteCompact(b, t.nsecs); !status.ok()) {
      return status;
    }
    return absl::OkStatus();
  }

  template <> inline absl::Status ReadCompact(const Buffer& b, Time &t) {
    if (absl::Status status = ReadCompact(b, t.secs); !status.ok()) {
      return status;
    }
    if (absl::Status status = ReadCompact(b, t.nsecs); !status.ok()) {
      return status;
    }
    return absl::OkStatus();
  }

  template <> inline absl::Status ExpandField(const Buffer& b, Buffer &dest, const Time&) {
    if (absl::Status status = ExpandField(b, dest, uint32_t{}); !status.ok()) {
      return status;
    }
    if (absl::Status status = ExpandField(b, dest, uint32_t{}); !status.ok()) {
      return status;
    }
    return absl::OkStatus();
  }

  template <> inline absl::Status CompactField(const Buffer& b, Buffer &dest, const Time&) {
    if (absl::Status status = CompactField(b, dest, uint32_t{}); !status.ok()) {
      return status;
    }
    if (absl::Status status = CompactField(b, dest, uint32_t{}); !status.ok()) {
      return status;
    }
    return absl::OkStatus();
  }

  template <> inline absl::Status Write(Buffer& b, const Duration &d) {
    if (absl::Status status = Write(b, d.secs); !status.ok()) {
      return status;
    }
    if (absl::Status status = Write(b, d.nsecs); !status.ok()) {
      return status;
    }
    return absl::OkStatus();
  }

  template <> inline absl::Status Read(const Buffer& b, Duration &d) {
    if (absl::Status status = Read(b, d.secs); !status.ok()) {
      return status;
    }
    if (absl::Status status = Read(b, d.nsecs); !status.ok()) {
      return status;
    }
    return absl::OkStatus();
  }

  template <> inline absl::Status WriteCompact(Buffer& b, const Duration &d) {
    if (absl::Status status = WriteCompact(b, d.secs); !status.ok()) {
      return status;
    }
    if (absl::Status status = WriteCompact(b, d.nsecs); !status.ok()) {
      return status;
    }
    return absl::OkStatus();
  }

  template <> inline absl::Status ReadCompact(const Buffer& b, Duration &d) {
    if (absl::Status status = ReadCompact(b, d.secs); !status.ok()) {
      return status;
    }
    if (absl::Status status = ReadCompact(b, d.nsecs); !status.ok()) {
      return status;
    }
    return absl::OkStatus();
  }

  template <> inline absl::Status ExpandField(const Buffer& b, Buffer &dest, const Duration&) {
    if (absl::Status status = ExpandField(b, dest, uint32_t{}); !status.ok()) {
      return status;
    }
    if (absl::Status status = ExpandField(b, dest, uint32_t{}); !status.ok()) {
      return status;
    }
    return absl::OkStatus();
  }

  template <> inline absl::Status CompactField(const Buffer& b, Buffer &dest, const Duration&) {
    if (absl::Status status = CompactField(b, dest, uint32_t{}); !status.ok()) {
      return status;
    }
    if (absl::Status status = CompactField(b, dest, uint32_t{}); !status.ok()) {
      return status;
    }
    return absl::OkStatus();
  }

} // namespace neutron::serdes
