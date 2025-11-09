# Quick Start Guide

This guide will help you get started with Sato in 5 minutes.

## Step 1: Add Sato to Your WORKSPACE

```starlark
load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")

git_repository(
    name = "sato",
    remote = "https://github.com/dallison/sato.git",
    branch = "main",
)
```

## Step 2: Create a Protobuf Message

Create `my_message.proto`:

```protobuf
syntax = "proto3";

package myapp;

message Sensor {
  string name = 1;
  double temperature = 2;
  double humidity = 3;
  int32 battery_percent = 4;
  bool is_active = 5;
}
```

## Step 3: Add Build Rules

Create or edit your `BUILD` file:

```starlark
load("@sato//bazel:proto_ros.bzl", "proto_ros_gen")
load("@rules_proto//proto:defs.bzl", "proto_library")

# Standard protobuf library
proto_library(
    name = "sensor_proto",
    srcs = ["my_message.proto"],
)

# Generate ROS structs and converters
proto_ros_gen(
    name = "sensor_ros",
    proto = "my_message.proto",
)
```

## Step 4: Build

```bash
bazel build //:sensor_ros
```

This generates:
- `Sensor_ros.h` - ROS struct definition and converter class
- `Sensor_ros.cc` - Converter implementation

## Step 5: Use in Your Code

```cpp
#include "Sensor_ros.h"

int main() {
  // Create a ROS message
  sato::ros::Sensor sensor;
  sensor.name = "temp_sensor_01";
  sensor.temperature = 23.5;
  sensor.humidity = 45.2;
  sensor.battery_percent = 87;
  sensor.is_active = true;
  
  // Convert to protobuf
  std::string proto_bytes;
  if (sato::ros::SensorConverter::RosToProto(sensor, &proto_bytes)) {
    // Send proto_bytes over network, save to file, etc.
    std::cout << "Serialized " << proto_bytes.size() << " bytes\n";
  }
  
  // Convert from protobuf
  sato::ros::Sensor received_sensor;
  if (sato::ros::SensorConverter::ProtoToRos(proto_bytes, &received_sensor)) {
    std::cout << "Received: " << received_sensor.name << "\n";
    std::cout << "Temperature: " << received_sensor.temperature << "°C\n";
  }
  
  return 0;
}
```

## That's It!

You now have:
- ✅ ROS-compatible structs generated from protobuf
- ✅ Bidirectional converters for serialization
- ✅ Type-safe message definitions
- ✅ Automatic build integration

## Next Steps

- Check out the [examples/](examples/) directory for more complex messages
- Read [ARCHITECTURE.md](ARCHITECTURE.md) to understand how it works
- See [CONTRIBUTING.md](CONTRIBUTING.md) to extend the plugin

## Common Use Cases

### ROS 1 to ROS 2 Migration
Use Sato to bridge ROS 1 and ROS 2 systems using protobuf as an intermediate format.

### Cross-Language Communication
Protobuf supports many languages - use Sato to interface C++ ROS nodes with Python, Go, or Java services.

### Data Logging
Serialize ROS messages to protobuf for efficient storage and replay.

### Network Communication
Send ROS messages over network protocols that expect protobuf format.
