# ROS 2 Touchscreen State Panel on Raspberry Pi 3B

## Project Description

This project implements a compact ROS 2 C++ human-machine interface (HMI) on a Raspberry Pi 3B using a Waveshare touchscreen. The display presents four touch-selectable fields. When the operator touches one of the fields, the application updates an internal panel state. That state is owned by a ROS 2 state manager and can be read externally over an Ethernet connection.

The system is intended for embedded robotics, machine control, or test-rig applications where a local touchscreen provides simple operator input and a remote controller, supervisory computer, or diagnostic tool needs to read the current state.

## Target Platform

- **Hardware:** Raspberry Pi 3B with Waveshare SKU 12824 touchscreen
- **Operating system:** 64-bit Raspbian Bookworm
- **Middleware:** ROS 2 Jazzy Jalisco, base version
- **Language:** C++17
- **Network:** Ethernet-accessible state read interface

## System Architecture

```text
Touchscreen input
      |
      v
touchscreen_hmi_node
      |
      v
state_manager_node  --->  /panel/state
      |
      v
ethernet_state_server_node
      |
      v
External Ethernet client
```

The system is divided into three primary ROS 2 C++ nodes:

1. **touchscreen_hmi_node**
   Reads touchscreen input, maps screen coordinates to one of four fields, updates the display, and sends a state-change request to the state manager.

2. **state_manager_node**
   Owns the authoritative internal state. It validates state transitions, stores the latest state, timestamps updates, publishes state changes, and serves read/write requests.

3. **ethernet_state_server_node**
   Provides an Ethernet-readable interface for external systems. This should normally be read-only and obtain state through the ROS 2 state manager rather than accessing internal variables directly.

## Touchscreen backend

The touchscreen_hmi_node shall read touch events from Linux evdev by default.

The touchscreen_hmi_node shall suppress repeated touches to the same field within debounce_ms. Default debounce_ms: 250.

Touch input shall be processed on press events only, not continuously while the finger remains down.

Configurable parameters:
- input_device_path: string, default="/dev/input/event5"
- debounce_ms: integer, default=250
- screen_width_px: integer
- screen_height_px: integer
- rotation_degrees: integer enum {0, 90, 180, 270}
- status_bar_height_px: integer
- center_indicator_radius_px: integer

Touchscreen coordinate system:
- Coordinates are measured in display pixels after calibration and rotation are applied.
- Origin: top-left corner.
- x increases to the right.
- y increases downward.
- Screen width and height shall be configurable launch parameters:
  - screen_width_px
  - screen_height_px
- Default values shall match the configured Waveshare display.

A touch shall be processed only on the transition from not-pressed to pressed.

The node shall ignore:
- move/drag events
- release events
- repeated pressed events while the finger remains down
- multi-touch contacts other than the first active contact
- touches whose calibrated coordinate is outside every valid TouchField

Debounce rule:

A valid touch shall be suppressed if:
- it maps to the same PanelState as the last accepted touch, and
- the elapsed time since that accepted touch is less than debounce_ms.

Touches to a different field shall not be suppressed by same-field debounce.

For testability, touch event reading shall be isolated behind an interface so unit tests can inject synthetic touch events without physical hardware.

## Touchscreen design

The screen contains six visual regions: four touchable color fields, one non-touchable center state indicator, and one non-touchable top status bar.

Touchable fields:

The four touchable fields shall occupy the screen area below the status bar. The center indicator is visual-only.

Default layout:

- status_bar: x=[0,screen_width_px), y=[0,status_bar_height_px)
- FIELD_1 red: upper-left quadrant below status bar
- FIELD_2 blue: upper-right quadrant below status bar
- FIELD_3 green: lower-left quadrant below status bar
- FIELD_4 yellow: lower-right quadrant below status bar

The exact rectangles shall be derived from:
- screen_width_px
- screen_height_px
- status_bar_height_px
- center_indicator_radius_px
- layout_mode

Touches inside the non-touchable center indicator shall be ignored.
Touches in the status bar shall be ignored.
Touches outside all configured TouchField rectangles shall be ignored.

In middle there is cicrular section with radius center_indicator_radius_px, that displays current panel state (by swapping its color to last pressed section)

Top status bar displays black bar of height status_bar_height_px with time on right side and name of device on left side.

## ROS 2 Interfaces

The state_manager_node shall publish /panel/state whenever the state changes. It shall also republish the latest state at 1 Hz so late subscribers and external diagnostics can observe the current state without making a service call.

```text
/panel/state
```

Suggested message:

```text
# panel_interfaces/msg/PanelState.msg

uint8 UNKNOWN=0
uint8 FIELD_1=1
uint8 FIELD_2=2
uint8 FIELD_3=3
uint8 FIELD_4=4
uint8 IDLE=255

uint8 state
builtin_interfaces/Time stamp
string source
```

Recommended services:

