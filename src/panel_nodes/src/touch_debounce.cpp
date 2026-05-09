#include "rpi_ros2_hmi_panel/touch_debounce.hpp"

namespace rpi_ros2_hmi_panel {

TouchDebouncer::TouchDebouncer(const std::int64_t debounce_ms)
    : debounce_ms_(debounce_ms < 0 ? 0 : debounce_ms)
{
}

bool TouchDebouncer::should_accept(const PanelState state, const std::int64_t now_ms)
{
  if (last_state_.has_value() && *last_state_ == state &&
      (now_ms - last_accepted_ms_) < debounce_ms_) {
    return false;
  }

  last_state_ = state;
  last_accepted_ms_ = now_ms;
  return true;
}

}  // namespace rpi_ros2_hmi_panel
