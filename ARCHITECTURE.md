# Sato Architecture

## Overview

Sato is a protobuf compiler plugin that generates ROS-compatible struct definitions and bidirectional converters between protobuf and ROS message formats.

## Components

### 1. Protobuf Compiler Plugin (`plugin/`)

The core of Sato is a protobuf compiler plugin built using the protobuf compiler API.

#### Files:
- `ros_generator.h` / `ros_generator.cc`: Core code generator that processes protobuf descriptors
- `main.cc`: Plugin entry point that registers with protoc
- `BUILD`: Bazel build configuration for the plugin

#### How it works:
1. The plugin receives a `FileDescriptor` from protoc
2. It iterates through all message types in the proto file
3. For each message, it generates:
   - A ROS struct with equivalent fields
   - A converter class with `ProtoToRos()` and `RosToProto()` methods

### 2. Bazel Extension (`bazel/proto_ros.bzl`)

A Starlark rule that integrates the plugin into Bazel builds.

#### Key Functions:
- `proto_ros_library`: Bazel rule that runs the plugin
- `proto_ros_gen`: Convenience macro for users

#### How it works:
1. Takes a `.proto` file as input
2. Invokes protoc with the `protoc-gen-ros` plugin
3. Outputs generated `.h` and `.cc` files
4. Makes them available as build dependencies

### 3. Type Mapping

The generator maps protobuf types to C++/ROS types:

| Protobuf | ROS/C++ | Notes |
|----------|---------|-------|
| double   | double  | Direct mapping |
| float    | float   | Direct mapping |
| int32    | int32_t | Fixed-width integer |
| int64    | int64_t | Fixed-width integer |
| uint32   | uint32_t | Fixed-width integer |
| uint64   | uint64_t | Fixed-width integer |
| bool     | bool    | Direct mapping |
| string   | std::string | Direct mapping |
| bytes    | std::string | Binary data |
| repeated T | std::vector<T> | Array type |

### 4. Generated Code Structure

For a protobuf message:
```protobuf
message Point {
  double x = 1;
  double y = 2;
  double z = 3;
}
```

Generates:
```cpp
// Point_ros.h
struct Point {
  double x;
  double y;
  double z;
};

class PointConverter {
 public:
  static bool ProtoToRos(const std::string& proto_data, Point* ros_msg);
  static bool RosToProto(const Point& ros_msg, std::string* proto_data);
};

// Point_ros.cc
bool PointConverter::ProtoToRos(const std::string& proto_data, Point* ros_msg) {
  // Parse protobuf, populate ROS struct
}

bool PointConverter::RosToProto(const Point& ros_msg, std::string* proto_data) {
  // Populate protobuf from ROS struct, serialize
}
```

## Build Flow

```
.proto file
    |
    v
proto_ros_gen rule
    |
    v
protoc + protoc-gen-ros plugin
    |
    v
Generated .h and .cc files
    |
    v
Available as build dependencies
```

## Usage Pattern

1. Define your message in protobuf
2. Add `proto_ros_gen` target in BUILD file
3. Build generates ROS structs and converters
4. Use in your application:
   ```cpp
   #include "Message_ros.h"
   
   // Deserialize protobuf to ROS
   std::string proto_bytes = /* ... */;
   sato::ros::Message ros_msg;
   MessageConverter::ProtoToRos(proto_bytes, &ros_msg);
   
   // Serialize ROS to protobuf
   std::string proto_bytes;
   MessageConverter::RosToProto(ros_msg, &proto_bytes);
   ```

## Extension Points

The architecture is designed to be extensible:

1. **Custom Type Mappings**: Modify `GetROSType()` to support custom types
2. **Additional Generators**: Create new generator classes for different output formats
3. **Custom Converters**: Extend converter generation for specialized needs

## Dependencies

- **Protobuf**: Core protobuf library and compiler API
- **Bazel**: Build system
- **Abseil**: C++ utilities (required by protobuf)

## Future Enhancements

Potential areas for expansion:
- Support for nested messages (partially implemented)
- Support for enum types
- Support for oneof fields
- Custom ROS message annotations
- Integration with ROS 2 IDL
- Python bindings
- Validation and error handling improvements
