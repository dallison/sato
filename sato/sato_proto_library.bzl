"""
This module provides a rule to generate sato message files from proto_library targets.
"""

load("@bazel_skylib//lib:paths.bzl", "paths")

MessageInfo = provider(fields = ["direct_sources", "transitive_sources", "cpp_outputs"])

def _sato_action(
        ctx,
        direct_sources,
        transitive_sources,
        out_dir,
        package_name,
        outputs,
        add_namespace,
        target_name):
    # The protobuf compiler allow plugins to get arguments specified in the --plugin_out
    # argument.  The args are passed as a comma separated list of key=value pairs followed
    # by a colon and the output directory.
    options_and_out_dir = ""
    if add_namespace != "":
        options_and_out_dir = "--sato_out=add_namespace={},package_name={},target_name={}:{}".format(add_namespace, package_name, target_name, out_dir)
    else:
        options_and_out_dir = "--sato_out=package_name={},target_name={}:{}".format(package_name, target_name, out_dir)

    inputs = depset(direct = direct_sources, transitive = transitive_sources)

    import_paths = []
    for s in transitive_sources:
        for f in s.to_list():
            if not f.is_source:
                index = f.path.find("_virtual_imports")
                if index != -1:
                    # Go to first slash after _virtual_imports/
                    slash = f.path.find("/", index + 17)
                    import_paths.append("-I" + f.path[:slash])

    plugin, _, plugin_manifests = ctx.resolve_command(tools = [ctx.attr.sato_plugin])
    plugin_arg = "--plugin=protoc-gen-sato={}".format(ctx.executable.sato_plugin.path)

    args = ctx.actions.args()
    args.add(plugin_arg)
    args.add(options_and_out_dir)
    args.add_all(inputs)
    args.add_all(import_paths)
    args.add("-I.")

    ctx.actions.run(
        inputs = inputs,
        tools = plugin,
        input_manifests = plugin_manifests,
        executable = ctx.executable.protoc,
        outputs = outputs,
        arguments = [args],
        progress_message = "Generating sato message files %s" % ctx.label,
        mnemonic = "Phaser",
    )

# This aspect generates the MessageInfo provider containing the files we
# will generate from running the Phaser plugin.
def _sato_aspect_impl(target, _ctx):
    direct_sources = []
    transitive_sources = depset()
    cpp_outputs = []

    def add_output(base):
        cpp_outputs.append(paths.replace_extension(base, ".sato.cc"))
        cpp_outputs.append(paths.replace_extension(base, ".sato.h"))
        cpp_outputs.append(paths.replace_extension(base, ".zip"))

    if ProtoInfo in target:
        transitive_sources = target[ProtoInfo].transitive_sources
        for s in transitive_sources.to_list():
            direct_sources.append(s)
            file_path = s.short_path
            if "_virtual_imports" in file_path:
                # For a file that is not in this package, we need to generate the
                # output in our package.
                # The path looks like:
                # ../com_google_protobuf/_virtual_imports/any_proto/google/protobuf/any.proto
                # We want to declare the file as:ƒ
                # google/protobuf/any.sato.cc
                v = file_path.split("_virtual_imports/")

                # Remove the first directory of v[1] to get the path relative to the package.
                file_path = v[1].split("/", 1)[1]
            add_output(file_path)

    return [MessageInfo(
        direct_sources = direct_sources,
        transitive_sources = transitive_sources,
        cpp_outputs = cpp_outputs,
    )]

sato_aspect = aspect(
    attr_aspects = ["deps"],
    provides = [MessageInfo],
    implementation = _sato_aspect_impl,
)

