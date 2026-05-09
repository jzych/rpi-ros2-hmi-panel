#!/usr/bin/env bash
# Source this file from a shell or systemd command before running ros2 commands.

_rpi_hmi_restore_nounset=0
case "$-" in
  *u*)
    _rpi_hmi_restore_nounset=1
    set +u
    ;;
esac

_rpi_hmi_script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
_rpi_hmi_workspace="$(cd "${_rpi_hmi_script_dir}/../.." && pwd)"

if [[ -f /opt/ros/jazzy/setup.bash ]]; then
  source /opt/ros/jazzy/setup.bash
elif [[ -f "${HOME}/ros2_jazzy/install/setup.bash" ]]; then
  source "${HOME}/ros2_jazzy/install/setup.bash"
else
  echo "ROS 2 Jazzy setup.bash not found" >&2
  return 1
fi

if [[ -f "${_rpi_hmi_workspace}/install/setup.bash" ]]; then
  source "${_rpi_hmi_workspace}/install/setup.bash"
fi

# Some source-built ROS/colcon installations on the Pi generate incomplete bash
# hooks. These exports make the isolated colcon install usable either way.
for _rpi_hmi_prefix in \
  "${_rpi_hmi_workspace}/install/panel_bringup" \
  "${_rpi_hmi_workspace}/install/panel_nodes" \
  "${_rpi_hmi_workspace}/install/panel_interfaces"; do
  if [[ -d "${_rpi_hmi_prefix}" ]]; then
    case ":${AMENT_PREFIX_PATH:-}:" in
      *":${_rpi_hmi_prefix}:"*) ;;
      *) export AMENT_PREFIX_PATH="${_rpi_hmi_prefix}:${AMENT_PREFIX_PATH:-}" ;;
    esac
    case ":${CMAKE_PREFIX_PATH:-}:" in
      *":${_rpi_hmi_prefix}:"*) ;;
      *) export CMAKE_PREFIX_PATH="${_rpi_hmi_prefix}:${CMAKE_PREFIX_PATH:-}" ;;
    esac
  fi
done

if [[ -d "${_rpi_hmi_workspace}/install/panel_interfaces/lib" ]]; then
  export LD_LIBRARY_PATH="${_rpi_hmi_workspace}/install/panel_interfaces/lib:${LD_LIBRARY_PATH:-}"
fi
if [[ -d "${_rpi_hmi_workspace}/install/panel_nodes/lib" ]]; then
  export LD_LIBRARY_PATH="${_rpi_hmi_workspace}/install/panel_nodes/lib:${LD_LIBRARY_PATH:-}"
fi

unset _rpi_hmi_prefix
unset _rpi_hmi_script_dir
unset _rpi_hmi_workspace

if [[ "${_rpi_hmi_restore_nounset}" == "1" ]]; then
  set -u
fi
unset _rpi_hmi_restore_nounset
