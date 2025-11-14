// Copyright 2024 David Allison
// All Rights Reserved
// See LICENSE file for licensing information.

#pragma once

// Vector fields (repeated fields).

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "sato/runtime/iterators.h"
#include "sato/runtime/message.h"
#include "sato/runtime/protobuf.h"
#include "toolbelt/payload_buffer.h"
#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <string_view>
#include <vector>

namespace sato {

class ProtoBuffer;


// This is a variable length vector of T.  It looks like a std::vector<T>.
// The binary message contains a toolbelt::VectorHeader at the binary offset.
// This contains the number of elements and the base offset for the data.
template <typename T, bool FixedSize = false, bool Signed = false,
          bool Packed = true>
class PrimitiveVectorField : public Field {
public:
  PrimitiveVectorField() = default;
  explicit PrimitiveVectorField(
                                int number)
      : Field(number) {}


  size_t SerializedSize() const {
    size_t sz = size();
    if (sz == 0) {
      return 0;
    }
    size_t length = 0;

    // Packed is default in proto3 but optional in proto2.
    if constexpr (Packed) {
      if constexpr (FixedSize) {
        return ProtoBuffer::LengthDelimitedSize(Number(), sz * sizeof(T));
      } else {
        T *base = GetRuntime()->template ToAddress<T>(BaseOffset());
        if (base == nullptr) {
          return 0;
        }
        for (size_t i = 0; i < sz; i++) {
          length += ProtoBuffer::VarintSize<T, Signed>(base[i]);
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
      T *base = GetRuntime()->template ToAddress<T>(BaseOffset());
      if (base == nullptr) {
        return 0;
      }
      for (size_t i = 0; i < sz; i++) {
        length += ProtoBuffer::TagSize(Number(), WireType::kVarint) +
                  ProtoBuffer::VarintSize<T, Signed>(base[i]);
      }
    }

    return ProtoBuffer::LengthDelimitedSize(Number(), length);
  }

  absl::Status Serialize(ProtoBuffer &buffer) const {
    size_t sz = size();
    if (sz == 0) {
      return absl::OkStatus();
    }

    T *base = GetRuntime()->template ToAddress<T>(BaseOffset());
    if (base == nullptr) {
      return absl::OkStatus();
    }
    // Packed is default in proto3 but optional in proto2.
    if constexpr (Packed) {
      if constexpr (FixedSize) {
        return buffer.SerializeLengthDelimited(
            Number(), reinterpret_cast<const char *>(base), sz * sizeof(T));
      } else {
        size_t length = 0;
        for (size_t i = 0; i < sz; i++) {
          length += ProtoBuffer::VarintSize<T, Signed>(base[i]);
        }

        if (absl::Status status =
                buffer.SerializeLengthDelimitedHeader(Number(), length);
            !status.ok()) {
          return status;
        }

        for (size_t i = 0; i < sz; i++) {
          if (absl::Status status =
                  buffer.SerializeRawVarint<T, Signed>(base[i]);
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
        if (absl::Status status = buffer.SerializeFixed<T>(Number(), base[i]);
            !status.ok()) {
          return status;
        }
      }
    } else {
      for (size_t i = 0; i < sz; i++) {
        if (absl::Status status =
                buffer.SerializeVarint<T, Signed>(Number(), base[i]);
            !status.ok()) {
          return status;
        }
      }
    }

    return absl::OkStatus();
  }

  absl::Status Deserialize(ProtoBuffer &buffer) {
    if constexpr (Packed) {
      absl::StatusOr<absl::Span<char>> data =
          buffer.DeserializeLengthDelimited();
      if (!data.ok()) {
        return data.status();
      }
      if constexpr (FixedSize) {
        resize(data->size() / sizeof(T));
        T *base = GetRuntime()->template ToAddress<T>(BaseOffset());
        memcpy(base, data->data(), data->size());
        return absl::OkStatus();
      } else {
        ProtoBuffer sub_buffer(*data);
        while (!sub_buffer.Eof()) {
          absl::StatusOr<T> v = sub_buffer.DeserializeVarint<T, Signed>();
          if (!v.ok()) {
            return v.status();
          }
          push_back(*v);
        }
      }
    } else {
      if constexpr (FixedSize) {
        absl::StatusOr<T> v = buffer.DeserializeFixed<T>();
        if (!v.ok()) {
          return v.status();
        }
        push_back(*v);
      } else {
        absl::StatusOr<T> v = buffer.DeserializeVarint<T, Signed>();
        if (!v.ok()) {
          return v.status();
        }
        push_back(*v);
      }
    }
    return absl::OkStatus();
  }

private:
  friend FieldIterator<PrimitiveVectorField, T>;
  friend FieldIterator<PrimitiveVectorField, const T>;
};

template <typename Enum = int, bool Packed = true>
class EnumVectorField : public Field {
public:
  EnumVectorField() = default;
  explicit EnumVectorField(int number)
      : Field(number),{}

  using T = typename std::underlying_type<Enum>::type;

  size_t SerializedSize() const {
    size_t sz = size();
    if (sz == 0) {
      return 0;
    }
    size_t length = 0;

    // Packed is default in proto3 but optional in proto2.
    if constexpr (Packed) {
      T *base = GetRuntime()->template ToAddress<T>(BaseOffset());
      if (base == nullptr) {
        return 0;
      }
      for (size_t i = 0; i < sz; i++) {
        length += ProtoBuffer::VarintSize<T, false>(base[i]);
      }
      return ProtoBuffer::LengthDelimitedSize(Number(), length);
    }

    // Not packed, just a sequence of individual fields, all with the same
    // tag.
    T *base = GetRuntime()->template ToAddress<T>(BaseOffset());
    if (base == nullptr) {
      return 0;
    }
    for (size_t i = 0; i < sz; i++) {
      length += ProtoBuffer::TagSize(Number(), WireType::kVarint) +
                ProtoBuffer::VarintSize<T, false>(base[i]);
    }

    return ProtoBuffer::LengthDelimitedSize(Number(), length);
  }

  absl::Status Serialize(ProtoBuffer &buffer) const {
    size_t sz = size();
    if (sz == 0) {
      return absl::OkStatus();
    }

    T *base = GetRuntime()->template ToAddress<T>(BaseOffset());
    if (base == nullptr) {
      return absl::OkStatus();
    }
    // Packed is default in proto3 but optional in proto2.
    if constexpr (Packed) {
      size_t length = 0;
      for (size_t i = 0; i < sz; i++) {
        length += ProtoBuffer::VarintSize<T, false>(base[i]);
      }

      if (absl::Status status =
              buffer.SerializeLengthDelimitedHeader(Number(), length);
          !status.ok()) {
        return status;
      }

      for (size_t i = 0; i < sz; i++) {
        if (absl::Status status = buffer.SerializeRawVarint<T, false>(base[i]);
            !status.ok()) {
          return status;
        }
      }
      return absl::OkStatus();
    }

    // Not packed, just a sequence of individual fields, all with the same
    // tag.

    for (size_t i = 0; i < sz; i++) {
      if (absl::Status status =
              buffer.SerializeVarint<T, false>(Number(), base[i]);
          !status.ok()) {
        return status;
      }
    }

    return absl::OkStatus();
  }

  absl::Status Deserialize(ProtoBuffer &buffer) {
    if constexpr (Packed) {
      absl::StatusOr<absl::Span<char>> data =
          buffer.DeserializeLengthDelimited();
      if (!data.ok()) {
        return data.status();
      }
      ProtoBuffer sub_buffer(*data);
      while (!sub_buffer.Eof()) {
        absl::StatusOr<T> v = sub_buffer.DeserializeVarint<T, false>();
        if (!v.ok()) {
          return v.status();
        }
        push_back(static_cast<Enum>(*v));
      }
    } else {
      absl::StatusOr<T> v = buffer.DeserializeVarint<T, false>();
      if (!v.ok()) {
        return v.status();
      }
      push_back(static_cast<Enum>(*v));
    }
    return absl::OkStatus();
  }

private:
};

template <typename T> class MessageVectorField : public Field {
public:
  MessageVectorField() = default;
  explicit MessageVectorField(
                              int number)
      : Field(number){}


  size_t SerializedSize() const {
    Populate();
    size_t length = 0;
    for (size_t i = 0; i < size(); i++) {
      length += sato::ProtoBuffer::LengthDelimitedSize(
          Number(), msgs_[i].SerializedSize());
    }
    return length;
  }

  absl::Status Serialize(ProtoBuffer &buffer) const {
    Populate();
    size_t sz = size();
    if (sz == 0) {
      return absl::OkStatus();
    }

    for (const auto &msg : msgs_) {
      if (absl::Status status = buffer.SerializeLengthDelimitedHeader(
              Number(), msg.SerializedSize());
          !status.ok()) {
        return status;
      }
      if (absl::Status status = msg.Serialize(buffer); !status.ok()) {
        return status;
      }
    }
    return absl::OkStatus();
  }

  absl::Status Deserialize(ProtoBuffer &buffer) {
    absl::StatusOr<absl::Span<char>> v = buffer.DeserializeLengthDelimited();
    if (!v.ok()) {
      return v.status();
    }
    ProtoBuffer msg_buffer(*v);
    T *msg = Add();
    if (absl::Status status = msg->Deserialize(msg_buffer); !status.ok()) {
      return status;
    }
    return absl::OkStatus();
  }

private:
  mutable std::vector<MessageObject<T>> msgs_;
  MessageObject<T> empty_;
};

class StringVectorField : public Field {
public:
  StringVectorField() = default;
  explicit StringVectorField(
                             int number)
      : Field(number) {}

  size_t SerializedSize() const {
    Populate();
    size_t length = 0;
    for (size_t i = 0; i < size(); i++) {
      length += sato::ProtoBuffer::LengthDelimitedSize(
          Number(), strings_[i].SerializedSize());
    }
    return length;
  }

  absl::Status Serialize(ProtoBuffer &buffer) const {
    Populate();
    size_t sz = size();
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

  absl::Status Deserialize(ProtoBuffer &buffer) {
    absl::StatusOr<std::string_view> v = buffer.DeserializeString();
    if (!v.ok()) {
      return v.status();
    }
    push_back(*v);
    return absl::OkStatus();
  }

private:
  mutable std::vector<NonEmbeddedStringField> strings_;
  NonEmbeddedStringField empty_;
};


} // namespace sato
