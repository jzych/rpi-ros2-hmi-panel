cmake_minimum_required(VERSION 3.20)
project(panel_bringup)

find_package(ament_cmake REQUIRED)

install(DIRECTORY
  config
  launch
  systemd
  DESTINATION share/${PROJECT_NAME}
)

ament_package()
