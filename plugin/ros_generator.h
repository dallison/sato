#ifndef SATO_PLUGIN_ROS_GENERATOR_H_
#define SATO_PLUGIN_ROS_GENERATOR_H_

#include <google/protobuf/compiler/code_generator.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/io/printer.h>
#include <string>

namespace sato {

class RosGenerator : public google::protobuf::compiler::CodeGenerator {
 public:
  RosGenerator() = default;
  ~RosGenerator() override = default;

  bool Generate(const google::protobuf::FileDescriptor* file,
                const std::string& parameter,
                google::protobuf::compiler::GeneratorContext* context,
                std::string* error) const override;

 private:
  void GenerateHeader(const google::protobuf::Descriptor* message,
                      google::protobuf::io::Printer* printer) const;
  
  void GenerateSource(const google::protobuf::Descriptor* message,
                      google::protobuf::io::Printer* printer) const;
  
  void GenerateConverters(const google::protobuf::Descriptor* message,
                          google::protobuf::io::Printer* printer) const;
  
  std::string GetROSType(const google::protobuf::FieldDescriptor* field) const;
  std::string GetCppType(const google::protobuf::FieldDescriptor* field) const;
};

}  // namespace sato

#endif  // SATO_PLUGIN_ROS_GENERATOR_H_
