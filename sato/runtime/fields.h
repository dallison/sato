// Copyright 2024 David Allison
// All Rights Reserved
// See LICENSE file for licensing information.

#pragma once

// Single value fields.

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

template <typename T> constexpr size_t AlignedOffset(size_t offset) {
  return (offset + sizeof(T) - 1) & ~(sizeof(T) - 1);
}

class Field {
public:
  Field() = default;
  Field(int number) : number_(number) {}
  virtual ~Field() = default;

  int Number() const { return number_; }
  bool IsPresent() const { return present_; }

protected:
  int number_ = 0;
  bool present_ = false;
};

#define DEFINE_PRIMITIVE_FIELD(cname, type)                                    \
  template <bool FixedSize = false, bool Signed = false>                       \
  class cname##Field : public Field {                                          \
  public:                                                                      \
    cname##Field() = default;                                                  \
    explicit cname##Field(int number) : Field(number) {}                       \
                                                                               \
    size_t SerializedProtoSize() const {                                       \
      if constexpr (FixedSize) {                                               \
        return ProtoBuffer::TagSize(Number(),                                  \
                                    ProtoBuffer::FixedWireType<type>()) +      \
               sizeof(type);                                                   \
      } else {                                                                 \
        return ProtoBuffer::TagSize(Number(), WireType::kVarint) +             \
               ProtoBuffer::VarintSize<type, Signed>(value_);                  \
      }                                                                        \
    }                                                                          \
                                                                               \
    absl::Status WriteProto(ProtoBuffer &buffer) const {                       \
      if constexpr (FixedSize) {                                               \
        return buffer.SerializeFixed<type>(Number(), value_);                  \
      } else {                                                                 \
        return buffer.SerializeVarint<type, Signed>(Number(), value_);         \
      }                                                                        \
    }                                                                          \
    absl::Status WriteROS(ROSBuffer &buffer) const { return Write(buffer, value_); } \
                                                                               \
    absl::Status ParseProto(ProtoBuffer &buffer) {                             \
      absl::StatusOr<type> v;                                                  \
      if constexpr (FixedSize) {                                               \
        v = buffer.DeserializeFixed<type>();                                   \
      } else {                                                                 \
        v = buffer.DeserializeVarint<type, Signed>();                          \
      }                                                                        \
      if (!v.ok()) {                                                           \
        return v.status();                                                     \
      }                                                                        \
      value_ = *v;                                                             \
      present_ = true;                                                         \
      return absl::OkStatus();                                                 \
    }                                                                          \
    absl::Status ParseROS(ROSBuffer &buffer) {                                 \
      if (absl::Status status = Read(buffer, value_); !status.ok()) {          \
        return status;                                                         \
      }                                                                        \
      present_ = value_ != 0;                                                  \
      return absl::OkStatus();                                                 \
    }                                                                          \
    size_t SerializedROSSize() const { return sizeof(type); }                  \
                                                                               \
  private:                                                                     \
    type value_ = {};                                                          \
  };

DEFINE_PRIMITIVE_FIELD(Int32, int32_t)
DEFINE_PRIMITIVE_FIELD(Uint32, uint32_t)
DEFINE_PRIMITIVE_FIELD(Int64, int64_t)
DEFINE_PRIMITIVE_FIELD(Uint64, uint64_t)
DEFINE_PRIMITIVE_FIELD(Double, double)
DEFINE_PRIMITIVE_FIELD(Float, float)
DEFINE_PRIMITIVE_FIELD(Bool, bool)

#undef DEFINE_PRIMITIVE_FIELD

// String field with an offset inline in the message.
class StringField : public Field {
public:
  StringField() = default;
  explicit StringField(int number) : Field(number) {}

  size_t SerializedProtoSize() const {
    size_t s = value_.size();
    return ProtoBuffer::LengthDelimitedSize(Number(), s);
  }
  size_t SerializedROSSize() const { return 4 + value_.size(); }

  absl::Status WriteProto(ProtoBuffer &buffer) const {
    size_t s = value_.size();
    return buffer.ProtoBuffer::SerializeLengthDelimited(Number(), value_.data(),
                                                        s);
  }
  absl::Status WriteROS(ROSBuffer &buffer) const { return Write(buffer, value_); }

  absl::Status ParseProto(ProtoBuffer &buffer) {
    absl::StatusOr<std::string_view> s = buffer.DeserializeString();
    if (!s.ok()) {
      return s.status();
    }
    value_ = *s;
    present_ = true;
    return absl::OkStatus();
  }

  absl::Status ParseROS(ROSBuffer &buffer) { 
    if (absl::Status status = Read(buffer, value_); !status.ok()) {
      return status;
    }
    present_ = value_.size() > 0;
    std::cerr << "StringField: " << value_ << std::endl;
    return absl::OkStatus();
  }

private:
  std::string_view value_ = {}; // No copy made for this.
};

template <typename MessageType> class MessageField : public Field {
public:
  MessageField() = default;
  explicit MessageField(int number) : Field(number) {}

  size_t SerializedProtoSize() const {
    return ProtoBuffer::LengthDelimitedSize(Number(),
                                            msg_.SerializedProtoSize());
  }

  size_t SerializedROSSize() const { return msg_.SerializedROSSize(); }

  absl::Status WriteProto(ProtoBuffer &buffer) const {
    size_t size = msg_.SerializedProtoSize();
    if (absl::Status status =
            buffer.SerializeLengthDelimitedHeader(Number(), size);
        !status.ok()) {
      return status;
    }

    return msg_.WriteProto(buffer);
  }

  absl::Status WriteROS(ROSBuffer &buffer) const { return msg_.WriteROS(buffer); }

  absl::Status ParseProto(ProtoBuffer &buffer) {
    absl::StatusOr<absl::Span<char>> s = buffer.DeserializeLengthDelimited();
    if (!s.ok()) {
      return s.status();
    }
    ProtoBuffer sub_buffer(s.value());
    return msg_.ParseProto(sub_buffer);
  }

  absl::Status ParseROS(ROSBuffer &buffer) { 
    if (absl::Status status = msg_.ParseROS(buffer); !status.ok()) {
      return status;
    }
    present_ = true;
    return absl::OkStatus();
  }

protected:
  MessageType msg_;
};

} // namespace sato
