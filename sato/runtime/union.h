// This is heavily based on Phaser (https://github.com/dallison/phaser) and
// Neutron (https://github.com/dallison/neutron).
// Copyright (C) 2025 David Allison.  All Rights Reserved.

#pragma once

// Union fields.
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "sato/runtime/fields.h"
#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace sato {

// Primitive union fields are just regular primitive fields.
#define DEFINE_PRIMITIVE_UNION_FIELD(cname, type)                              \
  template <bool FixedSize = false, bool Signed = false>                       \
  using Union##cname##Field = cname##Field<FixedSize, Signed>;

DEFINE_PRIMITIVE_UNION_FIELD(Int32, int32_t)
DEFINE_PRIMITIVE_UNION_FIELD(Uint32, uint32_t)
DEFINE_PRIMITIVE_UNION_FIELD(Int64, int64_t)
DEFINE_PRIMITIVE_UNION_FIELD(Uint64, uint64_t)
DEFINE_PRIMITIVE_UNION_FIELD(Double, double)
DEFINE_PRIMITIVE_UNION_FIELD(Float, float)
DEFINE_PRIMITIVE_UNION_FIELD(Bool, bool)

#undef DEFINE_PRIMITIVE_UNION_FIELD

using UnionStringField = StringField;
// The union contains an offset to the string data (length and bytes).

// Messages within unions are fields but they are encoded as an array of
// messages in ROS format so that they can be omitted if they are not present.
template <typename MessageType> class UnionMessageField : public Field {
public:
  UnionMessageField() {}

  size_t SerializedProtoSize() const {
    return ProtoBuffer::LengthDelimitedSize(Number(),
                                            msg_.SerializedProtoSize());
  }

  size_t SerializedROSSize() const {
    size_t size = 4; // 4 bytes for the discriminator
    if (msg_.IsPresent()) {
      size += msg_.SerializedROSSize();
    }
    return size;
  }

  absl::Status WriteProto(ProtoBuffer &buffer) const {
    return msg_.WriteProto(buffer);
  }

  absl::Status WriteROS(ROSBuffer &buffer) const {
    int array_size = msg_.IsPresent() ? 1 : 0;
    if (absl::Status status = Write(buffer, array_size); !status.ok()) {
      return status;
    }
    if (array_size > 0) {
      return msg_.WriteROS(buffer);
    }
    return absl::OkStatus();
  }

  absl::Status ParseProto(ProtoBuffer &buffer) {
    return msg_.ParseProto(buffer);
  }

  absl::Status ParseROS(ROSBuffer &buffer) {
    int array_size = 0;
    if (absl::Status status = Read(buffer, array_size); !status.ok()) {
      return status;
    }
    if (array_size > 0) {
      return msg_.ParseROS(buffer);
    }
    return absl::OkStatus();
  }

private:
  MessageField<MessageType> msg_;
}; // namespace sato

template <typename... T> class UnionField : public Field {
public:
  UnionField() = default;
  UnionField(std::vector<int> field_numbers)
      : Field(0), field_numbers_(field_numbers) {
    size_t i = 0;
    std::apply(
        [&](auto &...args) { (args.SetNumber(field_numbers_[i++]), ...); },
        value_);
  }

  int32_t Discriminator() const { return discriminator_; }

  template <int Id> size_t SerializedProtoSize() const {
    return std::get<Id>(value_).SerializedProtoSize();
  }

  size_t SerializedROSSize() const {
    size_t size = 4; // 4 bytes for the discriminator
    // Iterate through all tuple elements at runtime and call SerializedROSSize
    // on each
    std::apply(
        [&](auto &...args) { size += (args.SerializedROSSize() + ... + 0); },
        value_);
    return size;
  }

  absl::Status WriteDiscriminator(ROSBuffer &buffer) const {
    return Write(buffer, Discriminator());
  }

  template <int Id> absl::Status WriteProto(ProtoBuffer &buffer) const {
    if (std::get<Id>(value_).IsPresent()) {
      return std::get<Id>(value_).WriteProto(buffer);
    }
    return absl::OkStatus();
  }

  template <int Id> absl::Status ParseProto(ProtoBuffer &buffer) {
    if (absl::Status status = std::get<Id>(value_).ParseProto(buffer);
        !status.ok()) {
      return status;
    }
    discriminator_ = field_numbers_[Id];
    return absl::OkStatus();
  }

  absl::Status WriteROS(ROSBuffer &buffer) const {
    // Write the discrimintor and then the contents of the value tuple
    if (absl::Status status = Write(buffer, Discriminator()); !status.ok()) {
      return status;
    }
    // Iterate through all tuple elements at runtime and call WriteROS on each
    absl::Status result = absl::OkStatus();
    std::apply(
        [&](auto &...args) {
          ((result = args.WriteROS(buffer), result.ok()) && ...);
        },
        value_);
    return result;
  }

  absl::Status ParseROS(ROSBuffer &buffer) {
    if (absl::Status status = Read(buffer, discriminator_); !status.ok()) {
      return status;
    }
    // Iterate through all tuple elements at runtime and call ParseROS on each
    absl::Status result = absl::OkStatus();
    std::apply(
        [&](auto &...args) {
          ((result = args.ParseROS(buffer), result.ok()) && ...);
        },
        value_);
    return result;
  }

private:
  std::vector<int> field_numbers_; // field number for each tuple type
  int discriminator_;
  std::tuple<T...> value_;
};
} // namespace sato
