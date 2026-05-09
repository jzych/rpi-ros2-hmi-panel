#pragma once

#include "rpi_ros2_hmi_panel/panel_state.hpp"

#include <optional>
#include <string>
#include <vector>

namespace rpi_ros2_hmi_panel {

struct Point {
  int x;
  int y;
};

struct Rect {
  int x_min;
  int y_min;
  int x_max;
  int y_max;
};

struct TouchField {
  Rect rect;
  PanelState state;
};

struct LayoutConfig {
  int screen_width_px{800};
  int screen_height_px{480};
  int rotation_degrees{0};
  int status_bar_height_px{48};
  int center_indicator_radius_px{64};
  std::string layout_mode{"quadrants"};
};

[[nodiscard]] std::vector<TouchField> make_touch_fields(const LayoutConfig& config);
[[nodiscard]] Point apply_rotation(Point point, int width, int height, int rotation_degrees);
[[nodiscard]] std::optional<PanelState> map_touch_to_state(const LayoutConfig& config, Point point);

}  // namespace rpi_ros2_hmi_panel
