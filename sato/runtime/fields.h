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
    absl::Status WriteROS(ROSBuffer &buffer) { return Write(buffer, value_); } \
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
    absl::Status ParseROS(ROSBuffer &buffer) { return Read(buffer, value_); }  \
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

#if 0
template <typename Enum = int>
class EnumField : public Field {
public:
  using T = typename std::underlying_type<Enum>::type;
  EnumField() = default;
  explicit EnumField(int number)
      : Field(number) {}

  size_t SerializedSize() const {
    return ProtoBuffer::TagSize(Number(), WireType::kVarint) +
           ProtoBuffer::VarintSize<int32_t, false>(
               static_cast<int32_t>(GetUnderlying()));
  }

  absl::Status Serialize(ProtoBuffer &buffer) const {
    return buffer.SerializeVarint<int32_t, false>(
        Number(), static_cast<int32_t>(GetUnderlying()));
  }

  absl::Status Deserialize(ProtoBuffer &buffer) {
    absl::StatusOr<T> v = buffer.DeserializeVarint<T, false>();
    if (!v.ok()) {
      return v.status();
    }
    Set(*v);
    return absl::OkStatus();
  }

private:
 };

// String field with an offset inline in the message.
class StringField : public Field {
public:
  StringField() = default;
  explicit StringField(int number)
      : Field(number) {}

  size_t SerializedSize() const {
    size_t s = size();
    return ProtoBuffer::LengthDelimitedSize(Number(), s);
  }

  absl::Status Serialize(ProtoBuffer &buffer) const {
    size_t s = size();
    return buffer.ProtoBuffer::SerializeLengthDelimited(Number(), data(), s);
  }

  absl::Status Deserialize(ProtoBuffer &buffer) {
    absl::StatusOr<std::string_view> s = buffer.DeserializeString();
    if (!s.ok()) {
      return s.status();
    }
    ::toolbelt::PayloadBuffer::SetString(
        GetBufferAddr(), *s, GetMessageBinaryStart() + relative_binary_offset_);
    return absl::OkStatus();
  }

private:
  template <int N> friend class StringArrayField;
};


template <typename MessageType> class IndirectMessageField : public Field {
public:
  IndirectMessageField() = default;
  explicit IndirectMessageField(
                                int number)
      : Field(number)
        msg_(InternalDefault{}) {}

  const MessageType &Msg() const { return msg_; }
  MessageType &MutableMsg() { return msg_; }


  size_t SerializedSize() const {
    int32_t offset = FindFieldOffset(source_offset_);
    if (offset < 0) {
      return 0;
    }
    ::toolbelt::BufferOffset *addr = GetIndirectAddress(offset);
    if (*addr != 0) {
      // Load up the message if it's already been allocated.
      msg_.runtime = GetRuntime();
      msg_.absolute_binary_offset = *addr;
    }
    return ProtoBuffer::LengthDelimitedSize(Number(), msg_.SerializedSize());
  }

  absl::Status Serialize(ProtoBuffer &buffer) const {
    int32_t offset = FindFieldOffset(source_offset_);
    if (offset < 0) {
      return absl::OkStatus();
    }
    ::toolbelt::BufferOffset *addr = GetIndirectAddress(offset);
    if (*addr != 0) {
      // Load up the message if it's already been allocated.
      msg_.runtime = GetRuntime();
      msg_.absolute_binary_offset = *addr;
    }

    size_t size = msg_.SerializedSize();
    if (absl::Status status =
            buffer.SerializeLengthDelimitedHeader(Number(), size);
        !status.ok()) {
      return status;
    }

    return msg_.Serialize(buffer);
  }

  absl::Status Deserialize(ProtoBuffer &buffer) {
    absl::StatusOr<absl::Span<char>> s = buffer.DeserializeLengthDelimited();
    if (!s.ok()) {
      return s.status();
    }
    // Allocate a new message.
    void *msg_addr = ::toolbelt::PayloadBuffer::Allocate(
        GetBufferAddr(), MessageType::BinarySize(), 8);
    ::toolbelt::BufferOffset msg_offset = GetRuntime()->ToOffset(msg_addr);
    // Assign to the message.
    msg_.runtime = GetRuntime();
    msg_.absolute_binary_offset = msg_offset;

    // Buffer might have moved, get address of indirect again.
    ::toolbelt::BufferOffset *addr =
        GetIndirectAddress(relative_binary_offset_);
    *addr = msg_offset; // Put message field offset into message.

    // Install the metadata into the binary message.
    msg_.template InstallMetadata<MessageType>();

    ProtoBuffer sub_buffer(s.value());
    return msg_.Deserialize(sub_buffer);
  }

protected:
  mutable MessageType msg_;
};
#endif

} // namespace sato
