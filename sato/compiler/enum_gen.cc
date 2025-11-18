// This is heavily based on Phaser (https://github.com/dallison/phaser) and
// Neutron (https://github.com/dallison/neutron).
// Copyright (C) 2025 David Allison.  All Rights Reserved.

#include "sato/compiler/enum_gen.h"

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

  // Allocate buffer on heap and copy content so libzip can take ownership
  // This ensures the data persists until libzip writes it to the zip file
  zip_uint8_t *buffer = nullptr;
  zip_uint64_t buffer_size = content.size();
  if (buffer_size > 0) {
    buffer = static_cast<zip_uint8_t *>(malloc(buffer_size));
    if (buffer == nullptr) {
      std::cerr << "Failed to allocate buffer for zip source\n";
      exit(1);
    }
    std::memcpy(buffer, content.data(), buffer_size);
  }

  // Add the contents to the zip.
  // freep=1 means libzip will free the buffer when done
  zip_source_t *source = zip_source_buffer(zip, buffer, buffer_size, 1);
  if (source == nullptr) {
    std::cerr << "Failed to create zip source: " << zip_strerror(zip) << "\n";
    exit(1);
  }
  std::string filename = name + ".msg";
  zip_int64_t index =
      zip_file_add(zip, filename.c_str(), source, ZIP_FL_ENC_UTF_8);
  if (index < 0) {
    std::cerr << "Failed to add file " << filename
              << " to zip: " << zip_strerror(zip) << "\n";
    zip_source_free(source);
    exit(1);
  }
  // Note: zip_file_add takes ownership of source, so we don't free it
}

} // namespace sato
