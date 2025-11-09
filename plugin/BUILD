load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")

cc_library(
    name = "ros_generator",
    srcs = ["ros_generator.cc"],
    hdrs = ["ros_generator.h"],
    deps = [
        "@com_google_protobuf//:protobuf",
        "@com_google_protobuf//:protoc_lib",
    ],
    visibility = ["//visibility:public"],
)

cc_binary(
    name = "protoc-gen-ros",
    srcs = ["main.cc"],
    deps = [
        ":ros_generator",
        "@com_google_protobuf//:protobuf",
        "@com_google_protobuf//:protoc_lib",
    ],
    visibility = ["//visibility:public"],
)
