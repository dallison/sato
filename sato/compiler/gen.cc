// Copyright 2024 David Allison
// All Rights Reserved
// See LICENSE file for licensing information.

#include "sato/compiler/gen.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include <filesystem>
#include <fstream>

namespace sato {

static std::string GeneratedFilename(const std::filesystem::path &package_name,
                                     const std::filesystem::path &target_name,
                                     std::string filename) {
  size_t virtual_imports = filename.find("_virtual_imports/");
  if (virtual_imports != std::string::npos) {
    // This is something like:
    // bazel-out/darwin_arm64-dbg/bin/external/com_google_protobuf/_virtual_imports/any_proto/google/protobuf/any.proto
    filename = filename.substr(virtual_imports + sizeof("_virtual_imports/"));
    // Remove the first directory.
    filename = filename.substr(filename.find('/') + 1);
  }
  return package_name / target_name / filename;
}

bool CodeGenerator::Generate(
    const google::protobuf::FileDescriptor *file, const std::string &parameter,
    google::protobuf::compiler::GeneratorContext *generator_context,
    std::string *error) const {

  // The options for the compiler are passed in the --sato_out parameter
  // as a comma separated list of key=value pairs, followed by a colon
  // and then the output directory.
  std::vector<std::pair<std::string, std::string>> options;
  google::protobuf::compiler::ParseGeneratorParameter(parameter, &options);

  for (auto option : options) {
    if (option.first == "add_namespace") {
      added_namespace_ = option.second;
    } else if (option.first == "package_name") {
      package_name_ = option.second;
    } else if (option.first == "target_name") {
      target_name_ = option.second;
    }
  }

  Generator gen(file, added_namespace_, package_name_, target_name_);

  gen.Compile();

  std::string filename =
      GeneratedFilename(package_name_, target_name_, file->name());


  // Generate ROS messages.  We would really like to generate the .msg files and use
  // the regular ROS message generator but Bazel requires that all the outputs from
  // the plubin be declared up front in the .bzl file and we don't know what the files
  // will be called until the plugin has run.
  std::filesystem::path ros_message_path(filename);
  ros_message_path.replace_extension(".h");

  std::unique_ptr<google::protobuf::io::ZeroCopyOutputStream> ros_output(
      generator_context->Open(ros_message_path.string()));

  std::filesystem::create_directories(ros_message_path.parent_path());

  if (ros_output == nullptr) {
    std::cerr << "Failed to open " << ros_message_path << " for writing\n";
    *error = absl::StrFormat("Failed to open %s for writing",
                             ros_message_path.string());
    return false;
  }

  std::stringstream ros_stream;
  gen.GenerateROSMessages(ros_stream);

  std::filesystem::path hp(filename);
  hp.replace_extension(".sato.h");
  std::cerr << "Generating " << hp << "\n";

  // There appears to be no way to get anything other than a
  // ZeorCopyOutputStream from the GeneratorContext.  We want to use
  // std::ofstream to write the file, so we'll write to a stringstream and then
  // copy the data to the file.
  std::unique_ptr<google::protobuf::io::ZeroCopyOutputStream> header_output(
      generator_context->Open(hp.string()));

  std::filesystem::create_directories(hp.parent_path());

  if (header_output == nullptr) {
    std::cerr << "Failed to open " << hp << " for writing\n";
    *error = absl::StrFormat("Failed to open %s for writing", hp.string());
    return false;
  }
  std::stringstream header_stream;
  gen.GenerateHeaders(header_stream);

  std::filesystem::path cp(filename);
  cp.replace_extension(".sato.cc");

  std::unique_ptr<google::protobuf::io::ZeroCopyOutputStream> source_output(
      generator_context->Open(cp.string()));
  if (source_output == nullptr) {
    *error = absl::StrFormat("Failed to open %s for writing", cp.string());
    return false;
  }
  std::stringstream source_stream;
  gen.GenerateSources(source_stream);

  // Write to the streams that protobuf wants
  WriteToZeroCopyStream(ros_stream.str(), ros_output.get());
  WriteToZeroCopyStream(header_stream.str(), header_output.get());
  WriteToZeroCopyStream(source_stream.str(), source_output.get());
  return true;
}

void Generator::OpenNamespace(std::ostream &os) {
  std::vector<std::string> parts = absl::StrSplit(file_->package(), '.');
  for (const auto &part : parts) {
    os << "namespace " << part << " {\n";
  }
  if (!added_namespace_.empty()) {
    os << "namespace " << added_namespace_ << " {\n";
  }
}

void Generator::CloseNamespace(std::ostream &os) {
  if (!added_namespace_.empty()) {
    os << "} // namespace " << added_namespace_ << "\n";
  }
  std::vector<std::string> parts = absl::StrSplit(file_->package(), '.');
  for (const auto &part : parts) {
    os << "} // namespace " << part << "\n";
  }
}

Generator::Generator(const google::protobuf::FileDescriptor *file,
                     const std::string &ns, const std::string &pn,
                     const std::string &tn)
    : file_(file), added_namespace_(ns), package_name_(pn), target_name_(tn) {
  for (int i = 0; i < file->message_type_count(); i++) {
    message_gens_.push_back(std::make_unique<MessageGenerator>(
        file->message_type(i), added_namespace_, file->package()));
  }
  // Enums
  for (int i = 0; i < file->enum_type_count(); i++) {
    enum_gens_.push_back(std::make_unique<EnumGenerator>(file->enum_type(i)));
  }
}

void Generator::GenerateROSMessages(std::ostream &os) {
  for (auto &msg_gen : message_gens_) {
    msg_gen->GenerateROSMessage(os);
  }
}

void Generator::Compile() {
  for (auto &msg_gen : message_gens_) {
    msg_gen->Compile();
  }
}

void Generator::GenerateHeaders(std::ostream &os) {
  os << "#pragma once\n";
  os << "#include \"sato/runtime/runtime.h\"\n";
  for (int i = 0; i < file_->dependency_count(); i++) {
    std::string base = GeneratedFilename(package_name_, target_name_,
                                         file_->dependency(i)->name());
    std::filesystem::path p(base);
    p.replace_extension(".sato.h");
    os << "#include \"" << p.string() << "\"\n";
  }

  OpenNamespace(os);

  // Enums
  for (auto &enum_gen : enum_gens_) {
    enum_gen->GenerateHeader(os);
  }

  for (auto &msg_gen : message_gens_) {
    msg_gen->GenerateEnums(os);
  }

  for (auto &msg_gen : message_gens_) {
    msg_gen->GenerateHeader(os);
  }

  CloseNamespace(os);
}

void Generator::GenerateSources(std::ostream &os) {
  std::filesystem::path p(
      GeneratedFilename(package_name_, target_name_, file_->name()));
  p.replace_extension(".sato.h");
  os << "#include \"" << p.string() << "\"\n";

  OpenNamespace(os);

  for (auto &msg_gen : message_gens_) {
    msg_gen->GenerateSource(os);
  }

  CloseNamespace(os);
}
} // namespace sato
