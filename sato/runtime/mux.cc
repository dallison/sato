// This is heavily based on Phaser (https://github.com/dallison/phaser) and
// Neutron (https://github.com/dallison/neutron).
// Copyright (C) 2025 David Allison.  All Rights Reserved.

#include "sato/runtime/mux.h"
#include "absl/strings/str_format.h"
#include <memory>

namespace sato {

std::unique_ptr<absl::flat_hash_map<std::string, MultiplexerInfo>> sato_multiplexers;

absl::StatusOr<MultiplexerInfo *> GetMultiplexerInfo(std::string message_type) {
  auto it = sato_multiplexers->find(message_type);
  if (it == sato_multiplexers->end()) {
    return absl::InternalError(
        absl::StrFormat("Unknown sato message type '%s'", message_type));
  }
  return &it->second;
}

void MultiplexerRegisterMessage(const std::string &name, const MultiplexerInfo &info) {
  if (!sato_multiplexers) {
    // Lazy init because we can't guarantee the order of static initialization.
    sato_multiplexers =
        std::make_unique<absl::flat_hash_map<std::string, MultiplexerInfo>>();
  }
  (*sato_multiplexers)[name] = info;
}

std::unique_ptr<Message> MultiplexerCreateMessage(const std::string &message_type) {
  absl::StatusOr<MultiplexerInfo *> multiplexer_info = GetMultiplexerInfo(message_type);
  if (!multiplexer_info.ok()) {
    return nullptr;
  }
  return (*multiplexer_info)->create_message();
}

absl::Status MultiplexerParseProto(const std::string &message_type, Message &msg, ProtoBuffer &buffer) {
  absl::StatusOr<MultiplexerInfo *> multiplexer_info = GetMultiplexerInfo(message_type);
  if (!multiplexer_info.ok()) {
    return multiplexer_info.status();
  }
  return (*multiplexer_info)->parse_proto(msg, buffer);
}

absl::Status MultiplexerParseROS(const std::string &message_type, Message &msg, ROSBuffer &buffer) {
  absl::StatusOr<MultiplexerInfo *> multiplexer_info = GetMultiplexerInfo(message_type);
  if (!multiplexer_info.ok()) {
    return multiplexer_info.status();
  }
  return (*multiplexer_info)->parse_ros(msg, buffer);
}

absl::Status MultiplexerWriteProto(const std::string &message_type, const Message &msg, ProtoBuffer &buffer) {
  absl::StatusOr<MultiplexerInfo *> multiplexer_info = GetMultiplexerInfo(message_type);
  if (!multiplexer_info.ok()) {
    return multiplexer_info.status();
  }
  return (*multiplexer_info)->write_proto(msg, buffer);
}

absl::Status MultiplexerWriteROS(const std::string &message_type, const Message &msg, ROSBuffer &buffer) {
  absl::StatusOr<MultiplexerInfo *> multiplexer_info = GetMultiplexerInfo(message_type);
  if (!multiplexer_info.ok()) {
    return multiplexer_info.status();
  }
  return (*multiplexer_info)->write_ros(msg, buffer);
}

absl::StatusOr<size_t> MultiplexerSerializedProtoSize(const std::string &message_type, const Message &msg) {
  absl::StatusOr<MultiplexerInfo *> multiplexer_info = GetMultiplexerInfo(message_type);
  if (!multiplexer_info.ok()) {
    return multiplexer_info.status();
  }
  return (*multiplexer_info)->serialized_proto_size(msg);
}

absl::StatusOr<size_t> MultiplexerSerializedROSSize(const std::string &message_type, const Message &msg) {
  absl::StatusOr<MultiplexerInfo *> multiplexer_info = GetMultiplexerInfo(message_type);
  if (!multiplexer_info.ok()) {
    return multiplexer_info.status();
  }
  return (*multiplexer_info)->serialized_ros_size(msg);
}
} // namespace sato
