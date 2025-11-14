load("//sato:sato_library.bzl", "sato_library")

package(default_visibility = ["//visibility:public"])

proto_library(
    name = "foo_proto",
    srcs = [
        "Foo.proto",
    ],
)

proto_library(
    name = "test_proto",
    srcs = [
        "Test.proto",
    ],
    deps = [
        ":foo_proto",
        #"@com_google_protobuf//:any_proto",
    ],
)

cc_proto_library(
    name = "test_cc_proto",
    deps = [":test_proto"],
)

proto_library(
    name = "test_message_proto",
    srcs = [
        "TestMessage.proto",
    ],
    deps = [
        # "@com_google_protobuf//:any_proto",
    ],
)

cc_proto_library(
    name = "test_message_cc_proto",
    deps = [":test_message_proto"],
)

sato_library(
    name = "test_message_sato",
    add_namespace = "sato",
    runtime = "//sato/runtime:sato_runtime",
    deps = [":test_message_proto"],
)

sato_library(
    name = "test_sato",
    add_namespace = "sato",
    runtime = "//sato/runtime:sato_runtime",
    deps = [":test_proto"],
)

proto_library(
    name = "vision_proto",
    srcs = ["vision.proto"],
)

cc_proto_library(
    name = "vision_cc_proto",
    deps = [":vision_proto"],
)

sato_library(
    name = "vision_sato",
    add_namespace = "sato",
    runtime = "//sato/runtime:sato_runtime",
    deps = [":vision_proto"],
)

