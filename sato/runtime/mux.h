// This is heavily based on Phaser (https://github.com/dallison/phaser) and
// Neutron (https://github.com/dallison/neutron).
// Copyright (C) 2025 David Allison.  All Rights Reserved.

#pragma once
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "sato/runtime/message.h"
#include "sato/runtime/protobuf.h"
#include "sato/runtime/ros.h"

namespace sato {

struct MultiplexerInfo {
  absl::Status (*parse_proto)(Message &msg, ProtoBuffer &buffer);
  absl::Status (*parse_ros)(Message &msg, ROSBuffer &buffer);
  absl::Status (*write_proto)(const Message &msg, ProtoBuffer &buffer);
  absl::Status (*write_ros)(const Message &msg, ROSBuffer &buffer);
  size_t (*serialized_proto_size)(const Message &msg);
  size_t (*serialized_ros_size)(const Message &msg);
};

extern std::unique_ptr<absl::flat_hash_map<std::string, MultiplexerInfo>>
    sato_multiplexers;

absl::StatusOr<MultiplexerInfo *> GetMultiplexerInfo(std::string message_type);

void MultiplexerRegisterMessage(const std::string &name, const MultiplexerInfo &info);


absl::Status MultiplexerParseProto(const std::string &message_type, Message &msg, ProtoBuffer &buffer);
absl::Status MultiplexerParseROS(const std::string &message_type, Message &msg, ROSBuffer &buffer);
absl::Status MultiplexerWriteProto(const std::string &message_type, const Message &msg, ProtoBuffer &buffer);
absl::Status MultiplexerWriteROS(const std::string &message_type, const Message &msg, ROSBuffer &buffer);
absl::StatusOr<size_t> MultiplexerSerializedProtoSize(const std::string &message_type, const Message &msg);
absl::StatusOr<size_t> MultiplexerSerializedROSSize(const std::string &message_type, const Message &msg);

} // namespace sato
