// Copyright 2024 David Allison
// All Rights Reserved
// See LICENSE file for licensing information.

#pragma once
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "google/protobuf/descriptor.h"
#include <iostream>
#include <map>
#include <memory>
#include <vector>
#include "zip.h"
namespace sato {

class EnumGenerator {
public:
  EnumGenerator(const google::protobuf::EnumDescriptor *e) : enum_(e) {}

  void GenerateROSMessage(zip_t *zip);

private:
  friend class MessageGenerator;
  const google::protobuf::EnumDescriptor *enum_;
  std::vector<std::unique_ptr<EnumGenerator>> nested_enum_gens_;
};

} // namespace sato
