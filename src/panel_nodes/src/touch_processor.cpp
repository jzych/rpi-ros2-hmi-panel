#include "rpi_ros2_hmi_panel/touch_processor.hpp"

#include <utility>

namespace rpi_ros2_hmi_panel {

PressFrameTouchProcessor::PressFrameTouchProcessor(
    LayoutConfig layout_config,
    TouchCallback callback)
    : layout_config_(std::move(layout_config)), callback_(std::move(callback))
{
}

void PressFrameTouchProcessor::update_x(const int x)
{
  latest_x_ = x;
  has_x_ = true;
}

void PressFrameTouchProcessor::update_y(const int y)
{
  latest_y_ = y;
  has_y_ = true;
}

void PressFrameTouchProcessor::set_pressed(const bool pressed)
{
  if (pressed && !pressed_) {
    pending_press_ = true;
  }
  pressed_ = pressed;
}

void PressFrameTouchProcessor::sync_frame()
{
  if (!pending_press_ || !has_x_ || !has_y_) {
    return;
  }

  pending_press_ = false;
  callback_(apply_rotation(
      Point{latest_x_, latest_y_},
      layout_config_.screen_width_px,
      layout_config_.screen_height_px,
      layout_config_.rotation_degrees));
}

}  // namespace rpi_ros2_hmi_panel
