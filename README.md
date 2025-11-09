# Sato - Protobuf to ROS Converter

Sato is a protobuf compiler plugin built using Bazel that generates ROS structs from protobuf IDL files and provides converters between serialized protobuf and serialized ROS messages.

## Features

- **Automatic Code Generation**: Generates ROS struct definitions from protobuf messages
- **Bidirectional Conversion**: Converts between serialized protobuf and ROS formats
- **Bazel Integration**: Easy-to-use Starlark extension for Bazel builds
- **Type Mapping**: Automatic mapping between protobuf and ROS types

## Building

This project requires Bazel 6.0.0 or higher.

```bash
# Build the plugin
bazel build //plugin:protoc-gen-ros

# Build the example
bazel build //examples:robot_ros
```

## Usage

### In your WORKSPACE file

```starlark
load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")

git_repository(
    name = "sato",
    remote = "https://github.com/dallison/sato.git",
    branch = "main",
)
```

### In your BUILD file

```starlark
load("@sato//bazel:proto_ros.bzl", "proto_ros_gen")
load("@rules_proto//proto:defs.bzl", "proto_library")

proto_library(
    name = "my_message_proto",
    srcs = ["my_message.proto"],
)

proto_ros_gen(
    name = "my_message_ros",
    proto = "my_message.proto",
)
```

### Generated Code

For a protobuf message like:

```protobuf
syntax = "proto3";

package example;

message Point {
  double x = 1;
  double y = 2;
  double z = 3;
}
```

Sato generates:

1. **ROS Struct** (`Point_ros.h`):
```cpp
namespace sato {
namespace ros {

struct Point {
  double x;
  double y;
  double z;
};

class PointConverter {
 public:
  // Convert from protobuf to ROS
  static bool ProtoToRos(const std::string& proto_data, Point* ros_msg);
  
  // Convert from ROS to protobuf
  static bool RosToProto(const Point& ros_msg, std::string* proto_data);
};

}  // namespace ros
}  // namespace sato
```

2. **Converter Implementation** (`Point_ros.cc`): Complete implementation of conversion functions

### Using the Converter

```cpp
#include "Point_ros.h"

// Convert protobuf to ROS
std::string serialized_proto = /* ... */;
sato::ros::Point ros_message;
if (sato::ros::PointConverter::ProtoToRos(serialized_proto, &ros_message)) {
  // Use ros_message
}

// Convert ROS to protobuf
sato::ros::Point ros_message;
ros_message.x = 1.0;
ros_message.y = 2.0;
ros_message.z = 3.0;

std::string serialized_proto;
if (sato::ros::PointConverter::RosToProto(ros_message, &serialized_proto)) {
  // Use serialized_proto
}
```

## Supported Types

| Protobuf Type | ROS/C++ Type |
|---------------|--------------|
| double        | double       |
| float         | float        |
| int32/sint32  | int32_t      |
| int64/sint64  | int64_t      |
| uint32        | uint32_t     |
| uint64        | uint64_t     |
| bool          | bool         |
| string        | std::string  |
| bytes         | std::string  |
| repeated T    | std::vector<T> |

## Example

See the `examples/` directory for a complete example with a `RobotState` message including nested messages and repeated fields.

## License

See LICENSE file for details.
