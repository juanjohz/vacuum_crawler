cmake_minimum_required(VERSION 3.8)
project(vacuum_teleop)

# C++ version
set(CMAKE_CXX_STANDARD 17)

# Dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)

# Create executable
add_executable(vacuum_teleop_node
  src/vacuum_teleop.cpp
)

# Link ROS dependencies
ament_target_dependencies(vacuum_teleop_node
  rclcpp
  sensor_msgs
  geometry_msgs
)

# Install executable
install(TARGETS
  vacuum_teleop_node
  DESTINATION lib/${PROJECT_NAME}
)

ament_package()
