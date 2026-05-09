# rpi-ros2-hmi-panel

This project implements a compact ROS 2 C++ human-machine interface (HMI) on a Raspberry Pi 3B using a Waveshare touchscreen.

## Build

```bash
source /opt/ros/jazzy/setup.bash
colcon build --cmake-args -DCMAKE_BUILD_TYPE=RelWithDebInfo
```

On the Raspberry Pi source-installed ROS 2 environment:

```bash
source ~/ros2_jazzy/install/setup.bash
export MAKEFLAGS=-j1 CMAKE_BUILD_PARALLEL_LEVEL=1
colcon build --parallel-workers 1 --cmake-args -DCMAKE_BUILD_TYPE=RelWithDebInfo
```

Required non-ROS packages:

```bash
sudo apt install -y libsdl2-dev nlohmann-json3-dev pkg-config
```

## Test

```bash
source /opt/ros/jazzy/setup.bash
colcon test --event-handlers console_direct+
colcon test-result --verbose
```

On the Raspberry Pi source-installed ROS 2 environment:

```bash
source ~/ros2_jazzy/install/setup.bash
colcon test --parallel-workers 1 --event-handlers console_direct+
colcon test-result --verbose
```

## Manual Touchscreen Test

Run this on the Raspberry Pi from the workspace root after building:

```bash
chmod +x deployment/scripts/manual_panel_test.sh
deployment/scripts/manual_panel_test.sh
```

The script launches all nodes, then asks the operator to touch:

- `FIELD_1`: red upper-left quadrant below the status bar.
- `FIELD_2`: blue upper-right quadrant below the status bar.
- `FIELD_3`: green lower-left quadrant below the status bar.
- `FIELD_4`: yellow lower-right quadrant below the status bar.
- `FIELD_4` twice quickly to check same-field debounce.
- The black top status bar to confirm it is ignored.
- The center circular indicator to confirm it is ignored.

After each prompt, press Enter in the terminal. The script verifies `/panel/get_state`
and the TCP JSON response from `127.0.0.1:5050`.

To exit an interactive panel run, press `Esc` in the panel window or press `Ctrl+C`
in the terminal running `ros2 launch`.

## Launch

On the Raspberry Pi:

```bash
source deployment/scripts/source_workspace.bash
XDG_RUNTIME_DIR=/run/user/1000 WAYLAND_DISPLAY=wayland-0 SDL_VIDEODRIVER=wayland SDL_RENDER_DRIVER=software ros2 launch panel_bringup panel.launch.py
```

On a desktop Linux system with ROS 2 Jazzy and a graphical SDL session:

```bash
source /opt/ros/jazzy/setup.bash
source install/setup.bash
ros2 launch panel_bringup panel.launch.py
```

The default touchscreen input path is:

```text
/dev/input/by-path/platform-3f204000.spi-cs-1-event
```

The default TCP endpoint listens on `0.0.0.0:5050` and accepts newline-delimited JSON:

```bash
PI_HOST=192.168.0.135
python3 - <<'PY'
import os
import socket

with socket.create_connection((os.environ["PI_HOST"], 5050), timeout=5) as sock:
    sock.sendall(b'{"command":"get_state"}\n')
    print(sock.recv(4096).decode("utf-8").strip())
PY
```

## systemd

The baseline service unit is installed from:

```text
src/panel_bringup/systemd/rpi-ros2-hmi-panel.service
```

Install it on the Raspberry Pi:

```bash
sudo cp src/panel_bringup/systemd/rpi-ros2-hmi-panel.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now rpi-ros2-hmi-panel.service
sudo systemctl status rpi-ros2-hmi-panel.service --no-pager
```

Adjust the workspace path, user, `ROS_DOMAIN_ID`, and network/firewall policy if the target setup changes.
