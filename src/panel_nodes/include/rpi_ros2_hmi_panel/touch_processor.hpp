#pragma once

#include "rpi_ros2_hmi_panel/touch_layout.hpp"

#include <functional>

namespace rpi_ros2_hmi_panel {

class PressFrameTouchProcessor {
 public:
  using TouchCallback = std::function<void(Point)>;

  PressFrameTouchProcessor(LayoutConfig layout_config, TouchCallback callback);

  void update_x(int x);
  void update_y(int y);
  void set_pressed(bool pressed);
  void sync_frame();

 private:
  LayoutConfig layout_config_;
  TouchCallback callback_;
  int latest_x_{0};
  int latest_y_{0};
  bool has_x_{false};
  bool has_y_{false};
  bool pressed_{false};
  bool pending_press_{false};
};

}  // namespace rpi_ros2_hmi_panel
