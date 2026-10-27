cmake_minimum_required(VERSION 3.8)
project(vacuum_controller)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(geometry_msgs REQUIRED)

add_library(vacuum_controller_core
  src/vacuum_driver.cpp
  src/vacuum_controller.cpp
)

target_include_directories(vacuum_controller_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

add_executable(control_node
  src/control_node.cpp
)

target_link_libraries(control_node
  vacuum_controller_core
)

ament_target_dependencies(control_node
  rclcpp
  geometry_msgs
)

install(TARGETS vacuum_controller_core
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
)

install(TARGETS control_node
  DESTINATION lib/${PROJECT_NAME}
)

install(DIRECTORY include/
  DESTINATION include
)

install(DIRECTORY config
  DESTINATION share/${PROJECT_NAME}
)

if(BUILD_TESTING)
  enable_testing()
  add_executable(controller_test test/controller_test.cpp)
  target_link_libraries(controller_test vacuum_controller_core)
  add_test(NAME controller_test COMMAND controller_test)
endif()

ament_package()
