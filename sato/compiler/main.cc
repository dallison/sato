// This is heavily based on Phaser (https://github.com/dallison/phaser) and
// Neutron (https://github.com/dallison/neutron).
// Copyright (C) 2025 David Allison.  All Rights Reserved.

#include "google/protobuf/compiler/code_generator.h"
#include "google/protobuf/compiler/plugin.h"
#include "sato/compiler/gen.h"


int main(int argc, char *argv[]) {
  sato::CodeGenerator generator;
  return google::protobuf::compiler::PluginMain(argc, argv, &generator);
}