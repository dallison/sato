// This is heavily based on Phaser (https://github.com/dallison/phaser) and
// Neutron (https://github.com/dallison/neutron).
// Copyright (C) 2025 David Allison.  All Rights Reserved.

#pragma once

// Fields that handle google.protobuf.Any messages.
// These are mesasges that contain two fields:
// 1. A string called 'type_url' that specifies the type of the message
// 2. A bytes field called 'value' that contains a message.
//
// In regular protobuf, the 'value' field is a serialized message, but
// in sato, the value is the binary of the message.
//
#include "sato/runtime/fields.h"
#include "sato/runtime/mux.h"
#include "sato/runtime/protobuf.h"
#include "sato/runtime/ros.h"
#include "toolbelt/hexdump.h"
#include <memory>
#include <stddef.h>
#include <string>
#include <string_view>

namespace sato {

// Hand-coded message class that represents a google.protobuf.Any message.
class AnyMessage : public Message {
public:
  AnyMessage() : type_url_(1) {}

  static std::string Name() { return "Any"; }
  static std::string FullName() { return "google.protobuf.Any"; }
  std::string GetName() const { return Name(); }
  std::string GetFullName() const override { return FullName(); }

  size_t SerializedProtoSize() const {
    size_t size = 0;
    if (type_url_.IsPresent()) {
      size += type_url_.SerializedProtoSize();
    }
    if (value_ != nullptr) {
      // This is the serialized proto message encoded in a string.
      size +=
          ProtoBuffer::LengthDelimitedSize(2, value_->SerializedProtoSize());
    }

    return size;
  }

  size_t SerializedROSSize() const {
    size_t size = type_url_.SerializedROSSize();
    if (value_ != nullptr) {
      size += value_->SerializedROSSize();
    }
    // Value is encoded as a string which has a 4 byte length prefix.
    return 4 + size;
  }

  absl::Status WriteProto(sato::ProtoBuffer &buffer) const {
    if (type_url_.IsPresent()) {
      if (absl::Status status = type_url_.WriteProto(buffer); !status.ok()) {
        return status;
      }

      if (value_ != nullptr) {
        // Need to write the value as a serialized protobuf message encoded in a
        // string.
        sato::ProtoBuffer value_buffer;
        if (absl::Status status = value_->WriteProto(value_buffer);
            !status.ok()) {
          return status;
        }
        std::string_view value_string(value_buffer.data(), value_buffer.size());
        if (absl::Status status = buffer.SerializeLengthDelimited(
                2, value_string.data(), value_string.size());
            !status.ok()) {
          return status;
        }
      }
    }
    return absl::OkStatus();
  }

  absl::Status WriteROS(sato::ROSBuffer &buffer, uint64_t timestamp = 0) const {
    // Always write the type url.  It might be empty but we will write the
    // length.
    if (absl::Status status = type_url_.WriteROS(buffer); !status.ok()) {
      return status;
    }
    sato::ROSBuffer value_buffer;
    if (value_ != nullptr) {
      if (absl::Status status = value_->WriteROS(value_buffer); !status.ok()) {
        return status;
      }
    }
    // Write the value.
    std::string_view value_string(value_buffer.data(), value_buffer.Size());
    if (absl::Status status = Write(buffer, value_string); !status.ok()) {
      return status;
    }
    return absl::OkStatus();
  }

  absl::Status ParseProto(sato::ProtoBuffer &buffer) {
    while (!buffer.Eof()) {
      absl::StatusOr<uint32_t> tag =
          buffer.DeserializeVarint<uint32_t, false>();
      if (!tag.ok()) {
        return tag.status();
      }
      uint32_t field_number = *tag >> sato::ProtoBuffer::kFieldIdShift;
      switch (field_number) {
      case 1:
        if (absl::Status status = type_url_.ParseProto(buffer); !status.ok()) {
          return status;
        }
        break;
      case 2: {
        std::string type = MessageTypeName();
        value_ = MultiplexerCreateMessage(type);
        if (value_ == nullptr) {
          return absl::InternalError(
              absl::StrFormat("Unknown message type: %s", type));
        }
        if (absl::Status status = value_->ParseProto(buffer); !status.ok()) {
          return status;
        }
        break;
      }
      default:
        if (absl::Status status = buffer.SkipTag(*tag); !status.ok()) {
          return status;
        }
      }
    }
    return absl::OkStatus();
  }

  absl::Status ParseROS(sato::ROSBuffer &buffer) {
    if (absl::Status status = type_url_.ParseROS(buffer); !status.ok()) {
      return status;
    }
    if (!type_url_.IsPresent()) {
      // Message type is empty.  We still have the empty value field to parse.
      return buffer.Skip(4);
    }
    std::string type = MessageTypeName();

    value_ = MultiplexerCreateMessage(type);
    if (value_ == nullptr) {
      return absl::InternalError(
          absl::StrFormat("Unknown message type: %s", type));
    }
    uint32_t value_size = 0;
    if (absl::Status status = Read(buffer, value_size); !status.ok()) {
      return status;
    }
    if (value_size > 0) {
      if (absl::Status status = value_->ParseROS(buffer); !status.ok()) {
        return status;
      }
    }

    return absl::OkStatus();
  }

  std::string MessageTypeName() const {
    std::string type = std::string(type_url_.Value());
    size_t pos = type.find('/');
    if (pos != std::string::npos) {
      return type.substr(pos + 1);
    }
    return type;
  }
  bool IsPresent() const { return type_url_.IsPresent(); }

private:
  sato::StringField type_url_;
  std::unique_ptr<sato::Message> value_;
};

class AnyField : public MessageField<AnyMessage> {
public:
  AnyField(int number) : MessageField(number) {}
  bool IsPresent() const { return msg_.IsPresent(); }
};

} // namespace sato
