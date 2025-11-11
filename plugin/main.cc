#include <google/protobuf/compiler/plugin.h>
#include "plugin/ros_generator.h"

int main(int argc, char* argv[]) {
  sato::RosGenerator generator;
  return google::protobuf::compiler::PluginMain(argc, argv, &generator);
}
