#pragma once

#include "rpi_ros2_hmi_panel/panel_state.hpp"

#include <cstdint>
#include <optional>

namespace rpi_ros2_hmi_panel {

class TouchDebouncer {
 public:
  explicit TouchDebouncer(std::int64_t debounce_ms);

  [[nodiscard]] bool should_accept(PanelState state, std::int64_t now_ms);

 private:
  std::int64_t debounce_ms_;
  std::optional<PanelState> last_state_;
  std::int64_t last_accepted_ms_{0};
};

}  // namespace rpi_ros2_hmi_panel