# The sato rule runs the Sato plugin from the protoc compiler.
# The deps for the rule are proto_libraries that contain the protobuf files.
def _sato_impl(ctx):
    outputs = []
    zip_files = []
  
    direct_sources = []
    transitive_sources = []
    cpp_outputs = []
    package_name = ctx.attr.package_name
    for dep in ctx.attr.deps:
        dep_outs = []
        for out in dep[MessageInfo].cpp_outputs:
            out_name = ctx.attr.target_name + "/" + out
            out_file = ctx.actions.declare_file(out_name)
            dep_outs.append(out_file)

            # Track zip files separately for unzipping
            if out_file.extension == "zip":
                zip_files.append(out_file)
            else:
                # If we are creating a header file in our package, we need to create a symlink to it.
                # This is because the header file will be something like
                # sato/testdata/sato/testdata/Test.sato.h
                # but we want to be able to do:
                # #include "sato/testdata/Test.sato.h"
                # so we create the symlink:
                # Test.sato.h -> sato/testdata/sato/testdata/Test.sato.h
                if out_file.extension == "h":
                    prefix = paths.join(ctx.attr.target_name, package_name)
                    symlink_name = out_file.short_path[len(prefix) + 1:]
                    if symlink_name.startswith(package_name):
                        # Header is in our package, remove the package name.
                        # If the header is outside our package (like google/protobuf/any.h),
                        # we don't want to create a symlink to it becuase it's in
                        # the right place already.
                        symlink_name = symlink_name[len(package_name) + 1:]
                        symlink = ctx.actions.declare_file(symlink_name)
                        ctx.actions.symlink(output = symlink, target_file = out_file)
                        dep_outs.append(symlink)
                cpp_outputs.append(out_file)

        direct_sources += dep[MessageInfo].direct_sources
        transitive_sources.append(dep[MessageInfo].transitive_sources)
        outputs += dep_outs

    # Include zip files in the outputs for _sato_action to create them
    all_outputs = cpp_outputs + zip_files

    _sato_action(
        ctx,
        direct_sources,
        transitive_sources,
        ctx.bin_dir.path,
        ctx.attr.package_name,
        all_outputs,
        ctx.attr.add_namespace,
        ctx.attr.target_name,
    )

    # Unzip zip files to a "proto_msgs" directory
    # This must run AFTER _sato_action creates the zip files
    # Place the msg directory in the same location as the .sato.cc files
    # The .sato.cc files are at target_name/package_path/file.sato.cc
    # Find the directory from the first output to determine where to place msg
    msg_dir_path = None
    for dep in ctx.attr.deps:
        for out in dep[MessageInfo].cpp_outputs:
            # out is like "sato/testdata/TestMessage.sato.cc"
            # We want the directory part: "sato/testdata"
            if out.endswith(".sato.cc"):
                # Get the directory part of the path
                last_slash = out.rfind("/")
                if last_slash != -1:
                    out_dir = out[:last_slash]
                    # msg directory should be at target_name/out_dir/msg
                    msg_dir_path = ctx.attr.target_name + "/" + out_dir + "/proto_msgs"
                else:
                    # File is in root, msg should be at target_name/msg
                    msg_dir_path = ctx.attr.target_name + "/proto_msgs"
                break
        if msg_dir_path:
            break
    
    # Fallback if no .cc files found
    if not msg_dir_path:
        msg_dir_path = ctx.attr.target_name + "/proto_msgs"
    
    msg_dir = ctx.actions.declare_directory(msg_dir_path)
    outputs.append(msg_dir)
    
    if zip_files:
        # Create a script to unzip files
        # The script will be executed with: script output_dir zip1 zip2 ...
        script_content = "#!/bin/bash\nset -e\n"
        script_content += "OUTPUT_DIR=\"$1\"\n"
        script_content += "shift\n"  # Remove first argument, leaving only zip files
        script_content += "mkdir -p \"${OUTPUT_DIR}\"\n"
        script_content += "for zipfile in \"$@\"; do\n"
        script_content += "  if [ -f \"${zipfile}\" ]; then\n"
        script_content += "    unzip -q -o \"${zipfile}\" -d \"${OUTPUT_DIR}\"\n"
        script_content += "  else\n"
        script_content += "    echo \"Warning: zip file ${zipfile} not found\" >&2\n"
        script_content += "    exit 1\n"
        script_content += "  fi\n"
        script_content += "done\n"
        
        script_file = ctx.actions.declare_file(ctx.attr.target_name + "_unzip.sh")
        ctx.actions.write(script_file, script_content, is_executable = True)
        
        # Use run_shell since the script is generated in the same rule
        # Build the command: script output_dir zip1 zip2 ...
        command = "\"{}\" \"{}\"".format(script_file.path, msg_dir.path)
        for zip_file in zip_files:
            command += " \"{}\"".format(zip_file.path)
        
        ctx.actions.run_shell(
            inputs = zip_files + [script_file],
            outputs = [msg_dir],
            command = command,
            progress_message = "Unzipping ROS messages to msg directory for %s" % ctx.label,
            mnemonic = "Unzip",
        )
    else:
        # No zip files, just create empty directory
        ctx.actions.run_shell(
            inputs = [],
            outputs = [msg_dir],
            command = "mkdir -p \"{}\"".format(msg_dir.path),
            progress_message = "Creating empty msg directory for %s" % ctx.label,
            mnemonic = "Mkdir",
        )

    return [DefaultInfo(files = depset(outputs))]

