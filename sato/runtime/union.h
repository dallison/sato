// Copyright 2024 David Allison
// All Rights Reserved
// See LICENSE file for licensing information.

#pragma once

// Union fields.
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "sato/runtime/iterators.h"
#include "sato/runtime/message.h"
#include "toolbelt/payload_buffer.h"
#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace sato {

class UnionMemberField {
protected:
  ::toolbelt::PayloadBuffer *
  GetBuffer(const std::shared_ptr<MessageRuntime> &runtime) const {
    return runtime->pb;
  }

  ::toolbelt::PayloadBuffer **
  GetBufferAddr(const std::shared_ptr<MessageRuntime> &runtime) const {
    return &runtime->pb;
  }
};

#define DEFINE_PRIMITIVE_UNION_FIELD(cname, type)                              \
  template <bool FixedSize = false, bool Signed = false>                       \
  class Union##cname##Field : public UnionMemberField {                        \
  public:                                                                      \
    Union##cname##Field() = default;                                           \
  
    size_t SerializedSize(int number,                                          \
                          const std::shared_ptr<MessageRuntime> &runtime,      \
                          uint32_t abs_offset) const {                         \
      if constexpr (FixedSize) {                                               \
        return ProtoBuffer::TagSize(number,                                    \
                                    ProtoBuffer::FixedWireType<type>()) +      \
               sizeof(type);                                                   \
      } else {                                                                 \
        return ProtoBuffer::TagSize(number, WireType::kVarint) +               \
               ProtoBuffer::VarintSize<type, Signed>(                          \
                   Get(runtime, abs_offset));                                  \
      }                                                                        \
    }                                                                          \
    absl::Status Serialize(int number, ProtoBuffer &buffer,                    \
                           const std::shared_ptr<MessageRuntime> &runtime,     \
                           uint32_t abs_offset) const {                        \
      if constexpr (FixedSize) {                                               \
        return buffer.SerializeFixed<type>(number, Get(runtime, abs_offset));  \
      } else {                                                                 \
        return buffer.SerializeVarint<type, Signed>(number,                    \
                                                    Get(runtime, abs_offset)); \
      }                                                                        \
    }                                                                          \
    absl::Status Deserialize(ProtoBuffer &buffer,                              \
                             const std::shared_ptr<MessageRuntime> &runtime,   \
                             uint32_t abs_offset) {                            \
      absl::StatusOr<type> v;                                                  \
      if constexpr (FixedSize) {                                               \
        v = buffer.DeserializeFixed<type>();                                   \
      } else {                                                                 \
        v = buffer.DeserializeVarint<type, Signed>();                          \
      }                                                                        \
      if (!v.ok()) {                                                           \
        return v.status();                                                     \
      }                                                                        \
      Set(*v, runtime, abs_offset);                                            \
      return absl::OkStatus();                                                 \
    }                                                                          \
    constexpr WireType GetWireType() {                                         \
      if constexpr (FixedSize) {                                               \
        return WireType::kFixed64;                                             \
      } else {                                                                 \
        return WireType::kVarint;                                              \
      }                                                                        \
    }                                                                          \
  };

DEFINE_PRIMITIVE_UNION_FIELD(Int32, int32_t)
DEFINE_PRIMITIVE_UNION_FIELD(Uint32, uint32_t)
DEFINE_PRIMITIVE_UNION_FIELD(Int64, int64_t)
DEFINE_PRIMITIVE_UNION_FIELD(Uint64, uint64_t)
DEFINE_PRIMITIVE_UNION_FIELD(Double, double)
DEFINE_PRIMITIVE_UNION_FIELD(Float, float)
DEFINE_PRIMITIVE_UNION_FIELD(Bool, bool)

#undef DEFINE_PRIMITIVE_UNION_FIELD

template <typename Enum = int>
class UnionEnumField : public UnionMemberField {
public:
  using T = typename std::underlying_type<Enum>::type;
  UnionEnumField() = default;

  size_t SerializedSize(int number,
                        const std::shared_ptr<MessageRuntime> &runtime,
                        uint32_t abs_offset) const {
    return ProtoBuffer::TagSize(number, WireType::kVarint) +
           ProtoBuffer::VarintSize<T, false>(
               GetUnderlying(runtime, abs_offset));
  }

  absl::Status Serialize(int number, ProtoBuffer &buffer,
                         const std::shared_ptr<MessageRuntime> &runtime,
                         uint32_t abs_offset) const {
    return buffer.SerializeVarint<T, false>(number,
                                            GetUnderlying(runtime, abs_offset));
  }

  absl::Status Deserialize(ProtoBuffer &buffer,
                           const std::shared_ptr<MessageRuntime> &runtime,
                           uint32_t abs_offset) {
    absl::StatusOr<T> v = buffer.DeserializeVarint<T, false>();
    if (!v.ok()) {
      return v.status();
    }
    Set(static_cast<Enum>(*v), runtime, abs_offset);
    return absl::OkStatus();
  }
  constexpr WireType GetWireType() { return WireType::kVarint; }
};

// The union contains an offset to the string data (length and bytes).
class UnionStringField : public UnionMemberField {
public:
  UnionStringField() = default;

  size_t SerializedSize(int number,
                        const std::shared_ptr<MessageRuntime> &runtime,
                        uint32_t abs_offset) const {
    size_t sz = size(runtime, abs_offset);
    return ProtoBuffer::TagSize(number, WireType::kLengthDelimited) +
           ProtoBuffer::VarintSize<int32_t, false>(sz) + sz;
  }

