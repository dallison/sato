// Copyright 2025 David Allison
// All Rights Reserved
// See LICENSE file for licensing information.

#include "sato/compiler/gen.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <unistd.h>
#include <vector>

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


  // Generate ROS messages. These are generated as a zip file because Bazel requires that all the outputs from
  // the plubin be declared up front in the .bzl file and we don't know what the files
  // will be called until the plugin has run.
  std::filesystem::path ros_message_path(filename);
  ros_message_path.replace_extension(".zip");

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
  gen.GenerateROSMessagesZip(ros_stream);

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

void Generator::GenerateROSMessagesZip(std::ostream &os) {
  // Create a single zip file and add all top-level messages (and their nested messages) to it
  int error;

  // Create a temporary file
  char tmp_file_template[] = "/tmp/sato_ros_messages_XXXXXX";
  int fd = mkstemp(tmp_file_template);
  if (fd < 0) {
    std::cerr << "Failed to create temporary file" << std::endl;
    return;
  }
  close(fd);
  std::string tmp_file(tmp_file_template);

  // Open a zip archive for writing to the temporary file
  zip_t *arc = zip_open(tmp_file.c_str(), ZIP_CREATE | ZIP_TRUNCATE, &error);
  if (arc == nullptr) {
    std::cerr << "Failed to open zip archive (error code: " << error << ")" << std::endl;
    std::remove(tmp_file.c_str());
    return;
  }

  // Generate all ROS messages in the zip
  for (auto &enum_gen : enum_gens_) {
    enum_gen->GenerateROSMessage(arc);
  }

  for (auto &msg_gen : message_gens_) {
    msg_gen->GenerateROSMessage(arc);
  }

  // Close the archive (this finalizes the zip structure)
  if (zip_close(arc) < 0) {
    std::cerr << "Failed to close zip archive." << std::endl;
    std::remove(tmp_file.c_str());
    return;
  }

  // Read the temporary file and write to the output stream
  std::ifstream tmp_stream(tmp_file, std::ios::binary);
  if (!tmp_stream) {
    std::cerr << "Failed to open temporary file for reading" << std::endl;
    std::remove(tmp_file.c_str());
    return;
  }

  // Read the entire file and write to output stream
  tmp_stream.seekg(0, std::ios::end);
  std::streamsize file_size = tmp_stream.tellg();
  tmp_stream.seekg(0, std::ios::beg);

  if (file_size > 0) {
    std::vector<char> buffer(file_size);
    tmp_stream.read(buffer.data(), file_size);
    if (tmp_stream.gcount() == file_size) {
      os.write(buffer.data(), file_size);
    } else {
      std::cerr << "WARNING: Failed to read entire file (read " << tmp_stream.gcount() 
                << " of " << file_size << " bytes)" << std::endl;
    }
  } else {
    std::cerr << "WARNING: Zip file is empty" << std::endl;
  }
  
  tmp_stream.close();

  // Clean up temporary file
  std::remove(tmp_file.c_str());
}

void Generator::Compile() {
  for (auto &msg_gen : message_gens_) {
    msg_gen->Compile();
  }
}

void Generator::GenerateHeaders(std::ostream &os) {
  os << "#pragma once\n";
  os << "#include \"sato/runtime/runtime.h\"\n";
  os << "#include \"sato/runtime/message.h\"\n";
  for (int i = 0; i < file_->dependency_count(); i++) {
    std::string base = GeneratedFilename(package_name_, target_name_,
                                         file_->dependency(i)->name());
    std::filesystem::path p(base);
    p.replace_extension(".sato.h");
    os << "#include \"" << p.string() << "\"\n";
  }

  OpenNamespace(os);

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