_sato_gen = rule(
    attrs = {
        "protoc": attr.label(
            executable = True,
            default = Label("@com_google_protobuf//:protoc"),
            cfg = "exec",
        ),
        "sato_plugin": attr.label(
            executable = True,
            default = Label("//sato/compiler:sato"),
            cfg = "exec",
        ),
        "deps": attr.label_list(
            aspects = [sato_aspect],
        ),
        "add_namespace": attr.string(),
        "package_name": attr.string(),
        "target_name": attr.string(),
    },
    implementation = _sato_impl,
)

def _split_files_impl(ctx):
    files = []
    for file in ctx.files.deps:
        if file.extension == ctx.attr.ext:
            files.append(file)

    return [DefaultInfo(files = depset(files))]

_split_files = rule(
    attrs = {
        "deps": attr.label_list(mandatory = True),
        "ext": attr.string(mandatory = True),
    },
    implementation = _split_files_impl,
)

# Rule to extract the msg directory from sato_gen outputs
def _extract_msg_dir_impl(ctx):
    msg_dir = None
    target_name = ctx.attr.target_name
    
    # Find the msg directory in the deps
    # The msg directory can be at target_name/msg or target_name/package_path/msg
    for file in ctx.files.deps:
        # Check if this is a directory
        if file.is_directory:
            file_path = file.path
            short_path = file.short_path if hasattr(file, 'short_path') else file_path
            
            # Check if path ends with /msg (could be target_name/msg or target_name/package/msg)
            if file_path.endswith("/msg") or short_path.endswith("/msg"):
                # Verify it's under the target_name
                if target_name in file_path or target_name in short_path:
                    msg_dir = file
                    break
    
    if msg_dir:
        return [DefaultInfo(files = depset([msg_dir]))]
    else:
        # Return empty depset if no msg directory found (e.g., if no zip files were generated)
        return [DefaultInfo(files = depset())]

_extract_msg_dir = rule(
    attrs = {
        "deps": attr.label_list(mandatory = True),
        "target_name": attr.string(mandatory = True),
    },
    implementation = _extract_msg_dir_impl,
)

def sato_proto_library(name, deps = [], runtime = "@sato//sato/runtime:sato_runtime", add_namespace = ""):
    """
    Generate a cc_libary for protobuf files specified in deps.

    Args:
        name: name
        deps: proto_libraries that contain the protobuf files
        deps: dependencies
        runtime: label for sato runtime.
        add_namespace: add given namespace to the message output
    """
    sato = name + "_sato"

    _sato_gen(
        name = sato,
        deps = deps,
        add_namespace = add_namespace,
        package_name = native.package_name(),
        target_name = name,
    )

    srcs = name + "_srcs"
    _split_files(
        name = srcs,
        ext = "cc",
        deps = [sato],
    )

    hdrs = name + "_hdrs"
    _split_files(
        name = hdrs,
        ext = "h",
        deps = [sato],
    )

    # Create a target with _msgs suffix containing all files from the msg directory
    msgs = name + "_msgs"
    _extract_msg_dir(
        name = msgs,
        deps = [sato],
        target_name = name,
    )

    libdeps = []
    for dep in deps:
        if not dep.endswith("_proto"):
            libdeps.append(dep)

    if runtime != "":
        libdeps = libdeps + [runtime]

    native.cc_library(
        name = name,
        srcs = [srcs],
        hdrs = [hdrs],
        deps = libdeps,
    )
