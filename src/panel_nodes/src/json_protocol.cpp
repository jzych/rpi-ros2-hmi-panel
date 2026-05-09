#include "rpi_ros2_hmi_panel/json_protocol.hpp"

#include <nlohmann/json.hpp>

namespace rpi_ros2_hmi_panel {

ParsedRequest parse_json_request(const std::string_view line, const std::size_t maximum_size)
{
  if (line.size() > maximum_size) {
    return ParsedRequest{RequestError::RequestTooLarge, false};
  }

  nlohmann::json request;
  try {
    request = nlohmann::json::parse(line);
  } catch (const nlohmann::json::parse_error&) {
    return ParsedRequest{RequestError::MalformedJson, false};
  }

  if (!request.contains("command")) {
    return ParsedRequest{RequestError::MissingCommand, false};
  }

  if (!request["command"].is_string()) {
    return ParsedRequest{RequestError::UnsupportedCommand, false};
  }

  if (request["command"].get<std::string>() != "get_state") {
    return ParsedRequest{RequestError::UnsupportedCommand, false};
  }

  return ParsedRequest{RequestError::None, true};
}

std::string request_error_name(const RequestError error)
{
  switch (error) {
    case RequestError::MalformedJson:
      return "malformed_json";
    case RequestError::MissingCommand:
      return "missing_command";
    case RequestError::UnsupportedCommand:
      return "unsupported_command";
    case RequestError::RequestTooLarge:
      return "request_too_large";
    case RequestError::StateUnavailable:
      return "state_unavailable";
    case RequestError::None:
      return "";
  }

  return "unsupported_command";
}

std::string make_error_response(const RequestError error)
{
  return (nlohmann::json{{"success", false}, {"error", request_error_name(error)}}).dump() + "\n";
}

std::string make_state_response(
    const PanelState state,
    const std::string_view stamp,
    const std::string_view source)
{
  return (nlohmann::json{
              {"success", true},
              {"state", panel_state_to_wire(state)},
              {"state_name", std::string(panel_state_name(state))},
              {"stamp", std::string(stamp)},
              {"source", std::string(source)},
          })
             .dump() +
         "\n";
}

}  // namespace rpi_ros2_hmi_panel
