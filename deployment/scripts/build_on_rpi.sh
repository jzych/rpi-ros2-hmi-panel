#!/usr/bin/env bash
set -euo pipefail

cd "${1:-$HOME/rpi-ros2-hmi-panel}"
set +u
if [[ -f /opt/ros/jazzy/setup.bash ]]; then
  source /opt/ros/jazzy/setup.bash
else
  source "$HOME/ros2_jazzy/install/setup.bash"
fi
set -u
export MAKEFLAGS="${MAKEFLAGS:--j1}"
export CMAKE_BUILD_PARALLEL_LEVEL="${CMAKE_BUILD_PARALLEL_LEVEL:-1}"
colcon build --parallel-workers 1 --cmake-args -DCMAKE_BUILD_TYPE=RelWithDebInfo
