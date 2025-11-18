// This is heavily based on Phaser (https://github.com/dallison/phaser) and
// Neutron (https://github.com/dallison/neutron).
// Copyright (C) 2025 David Allison.  All Rights Reserved.

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
