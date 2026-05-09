# Agent Operating Guide

This repository is configured for Codex with a lean, evidence-first workflow.

## Objectives

- Keep changes small, testable, and easy to review.
- Prefer evidence before edits: inspect code, confirm assumptions, then change.
- Keep the Raspberry Pi target path and WSL build path usable for fast compile/test loops.
- Preserve the ROS 2 interface contract from `.tasks/rpi-ros2-hmi-panel.md`.

## Project Facts

- Language: C++17.
- Middleware: ROS 2 Jazzy Jalisco.
- Build system: colcon with ament CMake packages.
- Primary target: Raspberry Pi 3B on 64-bit Raspberry Pi OS Bookworm.
- Source root: `src/`.
- Package layout:
  - `src/panel_interfaces`: ROS messages and services.
  - `src/panel_nodes`: C++ ROS nodes and testable logic.
  - `src/panel_bringup`: launch, config, and systemd assets.
  - `deployment`: deployment helper scripts.

## Session Start

1. Read `.tasks/rpi-ros2-hmi-panel.md` before implementation work.
2. Run `git status --short` and preserve unrelated user changes.
3. Read touched files and nearby tests before editing.

## Working Rules

1. Keep `CMakeLists.txt`, `package.xml`, source files, and launch/config files aligned.
2. Prefer simple implementations over speculative abstractions.
3. Keep ROS node classes thin; isolate state validation, touch mapping, debounce, and protocol handling into testable components.
4. Avoid shared global mutable state. The state manager owns authoritative panel state.
5. Use explicit conversions at ROS message/service boundaries.
6. Replace non-obvious numeric literals with named `constexpr` values that document intent.
7. Preserve a clean command path for both WSL users and the Raspberry Pi target.
8. After meaningful changes, run the smallest relevant verification step.

## C++ Rules

- Use C++17 and CMake 3.20+.
- Use namespace `rpi_ros2_hmi_panel` for shared C++ code.
- Use `enum class` for internal state.
- Prefer `std::string_view` for non-owning string interfaces when practical.
- Use `[[nodiscard]]` on computed results where ignoring the result would hide a defect.
- Use value types or smart pointers; avoid raw owning pointers.
- Protect cross-thread state access with a mutex.

## ROS 2 Rules

- `/panel/state` uses reliable QoS with depth 10.
- `state_manager_node` publishes immediately after accepted state changes and republishes the latest state at 1 Hz.
- `UNKNOWN` is reserved for startup, unavailable state, or internal errors.
- `UNKNOWN` must not be accepted as an operator-selected state after initialization.
- UI and Ethernet nodes must read or request state through ROS interfaces; they must not mutate state directly.

## Verification Order

When tool availability permits, prefer this order:

1. Source ROS: `source /opt/ros/jazzy/setup.bash`
2. Build: `colcon build --cmake-args -DCMAKE_BUILD_TYPE=RelWithDebInfo`
3. Focused tests: `colcon test --packages-select <package> --event-handlers console_direct+`
4. Full tests: `colcon test --event-handlers console_direct+`
5. Results: `colcon test-result --verbose`

Inside WSL, use the Linux workspace path:

`/mnt/m/CodingProjects/rpi-ros2-hmi-panel`

On the Raspberry Pi target, use:

`/home/erd/rpi-ros2-hmi-panel`

## Codex-Specific Notes

- If a task needs web verification, prefer official docs or primary sources.
- Do not mark work complete without stating what was verified and what could not be verified.
- If target-device work is needed, connect with `ssh erd@raspberrypi-jz.local`.
