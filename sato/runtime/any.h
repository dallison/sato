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
# #include "sato/runtime/sato_bank.h"
#include "toolbelt/hexdump.h"
#include <stddef.h>

namespace sato {

// Hand-coded message class that represents a google.protobuf.Any message.
class AnyMessage : public Message {
public:
  AnyMessage()
      : type_url_(1),
        value_(2) {}

 {}

  static std::string Name() { return "Any"; }
  static std::string FullName() { return "google.protobuf.Any"; }
  std::string GetName() const override { return Name(); }
  std::string GetFullName() const override { return FullName(); }

  size_t SerializedProtoSize() const {
    size_t size = 0;
    if (type_url_.IsPresent()) {
      size += type_url_.SerializedProtoSize();
    }
    if (value_.IsPresent()) {
      absl::StatusOr<size_t> value_size =
          PhaserBankSerializedSize(std::string(type_url()), As<Message>());
      if (!value_size.ok()) {
        return 0;
      }
      size += *value_size;
    }
    return size;
  }

  absl::Status Serialize(sato::ProtoBuffer &buffer) const {
    if (type_url_.IsPresent()) {
      if (absl::Status status = type_url_.Serialize(buffer); !status.ok()) {
        return status;
      }
    }
    if (value_.IsPresent()) {
      if (absl::Status status = PhaserBankSerializeToBuffer(
              std::string(type_url()), As<Message>(), buffer);
          !status.ok()) {
        return status;
      }
    }
    return absl::OkStatus();
  }

  absl::Status Deserialize(sato::ProtoBuffer &buffer) {
    while (!buffer.Eof()) {
      absl::StatusOr<uint32_t> tag =
          buffer.DeserializeVarint<uint32_t, false>();
      if (!tag.ok()) {
        return tag.status();
      }
      uint32_t field_number = *tag >> sato::ProtoBuffer::kFieldIdShift;
      switch (field_number) {
      case 1:
        if (absl::Status status = type_url_.Deserialize(buffer); !status.ok()) {
          return status;
        }
        break;
      case 2: {
        std::string type = MessageTypeName();

        absl::StatusOr<Message *> d = BuildMessageInstanceInValue();
        if (!d.ok()) {
          return d.status();
        }

        std::unique_ptr<Message> msg(*d);
        if (absl::Status status =
                PhaserBankDeserializeFromBuffer(type, *msg, buffer);
            !status.ok()) {
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

  std::string MessageTypeName() const {
    std::string type = std::string(type_url());
    size_t pos = type.find('/');
    if (pos != std::string::npos) {
      return type.substr(pos + 1);
    }
    return type;
  }

  absl::Status CloneFrom(const AnyMessage &msg) {
    if (msg.has_type_url()) {
      type_url_.Set(msg.type_url());
    }
    if (msg.has_value()) {
      std::string type = MessageTypeName();
      absl::StatusOr<Message *> d = BuildMessageInstanceInValue();
      if (!d.ok()) {
        return d.status();
      }

      std::unique_ptr<Message> dest(*d);
      absl::StatusOr<const Message *> s =
          PhaserBankMakeExisting(type, msg.runtime, msg.value().data());
      if (!s.ok()) {
        return s.status();
      }
      std::unique_ptr<const Message> src(*s);
      if (absl::Status status = PhaserBankCopy(type, *src, *dest);
          !status.ok()) {
        return status;
      }
    }
    return absl::OkStatus();
  }

  void CopyFrom(const Message &m) override {
    const AnyMessage &msg = static_cast<const AnyMessage &>(m);
    (void)CloneFrom(msg);
  }

  // Create an instance of message T in the value field.  Returns
  // a message whose storage is in the payload buffer inside
  // the value.
  template <typename T> T MutableAny() {
    set_type_url("type.googleapis.com/" + T::FullName());
    size_t size = T::BinarySize();
    absl::Span<char> memory = value_.Allocate(size, true);
    auto msg = T(runtime, runtime->ToOffset(memory.data()));
    msg.template InstallMetadata<T>();
    return msg;
  }

  // In sato the embedded message isn't serialized so this is just a
  // copy from msg into the value field.
  template <typename T> bool PackFrom(const T &msg) {
    T m = MutableAny<T>();
    return m.CloneFrom(msg).ok();
  }

  template <typename T> absl::Status PackFromOrStatus(const T &msg) {
    T m = MutableAny<T>();
    return m.CloneFrom(msg);
  }

  template <typename T> bool UnpackTo(T *msg) const {
    const char *addr = value().data();
    std::unique_ptr<T> embedded_msg = std::make_unique<T>(
        runtime, runtime->ToOffset(const_cast<char *>(addr)));
    return msg->CloneFrom(*embedded_msg).ok();
  }

  template <typename T> absl::Status UnpackToOrStatus(T *msg) const {
    const char *addr = value().data();
    std::unique_ptr<T> embedded_msg = std::make_unique<T>(
        runtime, runtime->ToOffset(const_cast<char *>(addr)));
    return msg->CloneFrom(*embedded_msg);
  }

  template <typename T> bool Is() const {
    const std::string &msg_type = T::FullName();
    std::string t = MessageTypeName();
    return t == msg_type;
  }

  // Gets the value of the any field as a message of type T.  This does not
  // check that the message is actually of type T, so it's up to you to call
  // the Is<T>() method first.  Caveat programmer.
  template <typename T> const T As() const {
    const T msg(runtime, runtime->ToOffset(const_cast<char *>(value().data())));
    return msg;
  }

private:
  absl::StatusOr<Message *> BuildMessageInstanceInValue() {
    std::string type = MessageTypeName();
    // Allocate space in this payload buffer with space for the string
    // length. The message data will be stored in a string field, which
    // needs the length before the data.
    absl::StatusOr<size_t> binary_size = PhaserBankBinarySize(type);
    if (!binary_size.ok()) {
      return binary_size.status();
    }
    char *memory = reinterpret_cast<char *>(::toolbelt::PayloadBuffer::Allocate(
        &runtime->pb, *binary_size + 4, 4, true));
    // Place the message after the string length field.
    absl::StatusOr<Message *> d = PhaserBankAllocateAtOffset(
        type, runtime, runtime->ToOffset(memory + 4));
    if (!d.ok()) {
      return d.status();
    }

    // Write the length of the string into the first 4 bytes of the
    // allocatedmemory
    uint32_t *string_length = reinterpret_cast<uint32_t *>(memory);
    *string_length = *binary_size;
    // Set the string indirect field to the address of the string length
    // field.
    value_.SetNoCopy(memory);

    return d;
  }

  sato::StringField type_url_;
  sato::StringField value_;
};

class AnyField : public MessageField<AnyMessage> {
public:
  AnyField(int number)
    : MessageField( number) {}

};

} // namespace sato
