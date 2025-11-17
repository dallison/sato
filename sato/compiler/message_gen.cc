// Copyright 2024 David Allison
// All Rights Reserved
// See LICENSE file for licensing information.

#include "sato/compiler/message_gen.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_replace.h"
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <ctype.h>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <vector>

namespace sato {

void WriteToZeroCopyStream(const std::string &data,
                           google::protobuf::io::ZeroCopyOutputStream *stream) {
  // Write to the stream that protobuf wants
  void *data_buffer;
  int size;
  size_t offset = 0;
  while (offset < data.size()) {
    stream->Next(&data_buffer, &size);
    int to_copy = std::min(size, static_cast<int>(data.size() - offset));
    std::memcpy(data_buffer, data.data() + offset, to_copy);
    offset += to_copy;
    stream->BackUp(size - to_copy);
  }
}

bool IsCppReservedWord(const std::string &s) {
  static absl::flat_hash_set<std::string> reserved_words = {
      "alignas",
      "alignof",
      "and",
      "and_eq",
      "asm",
      "atomic_cancel",
      "atomic_commit",
      "atomic_noexcept",
      "auto",
      "bitand",
      "bitor",
      "bool",
      "break",
      "case",
      "catch",
      "char",
      "char8_t",
      "char16_t",
      "char32_t",
      "class",
      "compl",
      "concept",
      "const",
      "consteval",
      "constexpr",
      "constinit",
      "const_cast",
      "continue",
      "co_await",
      "co_return",
      "co_yield",
      "decltype",
      "default",
      "delete",
      "do",
      "double",
      "dynamic_cast",
      "else",
      "enum",
      "explicit",
      "export",
      "extern",
      "false",
      "float",
      "for",
      "friend",
      "goto",
      "if",
      "inline",
      "int",
      "long",
      "mutable",
      "namespace",
      "new",
      "noexcept",
      "not",
      "not_eq",
      "nullptr",
      "operator",
      "or",
      "or_eq",
      "private",
      "protected",
      "public",
      "reflexpr",
      "register",
      "reinterpret_cast",
      "requires",
      "return",
      "short",
      "signed",
      "sizeof",
      "static",
      "static_assert",
      "static_cast",
      "struct",
      "switch",
      "synchronized",
      "template",
      "this",
      "thread_local",
      "throw",
      "true",
      "try",
      "typedef",
      "typeid",
      "typename",
      "union",
      "unsigned",
      "using",
      "virtual",
      "void",
      "volatile",
      "wchar_t",
      "while",
      "xor",
      "xor_eq",
  };
  return reserved_words.contains(s);
}

std::string
MessageGenerator::EnumName(const google::protobuf::EnumDescriptor *desc) {
  std::string name = desc->name();
  if (desc->containing_type() != nullptr) {
    name = desc->containing_type()->name() + "_" + name;
  }
  return name;
}

std::string
MessageGenerator::MessageName(const google::protobuf::Descriptor *desc,
                              bool is_ref) {
  if (is_ref && IsAny(desc)) {
    return "::sato::AnyMessage";
  }
  std::string full_name = desc->full_name();
  // If the message is in our package, use the short name.
  if (full_name.find(package_name_) == std::string::npos) {
    std::string cpp_name =
        absl::StrReplaceAll(desc->full_name(), {{".", "::"}});
    if (added_namespace_.empty()) {
      return cpp_name;
    }
    // Add the namespace between the final :: and the message name.
    size_t pos = cpp_name.rfind("::");
    return cpp_name.substr(0, pos) + "::" + added_namespace_ +
           cpp_name.substr(pos);
  }
  std::string name = desc->name();
  if (desc->containing_type() != nullptr) {
    name = desc->containing_type()->name() + "_" + name;
  }
  return name;
}

std::string MessageGenerator::FieldCFieldType(
    const google::protobuf::FieldDescriptor *field) {
  switch (field->type()) {
  case google::protobuf::FieldDescriptor::TYPE_INT32:
    return "Int32Field<false, false>";
  case google::protobuf::FieldDescriptor::TYPE_SINT32:
    return "Int32Field<false, true>";
  case google::protobuf::FieldDescriptor::TYPE_SFIXED32:
    return "Int32Field<true, false>";
  case google::protobuf::FieldDescriptor::TYPE_INT64:
    return "Int64Field<false, false>";
  case google::protobuf::FieldDescriptor::TYPE_SINT64:
    return "Int64Field<false, true>";
  case google::protobuf::FieldDescriptor::TYPE_SFIXED64:
    return "Int64Field<true, false>";
  case google::protobuf::FieldDescriptor::TYPE_UINT32:
    return "Uint32Field<false, false>";
  case google::protobuf::FieldDescriptor::TYPE_FIXED32:
    return "Uint32Field<true, false>";
  case google::protobuf::FieldDescriptor::TYPE_UINT64:
    return "Uint64Field<false, false>";
  case google::protobuf::FieldDescriptor::TYPE_FIXED64:
    return "Uint64Field<true, false>";
  case google::protobuf::FieldDescriptor::TYPE_DOUBLE:
    return "DoubleField<true, false>";
  case google::protobuf::FieldDescriptor::TYPE_FLOAT:
    return "FloatField<true, false>";
  case google::protobuf::FieldDescriptor::TYPE_BOOL:
    return "BoolField<false, false>";
  case google::protobuf::FieldDescriptor::TYPE_ENUM:
    return "Uint32Field<false, false>"; // We use a uint32_t to store the enum
                                        // value.
  case google::protobuf::FieldDescriptor::TYPE_STRING:
  case google::protobuf::FieldDescriptor::TYPE_BYTES:
    return "StringField";
  case google::protobuf::FieldDescriptor::TYPE_MESSAGE:
    if (IsAny(field)) {
      return "AnyField";
    }
    return "MessageField<" + MessageName(field->message_type(), true) + ">";

  case google::protobuf::FieldDescriptor::TYPE_GROUP:
    std::cerr << "Groups are not supported\n";
    exit(1);
  }
  return "unknown";
}

std::string MessageGenerator::FieldInfoType(
    const google::protobuf::FieldDescriptor *field) {
  switch (field->type()) {
  case google::protobuf::FieldDescriptor::TYPE_INT32:
    return "::sato::FieldType::kFieldInt32";
  case google::protobuf::FieldDescriptor::TYPE_SINT32:
    return "::sato::FieldType::kFieldInt32";
  case google::protobuf::FieldDescriptor::TYPE_SFIXED32:
    return "::sato::FieldType::kFieldInt32";
  case google::protobuf::FieldDescriptor::TYPE_INT64:
    return "::sato::FieldType::kFieldInt64";
  case google::protobuf::FieldDescriptor::TYPE_SINT64:
    return "::sato::FieldType::kFieldInt64";
  case google::protobuf::FieldDescriptor::TYPE_SFIXED64:
    return "::sato::FieldType::kFieldInt64";
  case google::protobuf::FieldDescriptor::TYPE_UINT32:
    return "::sato::FieldType::kFieldInt32";
  case google::protobuf::FieldDescriptor::TYPE_FIXED32:
    return "::sato::FieldType::kFieldInt32";
  case google::protobuf::FieldDescriptor::TYPE_UINT64:
    return "::sato::FieldType::kFieldInt64";
  case google::protobuf::FieldDescriptor::TYPE_FIXED64:
    return "::sato::FieldType::kFieldInt64";
  case google::protobuf::FieldDescriptor::TYPE_DOUBLE:
    return "::sato::FieldType::kFieldDouble";
  case google::protobuf::FieldDescriptor::TYPE_FLOAT:
    return "::sato::FieldType::kFieldFloat";
  case google::protobuf::FieldDescriptor::TYPE_BOOL:
    return "::sato::FieldType::kFieldBool";
  case google::protobuf::FieldDescriptor::TYPE_ENUM:
    return "::sato::FieldType::kFieldEnum";
  case google::protobuf::FieldDescriptor::TYPE_STRING:
    return "::sato::FieldType::kFieldString";
  case google::protobuf::FieldDescriptor::TYPE_BYTES:
    return "::sato::FieldType::kFieldBytes";
  case google::protobuf::FieldDescriptor::TYPE_MESSAGE:
    return "::sato::FieldType::kFieldMessage";

  case google::protobuf::FieldDescriptor::TYPE_GROUP:
    std::cerr << "Groups are not supported\n";
    exit(1);
  }
  return "::sato::FieldType::kFieldUnknown";
}

std::string
MessageGenerator::FieldCType(const google::protobuf::FieldDescriptor *field) {
  switch (field->type()) {
  case google::protobuf::FieldDescriptor::TYPE_INT32:
  case google::protobuf::FieldDescriptor::TYPE_SINT32:
  case google::protobuf::FieldDescriptor::TYPE_SFIXED32:
    return "int32_t";
  case google::protobuf::FieldDescriptor::TYPE_INT64:
  case google::protobuf::FieldDescriptor::TYPE_SINT64:
  case google::protobuf::FieldDescriptor::TYPE_SFIXED64:
    return "int64_t";
  case google::protobuf::FieldDescriptor::TYPE_UINT32:
  case google::protobuf::FieldDescriptor::TYPE_FIXED32:
    return "uint32_t";
  case google::protobuf::FieldDescriptor::TYPE_UINT64:
  case google::protobuf::FieldDescriptor::TYPE_FIXED64:
    return "uint64_t";
  case google::protobuf::FieldDescriptor::TYPE_DOUBLE:
    return "double";
  case google::protobuf::FieldDescriptor::TYPE_FLOAT:
    return "float";
  case google::protobuf::FieldDescriptor::TYPE_BOOL:
    return "bool";
  case google::protobuf::FieldDescriptor::TYPE_ENUM:
    return "uint32_t";
  case google::protobuf::FieldDescriptor::TYPE_STRING:
  case google::protobuf::FieldDescriptor::TYPE_BYTES:
    return "std::string_view";
  case google::protobuf::FieldDescriptor::TYPE_MESSAGE:
    return MessageName(field->message_type(), true);
  case google::protobuf::FieldDescriptor::TYPE_GROUP:
    std::cerr << "Groups are not supported\n";
    exit(1);
  }
  return "::sato::FieldType::kFieldUnknown";
}

std::string
MessageGenerator::FieldROSType(const google::protobuf::FieldDescriptor *field) {
  switch (field->type()) {
  case google::protobuf::FieldDescriptor::TYPE_INT32:
  case google::protobuf::FieldDescriptor::TYPE_SINT32:
  case google::protobuf::FieldDescriptor::TYPE_SFIXED32:
    return "int32";
  case google::protobuf::FieldDescriptor::TYPE_INT64:
  case google::protobuf::FieldDescriptor::TYPE_SINT64:
  case google::protobuf::FieldDescriptor::TYPE_SFIXED64:
    return "int64";
  case google::protobuf::FieldDescriptor::TYPE_UINT32:
  case google::protobuf::FieldDescriptor::TYPE_FIXED32:
    return "uint32";
  case google::protobuf::FieldDescriptor::TYPE_UINT64:
  case google::protobuf::FieldDescriptor::TYPE_FIXED64:
    return "uint64";
  case google::protobuf::FieldDescriptor::TYPE_DOUBLE:
    return "float64";
  case google::protobuf::FieldDescriptor::TYPE_FLOAT:
    return "float32";
  case google::protobuf::FieldDescriptor::TYPE_BOOL:
    return "bool";
  case google::protobuf::FieldDescriptor::TYPE_ENUM:
    return "int32";
  case google::protobuf::FieldDescriptor::TYPE_STRING:
  case google::protobuf::FieldDescriptor::TYPE_BYTES:
    return "string";
  case google::protobuf::FieldDescriptor::TYPE_MESSAGE:
    return MessageName(field->message_type(), true);
  case google::protobuf::FieldDescriptor::TYPE_GROUP:
    std::cerr << "Groups are not supported\n";
    exit(1);
  }
  return "unknown";
}

std::string MessageGenerator::FieldRepeatedCType(
    const google::protobuf::FieldDescriptor *field) {
  std::string packed = field->is_packed() ? ", true>" : ", false>";
  switch (field->type()) {
  case google::protobuf::FieldDescriptor::TYPE_INT32:
    return "PrimitiveVectorField<int32_t, false, false" + packed;
  case google::protobuf::FieldDescriptor::TYPE_SINT32:
    return "PrimitiveVectorField<int32_t, false, true" + packed;
  case google::protobuf::FieldDescriptor::TYPE_SFIXED32:
    return "PrimitiveVectorField<int32_t, true, false" + packed;
  case google::protobuf::FieldDescriptor::TYPE_INT64:
    return "PrimitiveVectorField<int64_t, false, false" + packed;
  case google::protobuf::FieldDescriptor::TYPE_SINT64:
    return "PrimitiveVectorField<int64_t, false, true" + packed;
  case google::protobuf::FieldDescriptor::TYPE_SFIXED64:
    return "PrimitiveVectorField<int64_t, true, false" + packed;
  case google::protobuf::FieldDescriptor::TYPE_UINT32:
    return "PrimitiveVectorField<uint32_t, false, false" + packed;
  case google::protobuf::FieldDescriptor::TYPE_FIXED32:
    return "PrimitiveVectorField<uint32_t, true, false" + packed;
  case google::protobuf::FieldDescriptor::TYPE_UINT64:
    return "PrimitiveVectorField<uint64_t, false, false" + packed;
  case google::protobuf::FieldDescriptor::TYPE_FIXED64:
    return "PrimitiveVectorField<uint64_t, true, false" + packed;
  case google::protobuf::FieldDescriptor::TYPE_DOUBLE:
    return "PrimitiveVectorField<double, true, false" + packed;
  case google::protobuf::FieldDescriptor::TYPE_FLOAT:
    return "PrimitiveVectorField<float, true, false" + packed;
  case google::protobuf::FieldDescriptor::TYPE_BOOL:
    return "PrimitiveVectorField<bool, false, false" + packed;
  case google::protobuf::FieldDescriptor::TYPE_ENUM:
    return "PrimitiveVectorField<uint32_t, false, false" + packed;
  case google::protobuf::FieldDescriptor::TYPE_STRING:
  case google::protobuf::FieldDescriptor::TYPE_BYTES:
    return "StringVectorField";
  case google::protobuf::FieldDescriptor::TYPE_MESSAGE:
    return "MessageVectorField<" + MessageName(field->message_type(), true) +
           ">";
  case google::protobuf::FieldDescriptor::TYPE_GROUP:
    std::cerr << "Groups are not supported\n";
    exit(1);
  }
  return "::sato::FieldType::kFieldUnknown";
}

std::string MessageGenerator::FieldUnionCType(
    const google::protobuf::FieldDescriptor *field) {
  switch (field->type()) {
  case google::protobuf::FieldDescriptor::TYPE_INT32:
    return "UnionInt32Field<false, false>";
  case google::protobuf::FieldDescriptor::TYPE_SINT32:
    return "UnionInt32Field<false, true>";
  case google::protobuf::FieldDescriptor::TYPE_SFIXED32:
    return "UnionInt32Field<true, false>";
  case google::protobuf::FieldDescriptor::TYPE_INT64:
    return "UnionInt64Field<false, false>";
  case google::protobuf::FieldDescriptor::TYPE_SINT64:
    return "UnionInt64Field<false, true>";
  case google::protobuf::FieldDescriptor::TYPE_SFIXED64:
    return "UnionInt64Field<true, false>";
  case google::protobuf::FieldDescriptor::TYPE_UINT32:
    return "UnionUint32Field<false, false>";
  case google::protobuf::FieldDescriptor::TYPE_FIXED32:
    return "UnionUint32Field<true, false>";
  case google::protobuf::FieldDescriptor::TYPE_UINT64:
    return "UnionUint64Field<false, false>";
  case google::protobuf::FieldDescriptor::TYPE_FIXED64:
    return "UnionUint64Field<true, false>";
  case google::protobuf::FieldDescriptor::TYPE_DOUBLE:
    return "UnionDoubleField<true, false>";
  case google::protobuf::FieldDescriptor::TYPE_FLOAT:
    return "UnionFloatField<true, false>";
  case google::protobuf::FieldDescriptor::TYPE_BOOL:
    return "UnionBoolField<false, false>";
  case google::protobuf::FieldDescriptor::TYPE_ENUM:
    return "UnionUint32Field<false, false>";
  case google::protobuf::FieldDescriptor::TYPE_STRING:
  case google::protobuf::FieldDescriptor::TYPE_BYTES:
    return "UnionStringField";
  case google::protobuf::FieldDescriptor::TYPE_MESSAGE:
    return "UnionMessageField<" + MessageName(field->message_type(), true) +
           ">";
  case google::protobuf::FieldDescriptor::TYPE_GROUP:
    std::cerr << "Groups are not supported\n";
    exit(1);
  }
  return "::sato::FieldType::kFieldUnknown";
}

uint32_t MessageGenerator::FieldBinarySize(
    const google::protobuf::FieldDescriptor *field) {
  switch (field->type()) {
  case google::protobuf::FieldDescriptor::TYPE_INT32:
  case google::protobuf::FieldDescriptor::TYPE_SINT32:
  case google::protobuf::FieldDescriptor::TYPE_SFIXED32:
    return 4;
  case google::protobuf::FieldDescriptor::TYPE_INT64:
  case google::protobuf::FieldDescriptor::TYPE_SINT64:
  case google::protobuf::FieldDescriptor::TYPE_SFIXED64:
    return 8;
  case google::protobuf::FieldDescriptor::TYPE_UINT32:
  case google::protobuf::FieldDescriptor::TYPE_FIXED32:
    return 4;
  case google::protobuf::FieldDescriptor::TYPE_UINT64:
  case google::protobuf::FieldDescriptor::TYPE_FIXED64:
    return 8;
  case google::protobuf::FieldDescriptor::TYPE_DOUBLE:
    return 8;
  case google::protobuf::FieldDescriptor::TYPE_FLOAT:
    return 4;
  case google::protobuf::FieldDescriptor::TYPE_BOOL:
    return 1;
  case google::protobuf::FieldDescriptor::TYPE_ENUM:
    return 4;
  case google::protobuf::FieldDescriptor::TYPE_STRING:
  case google::protobuf::FieldDescriptor::TYPE_BYTES:
    return 4;
  case google::protobuf::FieldDescriptor::TYPE_MESSAGE:
    return 4;
  case google::protobuf::FieldDescriptor::TYPE_GROUP:
    std::cerr << "Groups are not supported\n";
    exit(1);
  }
  return 0;
}

bool MessageGenerator::IsAny(const google::protobuf::Descriptor *desc) {
  return desc->full_name() == "google.protobuf.Any";
}

bool MessageGenerator::IsAny(const google::protobuf::FieldDescriptor *field) {
  return field->type() == google::protobuf::FieldDescriptor::TYPE_MESSAGE &&
         field->message_type()->full_name() == "google.protobuf.Any";
}

void MessageGenerator::CompileUnions() {
  for (int i = 0; i < message_->field_count(); i++) {
    const auto &field = message_->field(i);
    const google::protobuf::OneofDescriptor *oneof = field->containing_oneof();
    if (oneof == nullptr) {
      // Not a oneof, already handled in CompileFields.
      continue;
    }
    // We will have created a UnionInfo during the first pass in CompileFields.
    auto it = unions_.find(oneof);
    assert(it != unions_.end());

    auto union_info = it->second;
    // Append field to the members of the union.
    std::string field_type = FieldUnionCType(field);
    // Append union type to the end of the the union member type
    if (union_info->member_type == "UnionField") {
      union_info->member_type += "<";
    } else {
      union_info->member_type += ", ";
    }
    union_info->member_type += "::sato::" + field_type;
    uint32_t field_size = FieldBinarySize(field);
    union_info->members.push_back(std::make_shared<FieldInfo>(
        field, 0, union_info->id, field->name() + "_", field_type,
        FieldCType(field), FieldROSType(field), field_size));
    union_info->binary_size = std::max(union_info->binary_size, 4 + field_size);
    union_info->id++;
  }
  for (auto &[oneof, union_info] : unions_) {
    union_info->member_type += ">";
  }
}

void MessageGenerator::CompileFields() {
  uint32_t offset = 0;
  uint32_t id = 0;
  fields_.reserve(message_->field_count());
  for (int i = 0; i < message_->field_count(); i++) {
    const auto &field = message_->field(i);
    std::string field_type;
    const google::protobuf::OneofDescriptor *oneof = field->containing_oneof();
    int32_t field_size;
    uint32_t next_id = id;
    if (oneof != nullptr) {
      // In order to keep oneof fields in the correct position for printing so
      // that we match the protobuf printer, we create the union field here and
      // add it to the fields_in_order_ vector.  We will fill it in later during
      // the CompileUnions phase.  Since there will be multiple fields in the
      // union and we see each of them here, we only add it the first time we
      // see the oneof.
      auto it = unions_.find(oneof);
      if (it == unions_.end()) {
        auto union_info = std::make_shared<UnionInfo>(
            oneof, 4, oneof->name() + "_", "UnionField");
        unions_[oneof] = union_info;
        fields_in_order_.push_back(union_info);
      }
      continue;
    } else if (field->is_repeated()) {
      field_type = FieldRepeatedCType(field);
      field_size = 8;
    } else {
      field_type = FieldCFieldType(field);
      field_size = FieldBinarySize(field);
      if (field->type() != google::protobuf::FieldDescriptor::TYPE_MESSAGE &&
          field->type() != google::protobuf::FieldDescriptor::TYPE_STRING &&
          field->type() != google::protobuf::FieldDescriptor::TYPE_BYTES) {
        // Strings and messages don't consume a presence bit.
        next_id++;
      } else {
        id = 0;
      }
    }
    offset = (offset + (field_size - 1)) & ~(field_size - 1);
    fields_.push_back(std::make_shared<FieldInfo>(
        field, offset, id, field->name() + "_", field_type, FieldCType(field),
        FieldROSType(field), field_size));
    fields_in_order_.push_back(fields_.back());
    offset += field_size;
    id = next_id;
  }
}

void MessageGenerator::FinalizeOffsetsAndSizes() {
  uint32_t size = 4;
  // Find the max field id.  This will determine the number of 32-bit words we
  // need for the presence mask.
  int32_t max_id = -1;
  for (auto &field : fields_) {
    max_id = std::max(max_id, int32_t(field->id));
  }
  presence_mask_size_ = max_id == -1 ? 0 : ((max_id >> 5) + 1) * 4;
  size += presence_mask_size_;

  // Finalize the offsets in the fields vector now that we know the header size.
  for (auto &field : fields_) {
    field->offset += size;
  }

  // Set the offsets for the unions.
  uint32_t offset =
      fields_.empty() ? size
                      : (fields_.back()->offset + fields_.back()->binary_size);
  // Align offset to 4 bytes.
  offset = (offset + 3) & ~3;
  size = offset;

  // Add the offset to the unions.
  for (auto &[oneof, u] : unions_) {
    u->offset = offset;
    for (auto &field : u->members) {
      field->offset += offset;
    }
    offset += u->binary_size;
    size += u->binary_size;
  }
  binary_size_ = size;
}

void MessageGenerator::GenerateROSMessage(zip_t *zip) {
  for (const auto &nested : nested_message_gens_) {
    nested->GenerateROSMessage(zip);
  }

  // Write the message to a stringstream
  std::stringstream ss;
  for (auto &field : fields_) {
    ss << field->ros_type << " " << field->ros_member_name << "\n";
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
  std::string filename = MessageName(message_) + ".msg";
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

void MessageGenerator::Compile() {
  CompileFields();
  CompileUnions();
  FinalizeOffsetsAndSizes();
}

void MessageGenerator::GenerateHeader(std::ostream &os) {
  for (const auto &nested : nested_message_gens_) {
    nested->GenerateHeader(os);
  }

  os << "class " << MessageName(message_) << " {\n";
  os << " public:\n";
  // Generate constructors.
  GenerateConstructors(os, true);
  // Generate size functions.
  GenerateSizeFunctions(os);
  // Generate creators.
  GenerateCreators(os, true);
  // Generate clear function.
  GenerateClear(os, true);
  // Generate field metadata.
  GenerateFieldMetadata(os);

  os << "  static std::string FullName() { return \"" << message_->full_name()
     << "\"; }\n";
  os << "  static std::string Name() { return \"" << message_->name()
     << "\"; }\n\n";

  os << "  std::string GetName() const { return Name(); }\n";
  os << "  std::string GetFullName() const { return FullName(); }\n";

  GenerateNestedTypes(os);
  GenerateFieldNumbers(os);

  GenerateIndent(os);
  GenerateCopy(os, true);
  GenerateDebugString(os);

  // Generate protobuf accessors.
  GenerateProtobufAccessors(os);

  GenerateProtobufSerialization(os);

  // Generate serialized size.
  GenerateSerializedSize(os, true);
  // Generate serializer.
  GenerateROSToProto(os, true);
  // Generate deserializer.
  GenerateProtoToROS(os, true);

  os << " private:\n";
  GenerateFieldDeclarations(os);
  os << "};\n\n";

  // Steamer outside the class.
  GenerateStreamer(os);
  GenerateCopy(os, false);
}

void MessageGenerator::GenerateSource(std::ostream &os) {
  for (const auto &nested : nested_message_gens_) {
    nested->GenerateSource(os);
  }

  GenerateConstructors(os, false);

  // Generate creators.
  GenerateCreators(os, false);
  // Generate clear function.
  GenerateClear(os, false);

  // Generate serialized size.
  GenerateSerializedSize(os, false);
  // Generate serializer.
  GenerateROSToProto(os, false);
  // Generate deserializer.
  GenerateProtoToROS(os, false);

  // sato bank
  // GeneratePhaserBank(os);
}

void MessageGenerator::GenerateFieldDeclarations(std::ostream &os) {
  for (auto &field : fields_) {
    os << "  ::sato::" << field->member_type << " " << field->member_name
       << ";\n";
  }
  for (auto &[oneof, u] : unions_) {
    os << "  ::sato::" << u->member_type << " " << u->member_name << ";\n";
  }
}

void MessageGenerator::GenerateEnums(std::ostream &os) {
  // Nested enums.
  for (auto &msg : nested_message_gens_) {
    msg->GenerateEnums(os);
  }
  for (auto &enum_gen : enum_gens_) {
    enum_gen->GenerateHeader(os);
  }
}

void MessageGenerator::GenerateConstructors(std::ostream &os, bool decl) {
  // Generate default constructor.
  GenerateDefaultConstructor(os, decl);
  // GenerateInternalDefaultConstructor(os, decl);
  // Generate main constructor.
  // GenerateMainConstructor(os, decl);
}

void MessageGenerator::GenerateDefaultConstructor(std::ostream &os, bool decl) {
  if (decl) {
    os << "  " << MessageName(message_) << "();\n";
    return;
  }
  os << MessageName(message_) << "::" << MessageName(message_) << "()\n";
  // Generate field initializers.
  GenerateFieldInitializers(os);
  os << "{}\n\n";
}

void MessageGenerator::GenerateInternalDefaultConstructor(std::ostream &os,
                                                          bool decl) {
  if (decl) {
    os << "  " << MessageName(message_) << "(::sato::InternalDefault d);\n";
    return;
  }
  os << MessageName(message_) << "::" << MessageName(message_)
     << "(::sato::InternalDefault d)\n";
  // Generate field initializers.
  GenerateFieldInitializers(os);
  os << "{}\n\n";
}

void MessageGenerator::GenerateMainConstructor(std::ostream &os, bool decl) {
  if (decl) {
    os << "  " << MessageName(message_)
       << "(std::shared_ptr<::sato::MessageRuntime> runtime, "
          "::toolbelt::BufferOffset "
          "offset);\n";
    return;
  }
  os << MessageName(message_) << "::" << MessageName(message_) << "(";
  os << "std::shared_ptr<::sato::MessageRuntime> runtime, "
        "::toolbelt::BufferOffset "
        "offset) : Message(runtime, offset)\n";
  // Generate field initializers.
  GenerateFieldInitializers(os, ", ");
  os << "{}\n\n";
}

void MessageGenerator::GenerateFieldInitializers(std::ostream &os,
                                                 const char *sep) {
  if (fields_.empty() && unions_.empty()) {
    return;
  }

  for (auto &field : fields_) {
    os << sep << field->member_name << "(" << field->field->number() << ")\n";
    sep = ", ";
  }
  for (auto &[oneof, u] : unions_) {
    os << sep << u->member_name << "({";
    const char *num_sep = "";
    for (auto &field : u->members) {
      os << num_sep << field->field->number();
      num_sep = ",";
    }
    os << "})\n";
    sep = ", ";
  }
}

void MessageGenerator::GenerateCreators(std::ostream &os, bool decl) {}

void MessageGenerator::GenerateSizeFunctions(std::ostream &os) {}

void MessageGenerator::GenerateFieldMetadata(std::ostream &os) {}

void MessageGenerator::GenerateClear(std::ostream &os, bool decl) {}

void MessageGenerator::GenerateProtobufAccessors(std::ostream &os) {}

void MessageGenerator::GenerateFieldProtobufAccessors(std::ostream &os) {}

void MessageGenerator::GenerateFieldProtobufAccessors(
    std::shared_ptr<FieldInfo> field, std::shared_ptr<UnionInfo> union_field,
    int union_index, std::ostream &os) {}

void MessageGenerator::GenerateAnyProtobufAccessors(
    std::shared_ptr<FieldInfo> field, std::shared_ptr<UnionInfo> union_field,
    int union_index, std::ostream &os) {}

void MessageGenerator::GenerateUnionProtobufAccessors(std::ostream &os) {}

void MessageGenerator::GenerateNestedTypes(std::ostream &os) {}

void MessageGenerator::GenerateFieldNumbers(std::ostream &os) {}

void MessageGenerator::GenerateSerializedSize(std::ostream &os, bool decl) {
  if (decl) {
    os << "  size_t SerializedProtoSize() const;\n";
    os << "  size_t SerializedROSSize() const;\n";
    return;
  }
  os << "size_t " << MessageName(message_)
     << "::SerializedProtoSize() const {\n";
  os << "  size_t size = 0;\n";
  for (auto &field : fields_) {
    if (field->field->is_repeated()) {
      os << "  size += " << field->member_name << ".SerializedProtoSize();\n";
    } else {
      os << "  if (" << field->member_name << ".IsPresent()) {\n";
      os << "    size += " << field->member_name << ".SerializedProtoSize();\n";
      os << "  }\n";
    }
  }
  for (auto &[oneof, u] : unions_) {
    os << "  switch (" << u->member_name << ".Discriminator()) {\n";
    for (size_t i = 0; i < u->members.size(); i++) {
      auto &field = u->members[i];
      os << "  case " << field->field->number() << ":\n";
      os << "    size += " << u->member_name << ".SerializedProtoSize<" << i
         << ">();\n";
      os << "    break;\n";
    }
    os << "  }\n";
  }
  os << "  return size;\n";
  os << "}\n\n";

  os << "size_t " << MessageName(message_) << "::SerializedROSSize() const {\n";
  os << "  size_t size = 0;\n";
  for (auto &field : fields_) {
    os << "  size += " << field->member_name << ".SerializedROSSize();\n";
  }
  // In ROS format we expand all the union members into the message.  ROS has no
  // concept of oneofs.
  for (auto &[oneof, u] : unions_) {
    os << "  size += " << u->member_name << ".SerializedROSSize();\n";
  }
  os << "  return size;\n";
  os << "}\n\n";
}

void MessageGenerator::GenerateROSToProto(std::ostream &os, bool decl) {
  if (decl) {
    os << "  absl::Status ROSToProto(::sato::ROSBuffer &ros_buffer, "
          "::sato::ProtoBuffer &buffer);\n";
    os << "  absl::Status ParseROS(::sato::ROSBuffer &buffer);\n";
    os << "  absl::Status WriteProto(::sato::ProtoBuffer &buffer) const;\n";
    return;
  }

  os << "absl::Status " << MessageName(message_)
     << "::ParseROS(::sato::ROSBuffer &buffer) {\n";

  for (auto &field : fields_) {
    os << "  if (absl::Status status = " << field->member_name
       << ".ParseROS(buffer); !status.ok()) return status;\n";
  }
  for (auto &[oneof, u] : unions_) {
    os << "  if (absl::Status status = " << u->member_name
       << ".ParseROS(buffer); !status.ok()) return status;\n";
  }
  os << "  return absl::OkStatus();\n";
  os << "}\n\n";

  os << "absl::Status " << MessageName(message_)
     << "::WriteProto(::sato::ProtoBuffer &buffer) const {\n";
  for (auto &field : fields_) {
    os << "  if (" << field->member_name << ".IsPresent()) {\n";
    os << "    if (absl::Status status = " << field->member_name
       << ".WriteProto(buffer); !status.ok()) return status;\n";
    os << "  }\n";
  }
  for (auto &[oneof, u] : unions_) {
    os << "  switch (" << u->member_name << ".Discriminator()) {\n";
    for (size_t i = 0; i < u->members.size(); i++) {
      auto &field = u->members[i];
      os << "  case " << field->field->number() << ":\n";
      os << "    if (absl::Status status = " << u->member_name
         << ".WriteProto<" << i << ">(buffer); !status.ok()) return status;\n";
      os << "    break;\n";
    }
    os << "  }\n";
  }
  os << "  return absl::OkStatus();\n";
  os << "}\n\n";

  os << "absl::Status " << MessageName(message_)
     << "::ROSToProto(::sato::ROSBuffer &ros_buffer, "
          "::sato::ProtoBuffer &buffer) {";
  os << "  if (absl::Status status = ParseROS(ros_buffer); !status.ok()) return "
        "status;\n";
  os << "  if (absl::Status status = WriteProto(buffer); !status.ok()) "
        "return status;\n";
  os << "  return absl::OkStatus();\n";
  os << "}\n\n";
}

void MessageGenerator::GenerateProtoToROS(std::ostream &os, bool decl) {
  if (decl) {
    os << "  absl::Status ProtoToROS(::sato::ProtoBuffer &buffer, "
          "::sato::ROSBuffer "
          "&ros_buffer);\n";
    os << "  absl::Status ParseProto(::sato::ProtoBuffer &buffer);\n";
    os << "  absl::Status WriteROS(::sato::ROSBuffer &buffer) const;\n";
    return;
  }

  os << "absl::Status " << MessageName(message_)
     << "::ParseProto(::sato::ProtoBuffer &buffer) {\n";
  os << R"XXX(
  while (!buffer.Eof()) {
    absl::StatusOr<uint32_t> tag =
        buffer.DeserializeVarint<uint32_t, false>();
    if (!tag.ok()) {
      return tag.status();
    }
    uint32_t field_number = *tag >> ::sato::ProtoBuffer::kFieldIdShift;
    switch (field_number) {
)XXX";
  for (auto &field : fields_) {
    os << "    case " << field->field->number() << ":\n";
    os << "      if (absl::Status status = " << field->member_name
       << ".ParseProto(buffer); !status.ok()) return status;\n";
    os << "      break;\n";
  }
  for (auto &[oneof, u] : unions_) {
    for (size_t i = 0; i < u->members.size(); i++) {
      auto &field = u->members[i];
      os << "    case " << field->field->number() << ":\n";
      os << "      if (absl::Status status = " << u->member_name
         << ".ParseProto<" << i << ">(buffer); !status.ok()) return status;\n";
      os << "      break;\n";
    }
  }
  os << R"XXX(
    default:
      if (absl::Status status = buffer.SkipTag(*tag); !status.ok()) {
        return status;
      }
    }
  }
  return absl::OkStatus();
}
  
)XXX";

