#pragma once

#include "rpi_ros2_hmi_panel/panel_state.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace rpi_ros2_hmi_panel {

enum class RequestError {
  None,
  MalformedJson,
  MissingCommand,
  UnsupportedCommand,
  RequestTooLarge,
  StateUnavailable,
};

struct ParsedRequest {
  RequestError error{RequestError::None};
  bool get_state{false};
};

[[nodiscard]] ParsedRequest parse_json_request(std::string_view line, std::size_t maximum_size);
[[nodiscard]] std::string request_error_name(RequestError error);
[[nodiscard]] std::string make_error_response(RequestError error);
[[nodiscard]] std::string make_state_response(
    PanelState state,
    std::string_view stamp,
    std::string_view source);

}  // namespace rpi_ros2_hmi_panel
