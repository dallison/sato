load("@rules_proto//proto:defs.bzl", "proto_library")
load("//bazel:proto_ros.bzl", "proto_ros_gen")

proto_library(
    name = "robot_proto",
    srcs = ["robot.proto"],
)

proto_ros_gen(
    name = "robot_ros",
    proto = "robot.proto",
)