  os << "absl::Status " << MessageName(message_)
     << "::WriteROS(::sato::ROSBuffer &buffer) const {\n";
  for (auto &field : fields_) {
    os << "  if (absl::Status status = " << field->member_name
       << ".WriteROS(buffer); !status.ok()) return status;\n";
  }

  for (auto &[oneof, u] : unions_) {
    // ROS has no concept of oneofs.  We expand all the union members into the
    // message.
    os << "  if (absl::Status status = " << u->member_name
       << ".WriteROS(buffer); !status.ok()) return status;\n";
  }
  os << "  return absl::OkStatus();\n";
  os << "}\n\n";

  os << "absl::Status " << MessageName(message_)
     << "::ProtoToROS(::sato::ProtoBuffer &buffer, ::sato::ROSBuffer "
        "&ros_buffer) {";
  os << "  if (absl::Status status = ParseProto(buffer); !status.ok()) return "
        "status;\n";
  os << "  if (absl::Status status = WriteROS(ros_buffer); !status.ok()) "
        "return status;\n";
  os << "  return absl::OkStatus();\n";
  os << "}\n\n";
}

void MessageGenerator::GenerateProtobufSerialization(std::ostream &os) {}

void MessageGenerator::GenerateIndent(std::ostream &os) {}

void MessageGenerator::GenerateStreamer(std::ostream &os) {}

