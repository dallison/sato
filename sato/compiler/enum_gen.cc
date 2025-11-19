// This is heavily based on Phaser (https://github.com/dallison/phaser) and
// Neutron (https://github.com/dallison/neutron).
// Copyright (C) 2025 David Allison.  All Rights Reserved.

#include "sato/compiler/enum_gen.h"
#include "absl/strings/str_replace.h"
#include "sato/compiler/zip_utils.h"

#include <algorithm>

namespace sato {

void EnumGenerator::GenerateROSMessage(zip_t *zip) {
  std::stringstream ss;
  std::string name = enum_->name();
  if (enum_->containing_type() != nullptr) {
    name = enum_->containing_type()->name() + "_" + name;
  }
  for (int i = 0; i < enum_->value_count(); i++) {
    const google::protobuf::EnumValueDescriptor *value = enum_->value(i);
    std::string const_name = value->name();
    if (enum_->containing_type() != nullptr) {
      const_name = name + "_" + const_name;
    }
    ss << "int32  " << const_name << " = " << value->number() << "\n";
  }

  // Extract the string from the stringstream
  std::string content = ss.str();
  if (absl::Status status = AddFileToZip(zip, enum_->full_name(), content); !status.ok()) {
    std::cerr << "Failed to add file to zip: " << status.message() << "\n";
    exit(1);
  }
}

} // namespace sato
