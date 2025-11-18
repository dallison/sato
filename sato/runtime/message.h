// This is heavily based on Phaser (https://github.com/dallison/phaser) and
// Neutron (https://github.com/dallison/neutron).
// Copyright (C) 2025 David Allison.  All Rights Reserved.

#pragma once

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "sato/runtime/protobuf.h"
#include "sato/runtime/ros.h"
namespace sato {

class Message {
public:
  virtual ~Message() = default;
  bool IsPopulated() const { return populated_; }
  void SetPopulated(bool populated) { populated_ = populated; }

  virtual std::string GetName() const = 0;
  virtual std::string GetFullName() const = 0;

  virtual size_t SerializedProtoSize() const = 0;
  virtual size_t SerializedROSSize() const = 0;
  virtual absl::Status WriteProto(ProtoBuffer &buffer) const = 0;
  virtual absl::Status WriteROS(ROSBuffer &buffer, uint64_t timestamp = 0) const = 0;
  virtual absl::Status ParseProto(ProtoBuffer &buffer) = 0;
  virtual absl::Status ParseROS(ROSBuffer &buffer) = 0;

  absl::Status ProtoToROS(ProtoBuffer &proto_buffer, ROSBuffer &ros_buffer, uint64_t timestamp = 0) {
    if (absl::Status status = ParseProto(proto_buffer); !status.ok()) {
      return status;
    }
    if (absl::Status status = WriteROS(ros_buffer, timestamp); !status.ok()) {
      return status;
    }
    return absl::OkStatus();
  }
  absl::Status ROSToProto(ROSBuffer &ros_buffer, ProtoBuffer &proto_buffer) {
    if (absl::Status status = ParseROS(ros_buffer); !status.ok()) {
      return status;
    }
    if (absl::Status status = WriteProto(proto_buffer); !status.ok()) {
      return status;
    }
    return absl::OkStatus();
  }

private:
  bool populated_ = false;
};
}
