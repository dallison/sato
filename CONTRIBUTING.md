# Contributing to Sato

Thank you for your interest in contributing to Sato!

## Building the Project

### Prerequisites

- Bazel 6.0.0 or higher
- C++ compiler with C++14 support
- Git

### Build Commands

```bash
# Build the plugin
bazel build //plugin:protoc-gen-ros

# Build all targets
bazel build //...

# Run tests
bazel test //...

# Build examples
bazel build //examples:robot_ros
```

## Development Workflow

### 1. Setting Up

```bash
git clone https://github.com/dallison/sato.git
cd sato
```

### 2. Making Changes

- Create a feature branch: `git checkout -b feature/my-feature`
- Make your changes
- Build and test: `bazel build //... && bazel test //...`

### 3. Testing Your Changes

Add a test proto file in `examples/` or `test/`:

```protobuf
syntax = "proto3";

message MyTest {
  int32 value = 1;
}
```

Add a corresponding BUILD target:

```starlark
proto_ros_gen(
    name = "my_test_ros",
    proto = "my_test.proto",
)
```

Build and verify the generated code:

```bash
bazel build //examples:my_test_ros
```

### 4. Code Style

- Follow Google C++ Style Guide
- Use 2-space indentation
- Keep lines under 80 characters where practical
- Add comments for complex logic

### 5. Submitting Changes

- Commit your changes with clear messages
- Push to your fork
- Create a pull request

## Areas for Contribution

### High Priority

1. **Enum Support**: Add support for protobuf enum types
2. **Nested Messages**: Improve handling of nested message types
3. **Oneof Support**: Add support for oneof fields
4. **Tests**: Add comprehensive unit tests for the generator

### Medium Priority

1. **Error Handling**: Improve error messages and validation
2. **Documentation**: Expand examples and tutorials
3. **Performance**: Optimize generated converter code
4. **CI/CD**: Set up continuous integration

### Low Priority

1. **Python Bindings**: Generate Python ROS bindings
2. **Custom Annotations**: Support custom protobuf annotations
3. **ROS 2 Integration**: Direct ROS 2 IDL integration

## Code Structure

```
sato/
├── plugin/          # Protobuf compiler plugin
│   ├── ros_generator.h
│   ├── ros_generator.cc
│   ├── main.cc
│   └── BUILD
├── bazel/           # Bazel rules and extensions
│   ├── proto_ros.bzl
│   └── BUILD
├── examples/        # Example proto files and usage
├── test/            # Tests
└── WORKSPACE        # Bazel workspace configuration
```

## Testing Guidelines

### Unit Tests

Tests should:
- Be self-contained
- Test one thing at a time
- Have clear assertions
- Include both positive and negative cases

### Integration Tests

- Test complete proto files
- Verify generated code compiles
- Test converter functionality end-to-end

## Getting Help

- Open an issue for bugs or feature requests
- Start a discussion for questions or ideas
- Check existing issues before creating new ones

## License

By contributing, you agree that your contributions will be licensed under the same license as the project (see LICENSE file).
