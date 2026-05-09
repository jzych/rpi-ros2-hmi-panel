#pragma once

#include <cstdint>
#include <string_view>

namespace rpi_ros2_hmi_panel {

enum class PanelState : std::uint8_t {
  Unknown = 0,
  Field1 = 1,
  Field2 = 2,
  Field3 = 3,
  Field4 = 4,
  Idle = 255,
};

[[nodiscard]] constexpr bool is_valid_requested_state(const PanelState state) noexcept
{
  return state == PanelState::Field1 || state == PanelState::Field2 ||
         state == PanelState::Field3 || state == PanelState::Field4 ||
         state == PanelState::Idle;
}

[[nodiscard]] constexpr bool is_touch_selectable_state(const PanelState state) noexcept
{
  return state == PanelState::Field1 || state == PanelState::Field2 ||
         state == PanelState::Field3 || state == PanelState::Field4;
}

[[nodiscard]] constexpr PanelState panel_state_from_wire(const std::uint8_t state) noexcept
{
  switch (state) {
    case 1:
      return PanelState::Field1;
    case 2:
      return PanelState::Field2;
    case 3:
      return PanelState::Field3;
    case 4:
      return PanelState::Field4;
    case 255:
      return PanelState::Idle;
    default:
      return PanelState::Unknown;
  }
}

[[nodiscard]] constexpr std::uint8_t panel_state_to_wire(const PanelState state) noexcept
{
  return static_cast<std::uint8_t>(state);
}

[[nodiscard]] constexpr std::string_view panel_state_name(const PanelState state) noexcept
{
  switch (state) {
    case PanelState::Field1:
      return "FIELD_1";
    case PanelState::Field2:
      return "FIELD_2";
    case PanelState::Field3:
      return "FIELD_3";
    case PanelState::Field4:
      return "FIELD_4";
    case PanelState::Idle:
      return "IDLE";
    case PanelState::Unknown:
      return "UNKNOWN";
  }

  return "UNKNOWN";
}

}  // namespace rpi_ros2_hmi_panel
