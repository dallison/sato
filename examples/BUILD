load("@rules_proto//proto:defs.bzl", "proto_library")
load("@rules_cc//cc:defs.bzl", "cc_binary")
load("//bazel:proto_ros.bzl", "proto_ros_gen")

proto_library(
    name = "robot_proto",
    srcs = ["robot.proto"],
)

proto_ros_gen(
    name = "robot_ros",
    proto = "robot.proto",
)

proto_library(
    name = "nested_proto",
    srcs = ["nested.proto"],
)

proto_ros_gen(
    name = "nested_ros",
    proto = "nested.proto",
)

# Example demonstrating usage of the generated converters
cc_binary(
    name = "usage_example",
    srcs = ["usage_example.cc"],
    deps = [],
)
