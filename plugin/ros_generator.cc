#include "plugin/ros_generator.h"

#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <sstream>

namespace sato {

namespace {

std::string ToUpperCase(const std::string& str) {
  std::string result = str;
  for (char& c : result) {
    c = toupper(c);
  }
  return result;
}

std::string ToCamelCase(const std::string& str) {
  std::string result;
  bool capitalize_next = true;
  for (char c : str) {
    if (c == '_') {
      capitalize_next = true;
    } else {
      result += capitalize_next ? toupper(c) : c;
      capitalize_next = false;
    }
  }
  return result;
}

}  // namespace

bool RosGenerator::Generate(
    const google::protobuf::FileDescriptor* file,
    const std::string& parameter,
    google::protobuf::compiler::GeneratorContext* context,
    std::string* error) const {
  
  for (int i = 0; i < file->message_type_count(); ++i) {
    const google::protobuf::Descriptor* message = file->message_type(i);
    
    // Generate ROS message header
    std::string header_filename = message->name() + "_ros.h";
    std::unique_ptr<google::protobuf::io::ZeroCopyOutputStream> header_output(
        context->Open(header_filename));
    google::protobuf::io::Printer header_printer(header_output.get(), '$');
    GenerateHeader(message, &header_printer);
    
    // Generate ROS message source
    std::string source_filename = message->name() + "_ros.cc";
    std::unique_ptr<google::protobuf::io::ZeroCopyOutputStream> source_output(
        context->Open(source_filename));
    google::protobuf::io::Printer source_printer(source_output.get(), '$');
    GenerateSource(message, &source_printer);
  }
  
  return true;
}

void RosGenerator::GenerateHeader(
    const google::protobuf::Descriptor* message,
    google::protobuf::io::Printer* printer) const {
  
  std::string guard = "SATO_GENERATED_" + ToUpperCase(message->name()) + "_ROS_H_";
  
  printer->Print("#ifndef $guard$\n", "guard", guard);
  printer->Print("#define $guard$\n\n", "guard", guard);
  
  printer->Print("#include <string>\n");
  printer->Print("#include <vector>\n");
  printer->Print("#include <cstdint>\n\n");
  
  printer->Print("namespace sato {\n");
  printer->Print("namespace ros {\n\n");
  
  // Generate ROS struct
  printer->Print("struct $name$ {\n", "name", message->name());
  printer->Indent();
  
  for (int i = 0; i < message->field_count(); ++i) {
    const google::protobuf::FieldDescriptor* field = message->field(i);
    std::string ros_type = GetROSType(field);
    printer->Print("$type$ $name$;\n", 
                   "type", ros_type,
                   "name", field->name());
  }
  
  printer->Outdent();
  printer->Print("};\n\n");
  
  // Generate converter class
  printer->Print("class $name$Converter {\n", "name", message->name());
  printer->Print(" public:\n");
  printer->Indent();
  
  printer->Print("// Convert from protobuf to ROS\n");
  printer->Print("static bool ProtoToRos(const std::string& proto_data, $name$* ros_msg);\n\n",
                 "name", message->name());
  
  printer->Print("// Convert from ROS to protobuf\n");
  printer->Print("static bool RosToProto(const $name$& ros_msg, std::string* proto_data);\n",
                 "name", message->name());
  
  printer->Outdent();
  printer->Print("};\n\n");
  
  printer->Print("}  // namespace ros\n");
  printer->Print("}  // namespace sato\n\n");
  
  printer->Print("#endif  // $guard$\n", "guard", guard);
}

void RosGenerator::GenerateSource(
    const google::protobuf::Descriptor* message,
    google::protobuf::io::Printer* printer) const {
  
  printer->Print("#include \"$header$\"\n\n",
                 "header", message->name() + "_ros.h");
  
  printer->Print("#include \"$proto_header$\"\n\n",
                 "proto_header", message->file()->name());
  
  printer->Print("namespace sato {\n");
  printer->Print("namespace ros {\n\n");
  
  GenerateConverters(message, printer);
  
  printer->Print("}  // namespace ros\n");
  printer->Print("}  // namespace sato\n");
}

void RosGenerator::GenerateConverters(
    const google::protobuf::Descriptor* message,
    google::protobuf::io::Printer* printer) const {
  
  std::string proto_type = message->full_name();
  std::string msg_name = message->name();
  
  // ProtoToRos converter
  printer->Print("bool $name$Converter::ProtoToRos(\n",
                 "name", msg_name);
  printer->Print("    const std::string& proto_data, $name$* ros_msg) {\n",
                 "name", msg_name);
  printer->Indent();
  
  printer->Print("if (ros_msg == nullptr) {\n");
  printer->Print("  return false;\n");
  printer->Print("}\n\n");
  
  printer->Print("$proto_type$ proto_msg;\n",
                 "proto_type", proto_type);
  printer->Print("if (!proto_msg.ParseFromString(proto_data)) {\n");
  printer->Print("  return false;\n");
  printer->Print("}\n\n");
  
  for (int i = 0; i < message->field_count(); ++i) {
    const google::protobuf::FieldDescriptor* field = message->field(i);
    std::string field_name = field->name();
    
    if (field->is_repeated()) {
      printer->Print("ros_msg->$name$.clear();\n", "name", field_name);
      printer->Print("for (int i = 0; i < proto_msg.$name$_size(); ++i) {\n",
                     "name", field_name);
      printer->Print("  ros_msg->$name$.push_back(proto_msg.$name$(i));\n",
                     "name", field_name);
      printer->Print("}\n");
    } else {
      printer->Print("ros_msg->$name$ = proto_msg.$name$();\n",
                     "name", field_name);
    }
  }
  
  printer->Print("\nreturn true;\n");
  printer->Outdent();
  printer->Print("}\n\n");
  
  // RosToProto converter
  printer->Print("bool $name$Converter::RosToProto(\n",
                 "name", msg_name);
  printer->Print("    const $name$& ros_msg, std::string* proto_data) {\n",
                 "name", msg_name);
  printer->Indent();
  
  printer->Print("if (proto_data == nullptr) {\n");
  printer->Print("  return false;\n");
  printer->Print("}\n\n");
  
  printer->Print("$proto_type$ proto_msg;\n",
                 "proto_type", proto_type);
  
  for (int i = 0; i < message->field_count(); ++i) {
    const google::protobuf::FieldDescriptor* field = message->field(i);
    std::string field_name = field->name();
    
    if (field->is_repeated()) {
      printer->Print("for (const auto& item : ros_msg.$name$) {\n",
                     "name", field_name);
      printer->Print("  proto_msg.add_$name$(item);\n",
                     "name", field_name);
      printer->Print("}\n");
    } else {
      printer->Print("proto_msg.set_$name$(ros_msg.$name$);\n",
                     "name", field_name);
    }
  }
  
  printer->Print("\nif (!proto_msg.SerializeToString(proto_data)) {\n");
  printer->Print("  return false;\n");
  printer->Print("}\n\n");
  
  printer->Print("return true;\n");
  printer->Outdent();
  printer->Print("}\n");
}

std::string RosGenerator::GetROSType(
    const google::protobuf::FieldDescriptor* field) const {
  std::string base_type;
  
  switch (field->type()) {
    case google::protobuf::FieldDescriptor::TYPE_DOUBLE:
      base_type = "double";
      break;
    case google::protobuf::FieldDescriptor::TYPE_FLOAT:
      base_type = "float";
      break;
    case google::protobuf::FieldDescriptor::TYPE_INT32:
    case google::protobuf::FieldDescriptor::TYPE_SINT32:
    case google::protobuf::FieldDescriptor::TYPE_SFIXED32:
      base_type = "int32_t";
      break;
    case google::protobuf::FieldDescriptor::TYPE_INT64:
    case google::protobuf::FieldDescriptor::TYPE_SINT64:
    case google::protobuf::FieldDescriptor::TYPE_SFIXED64:
      base_type = "int64_t";
      break;
    case google::protobuf::FieldDescriptor::TYPE_UINT32:
    case google::protobuf::FieldDescriptor::TYPE_FIXED32:
      base_type = "uint32_t";
      break;
    case google::protobuf::FieldDescriptor::TYPE_UINT64:
    case google::protobuf::FieldDescriptor::TYPE_FIXED64:
      base_type = "uint64_t";
      break;
    case google::protobuf::FieldDescriptor::TYPE_BOOL:
      base_type = "bool";
      break;
    case google::protobuf::FieldDescriptor::TYPE_STRING:
    case google::protobuf::FieldDescriptor::TYPE_BYTES:
      base_type = "std::string";
      break;
    default:
      base_type = "int32_t";  // Default fallback
      break;
  }
  
  if (field->is_repeated()) {
    return "std::vector<" + base_type + ">";
  }
  
  return base_type;
}

std::string RosGenerator::GetCppType(
    const google::protobuf::FieldDescriptor* field) const {
  return GetROSType(field);
}

}  // namespace sato
