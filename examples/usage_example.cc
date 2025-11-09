#include <iostream>
#include <string>
#include <vector>

// This example demonstrates how the generated ROS converter would be used
// In actual usage, you would include the generated headers:
// #include "Point_ros.h"
// #include "Velocity_ros.h"
// #include "RobotState_ros.h"

namespace sato {
namespace ros {

// Example generated structs (these would be in the generated headers)
struct Point {
  double x;
  double y;
  double z;
};

struct Velocity {
  double linear_x;
  double linear_y;
  double linear_z;
  double angular_x;
  double angular_y;
  double angular_z;
};

struct RobotState {
  std::string name;
  Point position;
  Velocity velocity;
  int32_t battery_level;
  bool is_active;
  std::vector<std::string> sensors;
};

// Example generated converters (these would be in the generated source)
class RobotStateConverter {
 public:
  static bool ProtoToRos(const std::string& proto_data, RobotState* ros_msg) {
    // Mock implementation - actual would parse protobuf
    if (ros_msg == nullptr) return false;
    
    ros_msg->name = "robot_1";
    ros_msg->position.x = 1.0;
    ros_msg->position.y = 2.0;
    ros_msg->position.z = 0.5;
    ros_msg->velocity.linear_x = 0.5;
    ros_msg->velocity.linear_y = 0.0;
    ros_msg->velocity.linear_z = 0.0;
    ros_msg->velocity.angular_x = 0.0;
    ros_msg->velocity.angular_y = 0.0;
    ros_msg->velocity.angular_z = 0.1;
    ros_msg->battery_level = 85;
    ros_msg->is_active = true;
    ros_msg->sensors = {"camera", "lidar", "imu"};
    
    return true;
  }
  
  static bool RosToProto(const RobotState& ros_msg, std::string* proto_data) {
    // Mock implementation - actual would serialize to protobuf
    if (proto_data == nullptr) return false;
    *proto_data = "mock_serialized_protobuf_data";
    return true;
  }
};

}  // namespace ros
}  // namespace sato

void PrintRobotState(const sato::ros::RobotState& state) {
  std::cout << "Robot State:\n";
  std::cout << "  Name: " << state.name << "\n";
  std::cout << "  Position: (" << state.position.x << ", " 
            << state.position.y << ", " << state.position.z << ")\n";
  std::cout << "  Linear Velocity: (" << state.velocity.linear_x << ", "
            << state.velocity.linear_y << ", " << state.velocity.linear_z << ")\n";
  std::cout << "  Angular Velocity: (" << state.velocity.angular_x << ", "
            << state.velocity.angular_y << ", " << state.velocity.angular_z << ")\n";
  std::cout << "  Battery: " << state.battery_level << "%\n";
  std::cout << "  Active: " << (state.is_active ? "yes" : "no") << "\n";
  std::cout << "  Sensors: ";
  for (size_t i = 0; i < state.sensors.size(); ++i) {
    std::cout << state.sensors[i];
    if (i < state.sensors.size() - 1) std::cout << ", ";
  }
  std::cout << "\n\n";
}

int main() {
  std::cout << "=== Sato Protobuf to ROS Converter Example ===\n\n";
  
  // Scenario 1: Receive protobuf data and convert to ROS
  std::cout << "Scenario 1: Protobuf -> ROS\n";
  std::cout << "----------------------------\n";
  
  std::string received_proto = "serialized_protobuf_from_network";
  sato::ros::RobotState robot_state;
  
  if (sato::ros::RobotStateConverter::ProtoToRos(received_proto, &robot_state)) {
    std::cout << "Successfully converted protobuf to ROS struct\n";
    PrintRobotState(robot_state);
  } else {
    std::cerr << "Failed to convert protobuf to ROS\n";
    return 1;
  }
  
  // Scenario 2: Create ROS message and convert to protobuf
  std::cout << "Scenario 2: ROS -> Protobuf\n";
  std::cout << "----------------------------\n";
  
  sato::ros::RobotState outgoing_state;
  outgoing_state.name = "robot_2";
  outgoing_state.position.x = 10.0;
  outgoing_state.position.y = 5.0;
  outgoing_state.position.z = 0.0;
  outgoing_state.velocity.linear_x = 1.0;
  outgoing_state.velocity.linear_y = 0.0;
  outgoing_state.velocity.linear_z = 0.0;
  outgoing_state.velocity.angular_x = 0.0;
  outgoing_state.velocity.angular_y = 0.0;
  outgoing_state.velocity.angular_z = 0.2;
  outgoing_state.battery_level = 92;
  outgoing_state.is_active = true;
  outgoing_state.sensors.push_back("camera");
  outgoing_state.sensors.push_back("gps");
  
  std::cout << "Created ROS struct:\n";
  PrintRobotState(outgoing_state);
  
  std::string serialized_proto;
  if (sato::ros::RobotStateConverter::RosToProto(outgoing_state, &serialized_proto)) {
    std::cout << "Successfully converted ROS to protobuf\n";
    std::cout << "Serialized size: " << serialized_proto.size() << " bytes\n";
    std::cout << "Ready to send over network or save to file\n\n";
  } else {
    std::cerr << "Failed to convert ROS to protobuf\n";
    return 1;
  }
  
  // Scenario 3: Round-trip conversion
  std::cout << "Scenario 3: Round-trip (ROS -> Proto -> ROS)\n";
  std::cout << "---------------------------------------------\n";
  
  sato::ros::RobotState original, roundtrip;
  original.name = "test_robot";
  original.position.x = 3.14;
  original.position.y = 2.71;
  original.position.z = 1.41;
  original.battery_level = 75;
  original.is_active = false;
  
  std::cout << "Original:\n";
  PrintRobotState(original);
  
  std::string proto_bytes;
  if (sato::ros::RobotStateConverter::RosToProto(original, &proto_bytes) &&
      sato::ros::RobotStateConverter::ProtoToRos(proto_bytes, &roundtrip)) {
    std::cout << "After round-trip:\n";
    PrintRobotState(roundtrip);
    std::cout << "Round-trip conversion successful!\n";
  } else {
    std::cerr << "Round-trip conversion failed\n";
    return 1;
  }
  
  std::cout << "\n=== All scenarios completed successfully ===\n";
  return 0;
}
