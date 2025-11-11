# Nested Messages Handling

## Current Implementation

Sato currently supports nested messages with the following behavior:

### Simple Nested Messages

For a protobuf definition like:

```protobuf
message Point {
  double x = 1;
  double y = 2;
}

message Location {
  string name = 1;
  Point position = 2;
}
```

Sato generates:

```cpp
struct Point {
  double x;
  double y;
};

struct Location {
  string name;
  Point position;  // Nested struct
};
```

### Converter Behavior

The converter handles nested messages by direct assignment:

```cpp
// ProtoToRos
ros_msg->position = proto_msg.position();

// RosToProto  
proto_msg.set_position(ros_msg.position);
```

This works because:
1. Each message type gets its own ROS struct definition
2. Protobuf's accessors return/accept the nested message type
3. C++ allows assignment between compatible types

### Repeated Nested Messages

For repeated nested messages:

```protobuf
message Path {
  repeated Point waypoints = 1;
}
```

Generated code:

```cpp
struct Path {
  std::vector<Point> waypoints;
};

// Converter
ros_msg->waypoints.clear();
for (int i = 0; i < proto_msg.waypoints_size(); ++i) {
  ros_msg->waypoints.push_back(proto_msg.waypoints(i));
}
```

## Limitations

### Cross-File Dependencies

Currently, nested messages must be defined in the same .proto file. Cross-file dependencies require:

1. Including the generated headers for dependency messages
2. Properly ordering the generation

Example workaround:

```starlark
# For message A depending on message B in different file
proto_ros_gen(
    name = "message_b_ros",
    proto = "message_b.proto",
)

proto_ros_gen(
    name = "message_a_ros", 
    proto = "message_a.proto",
    # This would need to depend on message_b_ros
)
```

### Deep Nesting

Deeply nested messages work but require careful header inclusion:

```cpp
// All headers must be included
#include "OuterMessage_ros.h"
#include "MiddleMessage_ros.h"  
#include "InnerMessage_ros.h"
```

## Best Practices

1. **Keep messages in the same file** when they reference each other
2. **Use forward declarations** when possible
3. **Test nested conversions** thoroughly
4. **Document dependencies** in BUILD files

## Future Enhancements

Potential improvements for nested message handling:

1. Automatic header inclusion in generated code
2. Cross-file dependency resolution
3. Recursive converter generation for complex nesting
4. Validation of nested message compatibility

## Example Usage

See `examples/nested.proto` for working examples of:
- Simple nested messages
- Repeated nested messages  
- Multiple levels of nesting
