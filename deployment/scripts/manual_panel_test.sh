#!/usr/bin/env bash
set -euo pipefail

WORKSPACE="${1:-$HOME/rpi-ros2-hmi-panel}"
TCP_HOST="${TCP_HOST:-127.0.0.1}"
TCP_PORT="${TCP_PORT:-5050}"

cd "$WORKSPACE"
set +u
source deployment/scripts/source_workspace.bash
set -u

export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}"
export WAYLAND_DISPLAY="${WAYLAND_DISPLAY:-wayland-0}"
export SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-wayland}"
export SDL_RENDER_DRIVER="${SDL_RENDER_DRIVER:-software}"

cleanup() {
  if [[ -n "${LAUNCH_PID:-}" ]]; then
    kill -- "-$LAUNCH_PID" 2>/dev/null || true
    kill "$LAUNCH_PID" 2>/dev/null || true
    wait "$LAUNCH_PID" 2>/dev/null || true
  fi
  pkill -u "$(id -u)" -f 'panel_nodes/lib/panel_nodes/state_manager_node' 2>/dev/null || true
  pkill -u "$(id -u)" -f 'panel_nodes/lib/panel_nodes/touchscreen_hmi_node' 2>/dev/null || true
  pkill -u "$(id -u)" -f 'panel_nodes/lib/panel_nodes/ethernet_state_server_node' 2>/dev/null || true
}
trap cleanup EXIT INT TERM

echo "Starting HMI panel launch file..."
setsid ros2 launch panel_bringup panel.launch.py > /tmp/rpi_hmi_manual_launch.log 2>&1 &
LAUNCH_PID=$!

sleep 8

if ! kill -0 "$LAUNCH_PID" 2>/dev/null; then
  echo "Launch exited early. Log:"
  cat /tmp/rpi_hmi_manual_launch.log
  exit 1
fi

echo
echo "Manual touchscreen test"
echo "Expected layout:"
echo "  FIELD_1 red    = upper-left quadrant below black status bar"
echo "  FIELD_2 blue   = upper-right quadrant below black status bar"
echo "  FIELD_3 green  = lower-left quadrant below black status bar"
echo "  FIELD_4 yellow = lower-right quadrant below black status bar"
echo "  Center circle and top status bar are not touchable"
echo
echo "For each prompt, touch the screen once, then press Enter here."
echo "The script checks /panel/get_state and TCP JSON after each step."
echo

check_state() {
  local label="$1"
  local expected="$2"

  read -r -p "Touch ${label}, then press Enter to verify..."

  local service_output
  service_output="$(ros2 service call /panel/get_state panel_interfaces/srv/GetPanelState '{}' 2>&1)"
  echo "$service_output"
  if ! grep -Eq "state[:=][[:space:]]*${expected}" <<<"$service_output"; then
    echo "FAIL: expected ROS state=${expected} for ${label}"
    exit 1
  fi

  local tcp_output
  tcp_output="$(
    python3 - "$TCP_HOST" "$TCP_PORT" <<'PY'
import socket
import sys

host = sys.argv[1]
port = int(sys.argv[2])
with socket.create_connection((host, port), timeout=5) as sock:
    sock.sendall(b'{"command":"get_state"}\n')
    print(sock.recv(4096).decode("utf-8").strip())
PY
  )"
  echo "$tcp_output"
  if ! grep -q "\"state\":${expected}" <<<"$tcp_output"; then
    echo "FAIL: expected TCP state=${expected} for ${label}"
    exit 1
  fi
}

check_unchanged() {
  local label="$1"
  local expected="$2"

  read -r -p "Touch ${label}, then press Enter to verify state did not change..."

  local service_output
  service_output="$(ros2 service call /panel/get_state panel_interfaces/srv/GetPanelState '{}' 2>&1)"
  echo "$service_output"
  if ! grep -Eq "state[:=][[:space:]]*${expected}" <<<"$service_output"; then
    echo "FAIL: expected unchanged ROS state=${expected} after ${label}"
    exit 1
  fi
}

check_state "FIELD_1 red upper-left quadrant" 1
check_state "FIELD_2 blue upper-right quadrant" 2
check_state "FIELD_3 green lower-left quadrant" 3
check_state "FIELD_4 yellow lower-right quadrant" 4

echo
echo "Debounce check: touch FIELD_4 yellow twice quickly. The state should remain FIELD_4."
check_unchanged "FIELD_4 yellow twice quickly" 4

echo
echo "Ignored-region checks. These should not change the current state from FIELD_4."
check_unchanged "the black top status bar" 4
check_unchanged "the center circular indicator" 4

echo
echo "PASS: manual touchscreen, ROS service, and TCP JSON checks completed."
echo "Launch log: /tmp/rpi_hmi_manual_launch.log"
