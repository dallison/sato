// This is heavily based on Phaser (https://github.com/dallison/phaser) and
// Neutron (https://github.com/dallison/neutron).
// Copyright (C) 2025 David Allison.  All Rights Reserved.

#pragma once
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "google/protobuf/descriptor.h"
#include "sato/compiler/enum_gen.h"
#include <iostream>
#include <map>
#include <memory>
#include <vector>
#include "zip.h"

#include "google/protobuf/compiler/code_generator.h"
#include "google/protobuf/compiler/plugin.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/io/zero_copy_stream.h"

namespace sato {

  void
WriteToZeroCopyStream(const std::string &data,
                      google::protobuf::io::ZeroCopyOutputStream *stream);
struct FieldInfo {
  // Constructor.
  FieldInfo(const google::protobuf::FieldDescriptor *f, 
            const std::string &name, const std::string &mtype,
            const std::string &ctype, const std::string &ros_type)
      : field(f), member_name(name), member_type(mtype),
        c_type(ctype), ros_type(ros_type) {
          // Remove trailing underscore from the ROS member name.
          ros_member_name = member_name.substr(0, member_name.size() - 1);
        }
  virtual ~FieldInfo() = default;
  virtual bool IsUnion() const { return false; }
  const google::protobuf::FieldDescriptor *field;

  std::string member_name;
  std::string member_type;
  std::string c_type;
  std::string ros_type;
  std::string ros_member_name;
};

struct UnionInfo : public FieldInfo {
  // Constructor
  UnionInfo(const google::protobuf::OneofDescriptor *o, 
            const std::string &name, const std::string &type)
      : FieldInfo(nullptr, name, type, "", ""), oneof(o) {}
  bool IsUnion() const override { return true; }
  const google::protobuf::OneofDescriptor *oneof;
  std::vector<std::shared_ptr<FieldInfo>> members;
};

class MessageGenerator {
public:
  MessageGenerator(const google::protobuf::Descriptor *message,
                   const std::string &added_namespace,
                   const std::string &package_name)
      : message_(message), added_namespace_(added_namespace),
        package_name_(package_name) {
    for (int i = 0; i < message_->nested_type_count(); i++) {
      nested_message_gens_.push_back(std::make_unique<MessageGenerator>(
          message_->nested_type(i), added_namespace, package_name));
    }
    // Enums
    for (int i = 0; i < message_->enum_type_count(); i++) {
      enum_gens_.push_back(
          std::make_unique<EnumGenerator>(message_->enum_type(i)));
    }
  }

  void Compile();

  void GenerateHeader(std::ostream &os);
  void GenerateSource(std::ostream &os);
  void GenerateROSMessage(zip_t *zip);

  void GenerateFieldDeclarations(std::ostream &os);

  void GenerateEnums(std::ostream &os);

private:
  void CompileFields();
  void CompileUnions();

  void GenerateDefaultConstructor(std::ostream &os, bool decl);
  void GenerateConstructors(std::ostream &os, bool decl);
  void GenerateFieldInitializers(std::ostream &os, const char *sep = ": ");
  void GenerateSizeFunctions(std::ostream &os);

  void GenerateSerializedSize(std::ostream &os, bool decl);
  void GenerateROSToProto(std::ostream &os, bool decl);
  void GenerateProtoToROS(std::ostream &os, bool decl);

  bool IsAny(const google::protobuf::Descriptor *desc);
  bool IsAny(const google::protobuf::FieldDescriptor *field);

  void GenerateMultiplexer(std::ostream &os);


  // If is_ref is true, it changes how the generator treats google.protobuf.Any.
  // For a reference to a google.protobuf.Any, we use an internal
  // ::sato::AnyMessage type.
  std::string MessageName(const google::protobuf::Descriptor *desc,
                          bool is_ref = false);
  std::string FieldCFieldType(const google::protobuf::FieldDescriptor *field);
  std::string FieldCType(const google::protobuf::FieldDescriptor *field);
  std::string FieldROSType(const google::protobuf::FieldDescriptor *field);
  std::string
  FieldRepeatedCType(const google::protobuf::FieldDescriptor *field);
  std::string FieldUnionCType(const google::protobuf::FieldDescriptor *field);
  uint32_t FieldBinarySize(const google::protobuf::FieldDescriptor *field);

  const google::protobuf::Descriptor *message_;
  std::vector<std::unique_ptr<MessageGenerator>> nested_message_gens_;
  std::vector<std::unique_ptr<EnumGenerator>> enum_gens_;
  std::vector<std::shared_ptr<FieldInfo>> fields_;
  std::map<const google::protobuf::OneofDescriptor *,
           std::shared_ptr<UnionInfo>>
      unions_;
  std::vector<std::shared_ptr<FieldInfo>> fields_in_order_;
  std::string added_namespace_;
  std::string package_name_;
};

} // namespace sato
