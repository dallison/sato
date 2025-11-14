// Copyright 2024 David Allison
// All Rights Reserved
// See LICENSE file for licensing information.

#include "google/protobuf/compiler/code_generator.h"
#include "google/protobuf/compiler/plugin.h"
#include "sato/compiler/gen.h"


int main(int argc, char *argv[]) {
  sato::CodeGenerator generator;
  return google::protobuf::compiler::PluginMain(argc, argv, &generator);
}