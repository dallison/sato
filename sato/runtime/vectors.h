// Copyright 2024 David Allison
// All Rights Reserved
// See LICENSE file for licensing information.

#pragma once

// Vector fields (repeated fields).

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "sato/runtime/protobuf.h"
#include "sato/runtime/ros.h"
#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <string_view>
#include <vector>

namespace sato {

class ProtoBuffer;
class ROSBuffer;

// This is a variable length vector of T.  It looks like a std::vector<T>.
// The binary message contains a toolbelt::VectorHeader at the binary offset.
// This contains the number of elements and the base offset for the data.
template <typename T, bool FixedSize = false, bool Signed = false,
          bool Packed = true>
class PrimitiveVectorField : public Field {
public:
  PrimitiveVectorField() = default;
  explicit PrimitiveVectorField(int number) : Field(number) {}

  size_t SerializedProtoSize() const {
    size_t sz = values_.size();
    if (sz == 0) {
      return 0;
    }
    size_t length = 0;

    // Packed is default in proto3 but optional in proto2.
    if constexpr (Packed) {
      if constexpr (FixedSize) {
        return ProtoBuffer::LengthDelimitedSize(Number(), sz * sizeof(T));
      } else {
        for (size_t i = 0; i < sz; i++) {
          length += ProtoBuffer::VarintSize<T, Signed>(values_[i]);
        }
        return ProtoBuffer::LengthDelimitedSize(Number(), length);
      }
    }

    // Not packed, just a sequence of individual fields, all with the same
    // tag.
    if constexpr (FixedSize) {
      length += sz * (ProtoBuffer::TagSize(Number(),
                                           ProtoBuffer::FixedWireType<T>()) +
                      sizeof(T));
    } else {
      for (size_t i = 0; i < sz; i++) {
        length += ProtoBuffer::TagSize(Number(), WireType::kVarint) +
                  ProtoBuffer::VarintSize<T, Signed>(values_[i]);
      }
    }

    return ProtoBuffer::LengthDelimitedSize(Number(), length);
  }
  size_t SerializedROSSize() const { return 4 + values_.size() * sizeof(T); }

  absl::Status WriteProto(ProtoBuffer &buffer) const {
    size_t sz = values_.size();
    if (sz == 0) {
      return absl::OkStatus();
    }

    // Packed is default in proto3 but optional in proto2.
    if constexpr (Packed) {
      if constexpr (FixedSize) {
        return buffer.SerializeLengthDelimited(
            Number(), reinterpret_cast<const char *>(values_.data()),
            sz * sizeof(T));
      } else {
        size_t length = 0;
        for (size_t i = 0; i < sz; i++) {
          length += ProtoBuffer::VarintSize<T, Signed>(values_[i]);
        }

        if (absl::Status status =
                buffer.SerializeLengthDelimitedHeader(Number(), length);
            !status.ok()) {
          return status;
        }

        for (size_t i = 0; i < sz; i++) {
          if (absl::Status status =
                  buffer.SerializeRawVarint<T, Signed>(values_[i]);
              !status.ok()) {
            return status;
          }
        }
        return absl::OkStatus();
      }
    }

    // Not packed, just a sequence of individual fields, all with the same
    // tag.
    if constexpr (FixedSize) {
      for (size_t i = 0; i < sz; i++) {
        if (absl::Status status =
                buffer.SerializeFixed<T>(Number(), values_[i]);
            !status.ok()) {
          return status;
        }
      }
    } else {
      for (size_t i = 0; i < sz; i++) {
        if (absl::Status status =
                buffer.SerializeVarint<T, Signed>(Number(), values_[i]);
            !status.ok()) {
          return status;
        }
      }
    }

    return absl::OkStatus();
  }

  absl::Status WriteROS(ROSBuffer &buffer) const {
    return Write(buffer, values_);
  }

  absl::Status ParseProto(ProtoBuffer &buffer) {
    if constexpr (Packed) {
      absl::StatusOr<absl::Span<char>> data =
          buffer.DeserializeLengthDelimited();
      if (!data.ok()) {
        return data.status();
      }
      if constexpr (FixedSize) {
        values_.resize(data->size() / sizeof(T));
        memcpy(values_.data(), data->data(), data->size());
        return absl::OkStatus();
      } else {
        ProtoBuffer sub_buffer(*data);
        while (!sub_buffer.Eof()) {
          absl::StatusOr<T> v = sub_buffer.DeserializeVarint<T, Signed>();
          if (!v.ok()) {
            return v.status();
          }
          values_.push_back(*v);
        }
      }
    } else {
      if constexpr (FixedSize) {
        absl::StatusOr<T> v = buffer.DeserializeFixed<T>();
        if (!v.ok()) {
          return v.status();
        }
        values_.push_back(*v);
      } else {
        absl::StatusOr<T> v = buffer.DeserializeVarint<T, Signed>();
        if (!v.ok()) {
          return v.status();
        }
        values_.push_back(*v);
      }
    }
    return absl::OkStatus();
  }