void MessageGenerator::GenerateCopy(std::ostream &os, bool decl) {}

// DebugString
void MessageGenerator::GenerateDebugString(std::ostream &os) {}

void MessageGenerator::GeneratePhaserBank(std::ostream &os) {

  os << "static absl::Status " << MessageName(message_)
     << "SerializeToBuffer(const ::sato::Message& msg, ::sato::ProtoBuffer "
        "&buffer) {\n";
  os << "  const " << MessageName(message_) << " *m = static_cast<const "
     << MessageName(message_) << "*>(&msg);\n";
  os << "  return m->Serialize(buffer);\n";
  os << "}\n\n";

  os << "static absl::Status " << MessageName(message_)
     << "DeserializeFromBuffer(::sato::Message &msg, ::sato::ProtoBuffer "
        "&buffer) {\n";
  os << "  " << MessageName(message_) << " *m = static_cast<"
     << MessageName(message_) << "*>(&msg);\n";
  os << "  return m->Deserialize(buffer);\n";
  os << "}\n\n";

  os << "static size_t " << MessageName(message_)
     << "SerializedSize(const ::sato::Message& msg) {\n";
  os << "  const " << MessageName(message_) << " *m = static_cast<const "
     << MessageName(message_) << "*>(&msg);\n";
  os << "  return m->SerializedSize();\n";
  os << "}\n\n";

  os << "static ::sato::BankInfo " << MessageName(message_) << "BankInfo = {\n";
  os << "  .serialize_to_buffer = " << MessageName(message_)
     << "SerializeToBuffer,\n";
  os << "  .deserialize_from_buffer = " << MessageName(message_)
     << "DeserializeFromBuffer,\n";
  os << "  .serialized_size = " << MessageName(message_) << "SerializedSize,\n";

  os << "};\n\n";

  os << "static struct " << MessageName(message_) << "BankInitializer {\n";
  os << "  " << MessageName(message_) << "BankInitializer() {\n";
  os << "    ::sato::PhaserBankRegisterMessage(" << MessageName(message_)
     << "::FullName(), " << MessageName(message_) << "BankInfo);\n";
  os << "  }\n";
  os << "} " << MessageName(message_) << "BankInitializer;\n";
}

void MessageGenerator::GenerateFieldInfo(int index,
                                         std::shared_ptr<FieldInfo> field,
                                         std::shared_ptr<UnionInfo> union_field,
                                         int union_index, std::ostream &os) {}

} // namespace sato
