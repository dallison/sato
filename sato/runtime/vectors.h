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

  absl::Status WriteROS(ROSBuffer &buffer) { return Write(buffer, values_); }

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

  absl::Status ParseROS(ROSBuffer &buffer) { return Read(buffer, values_); }

private:
  std::vector<T> values_;
};

#if 0
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
#endif

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
      if (absl::Status status = buffer.SerializeLengthDelimitedHeader(
              Number(), msg.SerializedProtoSize());
          !status.ok()) {
        return status;
      }
      if (absl::Status status = msg.WriteProto(buffer); !status.ok()) {
        return status;
      }
    }
    return absl::OkStatus();
  }
  absl::Status WriteROS(ROSBuffer &buffer) { return Write(buffer, msgs_); }

  absl::Status ParseProto(ProtoBuffer &buffer) {
    absl::StatusOr<absl::Span<char>> v = buffer.DeserializeLengthDelimited();
    if (!v.ok()) {
      return v.status();
    }
    ProtoBuffer msg_buffer(*v);
    msgs_.push_back(MessageField<T>());
    if (absl::Status status = msgs_.back().ParseProto(msg_buffer); !status.ok()) {
      return status;
    }
    return absl::OkStatus();
  }
  absl::Status ParseROS(ROSBuffer &buffer) { return Read(buffer, msgs_); }

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
  absl::Status WriteROS(ROSBuffer &buffer) { return Write(buffer, strings_); }

  absl::Status ParseProto(ProtoBuffer &buffer) {
    absl::StatusOr<std::string_view> v = buffer.DeserializeString();
    if (!v.ok()) {
      return v.status();
    }
    strings_.push_back(*v);
    return absl::OkStatus();
  }
  absl::Status ParseROS(ROSBuffer &buffer) { return Read(buffer, strings_); }

private:
  std::vector<std::string_view> strings_;
};

} // namespace sato
