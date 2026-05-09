cmake_minimum_required(VERSION 3.20)
project(panel_nodes)

find_package(ament_cmake REQUIRED)
find_package(panel_interfaces REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(panel_core
  src/json_protocol.cpp
  src/touch_debounce.cpp
  src/touch_layout.cpp
  src/touch_processor.cpp
)
target_include_directories(panel_core
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(panel_core
  PUBLIC
    nlohmann_json::nlohmann_json
)

add_executable(state_manager_node src/state_manager_node.cpp)
ament_target_dependencies(state_manager_node
  panel_interfaces
  rclcpp
  std_msgs
)
target_link_libraries(state_manager_node panel_core)

add_executable(touchscreen_hmi_node src/touchscreen_hmi_node.cpp)
target_include_directories(touchscreen_hmi_node PRIVATE ${SDL2_INCLUDE_DIRS})
ament_target_dependencies(touchscreen_hmi_node
  panel_interfaces
  rclcpp
  std_msgs
)
target_link_libraries(touchscreen_hmi_node panel_core ${SDL2_LIBRARIES})

add_executable(ethernet_state_server_node src/ethernet_state_server_node.cpp)
ament_target_dependencies(ethernet_state_server_node
  panel_interfaces
  rclcpp
  std_msgs
)
target_link_libraries(ethernet_state_server_node panel_core Threads::Threads)

install(
  TARGETS
  panel_core
  state_manager_node
  touchscreen_hmi_node
  ethernet_state_server_node
  EXPORT panel_nodes_targets
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION lib/${PROJECT_NAME}
)

install(DIRECTORY include/ DESTINATION include)

if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)

  ament_add_gtest(core_tests test/core_tests.cpp)
  target_link_libraries(core_tests panel_core)
endif()

ament_package()
