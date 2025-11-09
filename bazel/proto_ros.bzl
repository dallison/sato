"""Bazel rules for generating ROS structs from protobuf definitions."""

def _proto_ros_library_impl(ctx):
    """Implementation of the proto_ros_library rule."""
    
    proto_src = ctx.file.proto
    plugin = ctx.executable._plugin
    
    # Output files
    name = ctx.attr.name
    header_file = ctx.actions.declare_file(name + "_ros.h")
    source_file = ctx.actions.declare_file(name + "_ros.cc")
    
    # Get protoc compiler
    protoc = ctx.executable._protoc
    
    # Build arguments
    proto_path = proto_src.dirname
    
    args = ctx.actions.args()
    args.add("--plugin=protoc-gen-ros=" + plugin.path)
    args.add("--ros_out=" + header_file.dirname)
    args.add("--proto_path=" + proto_path)
    args.add(proto_src.path)
    
    ctx.actions.run(
        inputs = [proto_src],
        outputs = [header_file, source_file],
        executable = protoc,
        arguments = [args],
        tools = [plugin],
        mnemonic = "ProtoRosGen",
        progress_message = "Generating ROS structs from %s" % proto_src.short_path,
    )
    
    return [
        DefaultInfo(files = depset([header_file, source_file])),
    ]

proto_ros_library = rule(
    implementation = _proto_ros_library_impl,
    attrs = {
        "proto": attr.label(
            allow_single_file = [".proto"],
            mandatory = True,
            doc = "The .proto file to generate ROS structs from",
        ),
        "_plugin": attr.label(
            default = Label("//plugin:protoc-gen-ros"),
            executable = True,
            cfg = "exec",
        ),
        "_protoc": attr.label(
            default = Label("@com_google_protobuf//:protoc"),
            executable = True,
            cfg = "exec",
        ),
    },
    doc = "Generates ROS struct definitions and converters from a protobuf file.",
)

def proto_ros_gen(name, proto, **kwargs):
    """
    Generates ROS struct and converter code from a protobuf definition.
    
    This macro creates a proto_ros_library target that generates:
    - A ROS struct definition matching the protobuf message
    - Converter functions to convert between serialized protobuf and ROS structs
    
    Args:
        name: Name of the target
        proto: The .proto file to process
        **kwargs: Additional arguments passed to the rule
    
    Example:
        proto_ros_gen(
            name = "my_message_ros",
            proto = "my_message.proto",
        )
    """
    proto_ros_library(
        name = name,
        proto = proto,
        **kwargs
    )
