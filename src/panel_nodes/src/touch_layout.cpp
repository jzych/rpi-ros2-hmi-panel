#include "rpi_ros2_hmi_panel/touch_layout.hpp"

#include <cmath>

namespace rpi_ros2_hmi_panel {

namespace {

[[nodiscard]] bool contains(const Rect rect, const Point point)
{
  return point.x >= rect.x_min && point.x < rect.x_max && point.y >= rect.y_min &&
         point.y < rect.y_max;
}

[[nodiscard]] bool is_inside_center_indicator(const LayoutConfig& config, const Point point)
{
  const auto center_x = config.screen_width_px / 2;
  const auto center_y = config.status_bar_height_px +
                        ((config.screen_height_px - config.status_bar_height_px) / 2);
  const auto dx = point.x - center_x;
  const auto dy = point.y - center_y;
  const auto radius = config.center_indicator_radius_px;
  return (dx * dx) + (dy * dy) <= radius * radius;
}

}  // namespace

std::vector<TouchField> make_touch_fields(const LayoutConfig& config)
{
  const auto mid_x = config.screen_width_px / 2;
  const auto mid_y =
      config.status_bar_height_px + ((config.screen_height_px - config.status_bar_height_px) / 2);

  return {
      TouchField{Rect{0, config.status_bar_height_px, mid_x, mid_y}, PanelState::Field1},
      TouchField{Rect{mid_x, config.status_bar_height_px, config.screen_width_px, mid_y},
                 PanelState::Field2},
      TouchField{Rect{0, mid_y, mid_x, config.screen_height_px}, PanelState::Field3},
      TouchField{Rect{mid_x, mid_y, config.screen_width_px, config.screen_height_px},
                 PanelState::Field4},
  };
}

Point apply_rotation(const Point point, const int width, const int height, const int rotation_degrees)
{
  switch (rotation_degrees) {
    case 90:
      return Point{height - 1 - point.y, point.x};
    case 180:
      return Point{width - 1 - point.x, height - 1 - point.y};
    case 270:
      return Point{point.y, width - 1 - point.x};
    default:
      return point;
  }
}

std::optional<PanelState> map_touch_to_state(const LayoutConfig& config, const Point point)
{
  if (config.layout_mode != "quadrants") {
    return std::nullopt;
  }

  if (point.x < 0 || point.y < 0 || point.x >= config.screen_width_px ||
      point.y >= config.screen_height_px) {
    return std::nullopt;
  }

  if (point.y < config.status_bar_height_px || is_inside_center_indicator(config, point)) {
    return std::nullopt;
  }

  for (const auto& field : make_touch_fields(config)) {
    if (contains(field.rect, point)) {
      return field.state;
    }
  }

  return std::nullopt;
}

}  // namespace rpi_ros2_hmi_panel