```text
/panel/get_state
/panel/set_state
```

Suggested set-state service:

```text
# panel_interfaces/srv/SetPanelState.srv

uint8 state
string source
---
bool success
string message
```

Suggested get-state service:

```text
# panel_interfaces/srv/GetPanelState.srv

---
uint8 state
builtin_interfaces/Time stamp
string source
```

/panel/state shall use reliable QoS with depth 10.

state_manager_node shall publish /panel/state:
- immediately whenever the authoritative state changes
- once per second even if unchanged

The timestamp shall be generated by state_manager_node using the node clock at the time the state is accepted.

## State lifecycle:

- On state_manager_node construction, the authoritative state shall be UNKNOWN.
- After all required ROS interfaces are initialized, state_manager_node shall transition once to IDLE with source="state_manager_node".
- Operator touches may transition the state from IDLE or any FIELD_* state to another FIELD_* state.
- Invalid state values shall be rejected.
- UNKNOWN is reserved for startup, unavailable state, or internal error conditions and shall not be accepted from touchscreen_hmi_node.

## C++ Design Principles

The internal state should have exactly one owner. UI code, Ethernet code, and other ROS 2 nodes should not mutate shared global variables directly.

A suitable state model is:

```cpp
enum class PanelState : uint8_t {
    Unknown = 0,
    Field1 = 1,
    Field2 = 2,
    Field3 = 3,
    Field4 = 4,
    Idle = 255
};
```

The state manager should protect state access with a mutex. Touch input should be converted into a state request, not a direct memory write. This keeps the project testable and reduces race-condition risk.

Touchscreen field mapping should also be isolated into a testable component:

```cpp
struct TouchField {
    int x_min;
    int y_min;
    int x_max;
    int y_max;
    PanelState state;
};
```

This allows coordinate mapping, screen rotation handling, and field layout to be validated without requiring physical touchscreen interaction.

## Ethernet State Access

The ethernet_state_server_node shall expose a read-only TCP server.
```
Default bind address: 0.0.0.0
Default port: 5050
Encoding: UTF-8
Framing: newline-delimited JSON
Maximum request size: 1024 bytes
Client timeout: 5 seconds
Concurrent clients: at least 2
```

Supported request:
```
{"command":"get_state"}
```

Successful response:
```
{
  "success": true,
  "state": 2,
  "state_name": "FIELD_2",
  "stamp": "2026-05-04T12:34:56.123Z",
  "source": "touchscreen_hmi_node"
}
```

Error responses:
- malformed JSON: {"success":false,"error":"malformed_json"}
- missing command: {"success":false,"error":"missing_command"}
- unsupported command: {"success":false,"error":"unsupported_command"}
- oversized request: {"success":false,"error":"request_too_large"}
- ROS state unavailable: {"success":false,"error":"state_unavailable"}

The Ethernet server shall not mutate state. It shall obtain the current state by calling /panel/get_state or by subscribing to /panel/state and caching the latest message.

## Deployment Considerations

The application should be launched through a ROS 2 launch file and managed by `systemd` for automatic startup and restart. The deployment should define a fixed `ROS_DOMAIN_ID`, predictable Ethernet settings, and firewall rules limiting access to the external state port.

Important implementation checks include:

- touchscreen calibration and rotation
- debounce and repeated-touch suppression
- recovery if the input device disconnects
- non-blocking ROS 2 service calls from the UI node
- thread-safe state access
- structured logging
- watchdog or systemd restart behavior
- clear separation between UI, state ownership, and Ethernet access

## Definition of done:

1. The ROS 2 workspace builds successfully using colcon on ROS 2 Jazzy with C++17.
2. Launching the provided launch file starts touchscreen_hmi_node, state_manager_node, and ethernet_state_server_node.
3. Touching each of the four configured fields updates the center indicator color and causes /panel/state to report the corresponding state.
4. Touches outside valid fields do not change state.
5. /panel/get_state returns the latest state, timestamp, and source.
6. /panel/set_state rejects invalid state values with success=false.
7. The Ethernet server returns valid newline-delimited JSON for {"command":"get_state"}.
8. The Ethernet server does not directly mutate internal state.
9. Unit tests cover coordinate mapping, invalid touch handling, state validation, repeated-touch behavior, and get/set service behavior.
10. systemd service file starts the launch file automatically and restarts on failure.
11. The system publishes IDLE after successful initialization.
12. UNKNOWN is not accepted as an operator-selected state after initialization.
13. Malformed, oversized, missing-command, and unsupported TCP requests return defined errors.
14. The implementation includes a README with build, launch, test, and systemd installation commands.

## Summary

The recommended design is a ROS 2 C++ system where the touchscreen changes state through a controlled service, the state manager owns and publishes the current state, and the Ethernet interface reads that state through ROS 2. This avoids unsafe shared global state, improves maintainability, and makes the touchscreen, internal state logic, and network interface independently testable.