  absl::Status Serialize(int number, ProtoBuffer &buffer,
                         const std::shared_ptr<MessageRuntime> &runtime,
                         uint32_t abs_offset) const {
    return buffer.SerializeLengthDelimited(number, data(runtime, abs_offset),
                                           size(runtime, abs_offset));
  }

  absl::Status Deserialize(ProtoBuffer &buffer,
                           const std::shared_ptr<MessageRuntime> &runtime,
                           uint32_t abs_offset) {
    absl::StatusOr<std::string_view> v = buffer.DeserializeString();
    if (!v.ok()) {
      return v.status();
    }
    ::toolbelt::PayloadBuffer::SetString(GetBufferAddr(runtime), *v,
                                         abs_offset);
    return absl::OkStatus();
  }

  constexpr WireType GetWireType() { return WireType::kLengthDelimited; }
};

template <typename MessageType>
class UnionMessageField : public UnionMemberField {
public:
  UnionMessageField() : msg_(InternalDefault{}) {}


  absl::Status SerializeToBuffer(ProtoBuffer &buffer) const {
    return msg_.SerializeToBuffer(buffer);
  }

  absl::Status DeserializeFromBuffer(ProtoBuffer &buffer) {
    return msg_.DeserializeFromBuffer(buffer);
  }

  size_t SerializedSize(int number) const {
    return ProtoBuffer::LengthDelimitedSize(
        number, Get(runtime, abs_offset).SerializedSize());
  }

  absl::Status Serialize(int number, ProtoBuffer &buffer) const {
    if (absl::Status status = buffer.SerializeLengthDelimitedHeader(
            number, Get(runtime, abs_offset).SerializedSize());
        !status.ok()) {
      return status;
    }
    return Get(runtime, abs_offset).Serialize(buffer);
  }

  absl::Status Deserialize(ProtoBuffer &buffer) {
    absl::StatusOr<absl::Span<char>> s = buffer.DeserializeLengthDelimited();
    if (!s.ok()) {
      return s.status();
    }
    void *msg_addr = ::toolbelt::PayloadBuffer::Allocate(
        GetBufferAddr(runtime), MessageType::BinarySize(), 8);
    ::toolbelt::BufferOffset msg_offset =
        GetBuffer(runtime)->ToOffset(msg_addr);
    // Assign to the message.
    msg_.runtime = runtime;
    msg_.absolute_binary_offset = msg_offset;
    msg_.template InstallMetadata<MessageType>();

    // Buffer might have moved, get address of indirect again.
    ::toolbelt::BufferOffset *addr = GetIndirectAddress(runtime, abs_offset);
    if (addr == nullptr) {
      return absl::OkStatus();
    }
    *addr = msg_offset; // Put message field offset into message.
    ProtoBuffer sub_buffer(s.value());
    return msg_.Deserialize(sub_buffer);
  }

private:
  mutable MessageType msg_;
}; // namespace sato

// All member of the tuple must be union fields.  These are stored in a
// std::tuple which does not store them inline so they need to contain the
// buffer shared pointer and the offset of the message binary data.
template <typename... T> class UnionField : public Field {
public:
  UnionField() = default;
  UnionField(
             int number, std::vector<int> field_numbers)
      : Field(number),
        field_numbers_(field_numbers) {}

  bool Is(int number) const { return Discriminator() == number; }

  int32_t Discriminator() const {
    return 0;     // TODO: implementq
  }


  template <int Id> size_t SerializedSize(int discriminator) const {
    int32_t relative_offset = Message::GetMessage(this, source_offset_)
                                  ->FindFieldOffset(field_numbers_[Id]);
    if (relative_offset < 0) { // Field not present.
      return 0;
    }
    return std::get<Id>(value_).SerializedSize(discriminator, GetRuntime(),
                                               GetMessageBinaryStart() +
                                                   relative_offset + 4);
  }

  template <int Id>
  absl::Status Serialize(int discriminator, ProtoBuffer &buffer) const {
    int32_t relative_offset = Message::GetMessage(this, source_offset_)
                                  ->FindFieldOffset(field_numbers_[Id]);
    if (relative_offset < 0) { // Field not present.
      return absl::OkStatus();
    }
    return std::get<Id>(value_).Serialize(discriminator, buffer, GetRuntime(),
                                          GetMessageBinaryStart() +
                                              relative_offset + 4);
  }

  template <int Id>
  absl::Status Deserialize(int discriminator, ProtoBuffer &buffer) {
    int32_t relative_offset = Message::GetMessage(this, source_offset_)
                                  ->FindFieldOffset(field_numbers_[Id]);
    if (relative_offset < 0) { // Field not present.
      return absl::OkStatus();
    }
    if (absl::Status status = std::get<Id>(value_).Deserialize(
            buffer, GetRuntime(),
            GetMessageBinaryStart() + relative_offset + 4);
        !status.ok()) {
      return status;
    }
    // Set the discriminator.
    int32_t *discrim = GetRuntime()->template ToAddress<int32_t>(
        GetMessageBinaryStart() + relative_binary_offset_);
    *discrim = field_numbers_[Id];
    return absl::OkStatus();
  }

private:
  std::vector<int> field_numbers_; // field number for each tuple type
  mutable std::tuple<T...> value_;
};
} // namespace sato