  absl::Status ParseROS(ROSBuffer &buffer) {
    if (absl::Status status = Read(buffer, values_); !status.ok()) {
      return status;
    }
    present_ = values_.size() > 0;
    return absl::OkStatus();
  }

private:
  std::vector<T> values_;
};
template <typename T> class MessageVectorField : public Field {
public:
  MessageVectorField() = default;
  explicit MessageVectorField(int number) : Field(number) {}

  size_t SerializedProtoSize() const {
    size_t length = 0;
    for (size_t i = 0; i < msgs_.size(); i++) {
      length += sato::ProtoBuffer::LengthDelimitedSize(
          Number(), msgs_[i].SerializedProtoSize());
    }
    return length;
  }
  size_t SerializedROSSize() const { return 4 + msgs_.size() * sizeof(T); }

  absl::Status WriteProto(ProtoBuffer &buffer) const {
    size_t sz = msgs_.size();
    if (sz == 0) {
      return absl::OkStatus();
    }

    for (const auto &msg : msgs_) {
      if (absl::Status status = msg.WriteProto(buffer); !status.ok()) {
        return status;
      }
    }
    return absl::OkStatus();
  }
  absl::Status WriteROS(ROSBuffer &buffer) const {
    if (absl::Status status =
            Write(buffer, static_cast<uint32_t>(msgs_.size()));
        !status.ok()) {
      return status;
    }
    for (const auto &msg : msgs_) {
      if (absl::Status status = msg.WriteROS(buffer); !status.ok()) {
        return status;
      }
    }
    return absl::OkStatus();
  }

  absl::Status ParseProto(ProtoBuffer &buffer) {
    msgs_.push_back(MessageField<T>(Number()));
    if (absl::Status status = msgs_.back().ParseProto(buffer); !status.ok()) {
      return status;
    }
    return absl::OkStatus();
  }

  absl::Status ParseROS(ROSBuffer &buffer) {
    int num_msgs = 0;
    if (absl::Status status = Read(buffer, num_msgs); !status.ok()) {
      return status;
    }
    for (int i = 0; i < num_msgs; i++) {
      msgs_.push_back(MessageField<T>(Number()));
      if (absl::Status status = msgs_.back().ParseROS(buffer); !status.ok()) {
        return status;
      }
    }
    present_ = num_msgs > 0;
    return absl::OkStatus();
  }

private:
  std::vector<MessageField<T>> msgs_;
};

class StringVectorField : public Field {
public:
  StringVectorField() = default;
  explicit StringVectorField(int number) : Field(number) {}

  size_t SerializedProtoSize() const {
    size_t length = 0;
    for (size_t i = 0; i < strings_.size(); i++) {
      length +=
          sato::ProtoBuffer::LengthDelimitedSize(Number(), strings_[i].size());
    }
    return length;
  }

  size_t SerializedROSSize() const {
    size_t length = 0;
    for (size_t i = 0; i < strings_.size(); i++) {
      length += 4 + strings_[i].size();
    }
    return 4 + length;
  }

  absl::Status WriteProto(ProtoBuffer &buffer) const {
    size_t sz = strings_.size();
    if (sz == 0) {
      return absl::OkStatus();
    }

    for (const auto &s : strings_) {
      if (absl::Status status =
              buffer.SerializeLengthDelimited(Number(), s.data(), s.size());
          !status.ok()) {
        return status;
      }
    }
    return absl::OkStatus();
  }

  absl::Status WriteROS(ROSBuffer &buffer) const {
    if (absl::Status status =
            Write(buffer, static_cast<uint32_t>(strings_.size()));
        !status.ok()) {
      return status;
    }
    for (const auto &s : strings_) {
      if (absl::Status status = Write(buffer, s); !status.ok()) {
        return status;
      }
    }
    return absl::OkStatus();
  }

  absl::Status ParseProto(ProtoBuffer &buffer) {
    absl::StatusOr<std::string_view> v = buffer.DeserializeString();
    if (!v.ok()) {
      return v.status();
    }
    strings_.push_back(*v);
    return absl::OkStatus();
  }
  absl::Status ParseROS(ROSBuffer &buffer) { 
    int num_strings = 0;
    if (absl::Status status = Read(buffer, num_strings); !status.ok()) {
      return status;
    }
    for (int i = 0; i < num_strings; i++) {
      std::string_view s;
      if (absl::Status status = Read(buffer, s); !status.ok()) {
        return status;
      }
      strings_.push_back(s);
    }
    present_ = num_strings > 0;
    return absl::OkStatus();
  }

private:
  std::vector<std::string_view> strings_;
};

} // namespace sato
