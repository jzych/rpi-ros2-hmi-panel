#include "rpi_ros2_hmi_panel/json_protocol.hpp"
#include "rpi_ros2_hmi_panel/panel_state.hpp"
#include "rpi_ros2_hmi_panel/touch_debounce.hpp"
#include "rpi_ros2_hmi_panel/touch_layout.hpp"
#include "rpi_ros2_hmi_panel/touch_processor.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace {

using rpi_ros2_hmi_panel::LayoutConfig;
using rpi_ros2_hmi_panel::PanelState;
using rpi_ros2_hmi_panel::Point;
using rpi_ros2_hmi_panel::RequestError;
using rpi_ros2_hmi_panel::TouchDebouncer;

TEST(PanelState, GivenWireValues_WhenConverted_ThenKnownValuesRoundTrip)
{
  EXPECT_EQ(rpi_ros2_hmi_panel::panel_state_from_wire(1), PanelState::Field1);
  EXPECT_EQ(rpi_ros2_hmi_panel::panel_state_from_wire(2), PanelState::Field2);
  EXPECT_EQ(rpi_ros2_hmi_panel::panel_state_from_wire(3), PanelState::Field3);
  EXPECT_EQ(rpi_ros2_hmi_panel::panel_state_from_wire(4), PanelState::Field4);
  EXPECT_EQ(rpi_ros2_hmi_panel::panel_state_from_wire(255), PanelState::Idle);
  EXPECT_EQ(rpi_ros2_hmi_panel::panel_state_from_wire(9), PanelState::Unknown);
}

TEST(PanelState, GivenPanelStates_WhenValidated_ThenUnknownIsRejected)
{
  EXPECT_TRUE(rpi_ros2_hmi_panel::is_valid_requested_state(PanelState::Field1));
  EXPECT_TRUE(rpi_ros2_hmi_panel::is_valid_requested_state(PanelState::Field2));
  EXPECT_TRUE(rpi_ros2_hmi_panel::is_valid_requested_state(PanelState::Field3));
  EXPECT_TRUE(rpi_ros2_hmi_panel::is_valid_requested_state(PanelState::Field4));
  EXPECT_TRUE(rpi_ros2_hmi_panel::is_valid_requested_state(PanelState::Idle));
  EXPECT_FALSE(rpi_ros2_hmi_panel::is_valid_requested_state(PanelState::Unknown));

  EXPECT_TRUE(rpi_ros2_hmi_panel::is_touch_selectable_state(PanelState::Field1));
  EXPECT_TRUE(rpi_ros2_hmi_panel::is_touch_selectable_state(PanelState::Field2));
  EXPECT_TRUE(rpi_ros2_hmi_panel::is_touch_selectable_state(PanelState::Field3));
  EXPECT_TRUE(rpi_ros2_hmi_panel::is_touch_selectable_state(PanelState::Field4));
  EXPECT_FALSE(rpi_ros2_hmi_panel::is_touch_selectable_state(PanelState::Idle));
  EXPECT_FALSE(rpi_ros2_hmi_panel::is_touch_selectable_state(PanelState::Unknown));
}

TEST(PanelState, GivenPanelStates_WhenNamed_ThenNamesMatchWireContract)
{
  EXPECT_EQ(rpi_ros2_hmi_panel::panel_state_name(PanelState::Unknown), "UNKNOWN");
  EXPECT_EQ(rpi_ros2_hmi_panel::panel_state_name(PanelState::Field1), "FIELD_1");
  EXPECT_EQ(rpi_ros2_hmi_panel::panel_state_name(PanelState::Field2), "FIELD_2");
  EXPECT_EQ(rpi_ros2_hmi_panel::panel_state_name(PanelState::Field3), "FIELD_3");
  EXPECT_EQ(rpi_ros2_hmi_panel::panel_state_name(PanelState::Field4), "FIELD_4");
  EXPECT_EQ(rpi_ros2_hmi_panel::panel_state_name(PanelState::Idle), "IDLE");
}

TEST(TouchLayout, GivenDefaultConfig_WhenFieldsAreCreated_ThenRectanglesMatchScreenHalves)
{
  const LayoutConfig config{};
  const auto fields = rpi_ros2_hmi_panel::make_touch_fields(config);

  ASSERT_EQ(fields.size(), 4U);
  EXPECT_EQ(fields[0].rect.x_min, 0);
  EXPECT_EQ(fields[0].rect.y_min, 48);
  EXPECT_EQ(fields[0].rect.x_max, 400);
  EXPECT_EQ(fields[0].rect.y_max, 264);
  EXPECT_EQ(fields[1].rect.x_min, 400);
  EXPECT_EQ(fields[1].rect.x_max, 800);
  EXPECT_EQ(fields[2].rect.y_min, 264);
  EXPECT_EQ(fields[2].rect.y_max, 480);
  EXPECT_EQ(fields[3].state, PanelState::Field4);
}

TEST(TouchLayout, GivenDefaultLayout_WhenMappingQuadrants_ThenReturnsExpectedStates)
{
  const LayoutConfig config{};

  EXPECT_EQ(rpi_ros2_hmi_panel::map_touch_to_state(config, Point{10, 60}), PanelState::Field1);
  EXPECT_EQ(rpi_ros2_hmi_panel::map_touch_to_state(config, Point{790, 60}), PanelState::Field2);
  EXPECT_EQ(rpi_ros2_hmi_panel::map_touch_to_state(config, Point{10, 470}), PanelState::Field3);
  EXPECT_EQ(rpi_ros2_hmi_panel::map_touch_to_state(config, Point{790, 470}), PanelState::Field4);
}

TEST(TouchLayout, GivenIgnoredRegions_WhenMappingTouch_ThenReturnsNoState)
{
  const LayoutConfig config{};

  EXPECT_FALSE(rpi_ros2_hmi_panel::map_touch_to_state(config, Point{10, 10}).has_value());
  EXPECT_FALSE(rpi_ros2_hmi_panel::map_touch_to_state(config, Point{400, 264}).has_value());
  EXPECT_FALSE(rpi_ros2_hmi_panel::map_touch_to_state(config, Point{-1, 60}).has_value());
  EXPECT_FALSE(rpi_ros2_hmi_panel::map_touch_to_state(config, Point{800, 60}).has_value());
}

TEST(TouchLayout, GivenBoundaryCoordinates_WhenMappingTouch_ThenHalfOpenRectanglesAreUsed)
{
  const LayoutConfig config{};

  EXPECT_FALSE(rpi_ros2_hmi_panel::map_touch_to_state(config, Point{0, 47}).has_value());
  EXPECT_EQ(rpi_ros2_hmi_panel::map_touch_to_state(config, Point{0, 48}), PanelState::Field1);
  EXPECT_EQ(rpi_ros2_hmi_panel::map_touch_to_state(config, Point{399, 48}), PanelState::Field1);
  EXPECT_EQ(rpi_ros2_hmi_panel::map_touch_to_state(config, Point{400, 48}), PanelState::Field2);
  EXPECT_EQ(rpi_ros2_hmi_panel::map_touch_to_state(config, Point{0, 263}), PanelState::Field1);
  EXPECT_EQ(rpi_ros2_hmi_panel::map_touch_to_state(config, Point{0, 264}), PanelState::Field3);
  EXPECT_EQ(rpi_ros2_hmi_panel::map_touch_to_state(config, Point{799, 479}), PanelState::Field4);
}

TEST(TouchLayout, GivenCenterCircleBoundary_WhenMappingTouch_ThenInsideAndEdgeAreIgnored)
{
  const LayoutConfig config{};

  EXPECT_FALSE(rpi_ros2_hmi_panel::map_touch_to_state(config, Point{400, 264}).has_value());
  EXPECT_FALSE(rpi_ros2_hmi_panel::map_touch_to_state(config, Point{464, 264}).has_value());
  EXPECT_EQ(rpi_ros2_hmi_panel::map_touch_to_state(config, Point{465, 263}), PanelState::Field2);
  EXPECT_EQ(rpi_ros2_hmi_panel::map_touch_to_state(config, Point{465, 264}), PanelState::Field4);
}

TEST(TouchLayout, GivenUnsupportedLayoutMode_WhenMappingTouch_ThenNoStateIsReturned)
{
  LayoutConfig config{};
  config.layout_mode = "unknown";

  EXPECT_FALSE(rpi_ros2_hmi_panel::map_touch_to_state(config, Point{10, 60}).has_value());
}

TEST(TouchLayout, GivenCustomDimensions_WhenMappingTouch_ThenUsesConfiguredScreen)
{
  LayoutConfig config{};
  config.screen_width_px = 320;
  config.screen_height_px = 240;
  config.status_bar_height_px = 24;
  config.center_indicator_radius_px = 20;

  EXPECT_EQ(rpi_ros2_hmi_panel::map_touch_to_state(config, Point{1, 25}), PanelState::Field1);
  EXPECT_EQ(rpi_ros2_hmi_panel::map_touch_to_state(config, Point{319, 25}), PanelState::Field2);
  EXPECT_EQ(rpi_ros2_hmi_panel::map_touch_to_state(config, Point{1, 239}), PanelState::Field3);
  EXPECT_EQ(rpi_ros2_hmi_panel::map_touch_to_state(config, Point{319, 239}), PanelState::Field4);
  EXPECT_FALSE(rpi_ros2_hmi_panel::map_touch_to_state(config, Point{160, 132}).has_value());
}

TEST(TouchLayout, GivenRotation_WhenApplied_ThenCoordinatesRotateClockwise)
{
  EXPECT_EQ((rpi_ros2_hmi_panel::apply_rotation(Point{1, 2}, 10, 20, 0).x), 1);
  EXPECT_EQ((rpi_ros2_hmi_panel::apply_rotation(Point{1, 2}, 10, 20, 0).y), 2);
  EXPECT_EQ((rpi_ros2_hmi_panel::apply_rotation(Point{1, 2}, 10, 20, 90).x), 17);
  EXPECT_EQ((rpi_ros2_hmi_panel::apply_rotation(Point{1, 2}, 10, 20, 90).y), 1);
  EXPECT_EQ((rpi_ros2_hmi_panel::apply_rotation(Point{1, 2}, 10, 20, 180).x), 8);
  EXPECT_EQ((rpi_ros2_hmi_panel::apply_rotation(Point{1, 2}, 10, 20, 180).y), 17);
  EXPECT_EQ((rpi_ros2_hmi_panel::apply_rotation(Point{1, 2}, 10, 20, 270).x), 2);
  EXPECT_EQ((rpi_ros2_hmi_panel::apply_rotation(Point{1, 2}, 10, 20, 270).y), 8);
  EXPECT_EQ((rpi_ros2_hmi_panel::apply_rotation(Point{1, 2}, 10, 20, 45).x), 1);
  EXPECT_EQ((rpi_ros2_hmi_panel::apply_rotation(Point{1, 2}, 10, 20, 45).y), 2);
}

TEST(TouchDebounce, GivenSameFieldWithinWindow_WhenEvaluated_ThenSecondTouchIsSuppressed)
{
  TouchDebouncer debouncer{250};

  EXPECT_TRUE(debouncer.should_accept(PanelState::Field1, 1000));
  EXPECT_FALSE(debouncer.should_accept(PanelState::Field1, 1100));
  EXPECT_TRUE(debouncer.should_accept(PanelState::Field1, 1250));
}

TEST(TouchDebounce, GivenDifferentFieldWithinWindow_WhenEvaluated_ThenTouchIsAccepted)
{
  TouchDebouncer debouncer{250};

  EXPECT_TRUE(debouncer.should_accept(PanelState::Field1, 1000));
  EXPECT_TRUE(debouncer.should_accept(PanelState::Field2, 1100));
}

TEST(TouchDebounce, GivenNegativeDebounce_WhenEvaluated_ThenItBehavesAsZero)
{
  TouchDebouncer debouncer{-1};

  EXPECT_TRUE(debouncer.should_accept(PanelState::Field1, 1000));
  EXPECT_TRUE(debouncer.should_accept(PanelState::Field1, 1000));
}

TEST(TouchDebounce, GivenClockMovesBackward_WhenSameStateEvaluated_ThenTouchIsSuppressed)
{
  TouchDebouncer debouncer{250};

  EXPECT_TRUE(debouncer.should_accept(PanelState::Field1, 1000));
  EXPECT_FALSE(debouncer.should_accept(PanelState::Field1, 900));
}

TEST(TouchProcessor, GivenCoordinatesAfterPress_WhenFrameSyncs_ThenCurrentCoordinatesAreEmitted)
{
  const LayoutConfig config{};
  std::vector<Point> touches;
  rpi_ros2_hmi_panel::PressFrameTouchProcessor processor{
      config, [&touches](const Point point) { touches.push_back(point); }};

  processor.update_x(10);
  processor.update_y(60);
  processor.set_pressed(true);
  processor.update_x(700);
  processor.update_y(400);
  processor.sync_frame();

  ASSERT_EQ(touches.size(), 1U);
  EXPECT_EQ(touches[0].x, 700);
  EXPECT_EQ(touches[0].y, 400);
}

TEST(TouchProcessor, GivenRepeatedPressWhileHeld_WhenFrameSyncs_ThenOnlyOneTouchIsEmitted)
{
  const LayoutConfig config{};
  std::vector<Point> touches;
  rpi_ros2_hmi_panel::PressFrameTouchProcessor processor{
      config, [&touches](const Point point) { touches.push_back(point); }};

  processor.update_x(10);
  processor.update_y(60);
  processor.set_pressed(true);
  processor.sync_frame();
  processor.update_x(700);
  processor.update_y(400);
  processor.set_pressed(true);
  processor.sync_frame();

  ASSERT_EQ(touches.size(), 1U);
  EXPECT_EQ(touches[0].x, 10);
  EXPECT_EQ(touches[0].y, 60);
}

TEST(TouchProcessor, GivenReleaseAndNewPress_WhenFrameSyncs_ThenSecondTouchIsEmitted)
{
  const LayoutConfig config{};
  std::vector<Point> touches;
  rpi_ros2_hmi_panel::PressFrameTouchProcessor processor{
      config, [&touches](const Point point) { touches.push_back(point); }};

  processor.update_x(10);
  processor.update_y(60);
  processor.set_pressed(true);
  processor.sync_frame();
  processor.set_pressed(false);
  processor.sync_frame();
  processor.update_x(700);
  processor.update_y(400);
  processor.set_pressed(true);
  processor.sync_frame();

  ASSERT_EQ(touches.size(), 2U);
  EXPECT_EQ(touches[0].x, 10);
  EXPECT_EQ(touches[0].y, 60);
  EXPECT_EQ(touches[1].x, 700);
  EXPECT_EQ(touches[1].y, 400);
}

TEST(TouchProcessor, GivenMissingCoordinate_WhenFrameSyncs_ThenNoTouchIsEmitted)
{
  const LayoutConfig config{};
  std::vector<Point> touches;
  rpi_ros2_hmi_panel::PressFrameTouchProcessor processor{
      config, [&touches](const Point point) { touches.push_back(point); }};

  processor.update_x(10);
  processor.set_pressed(true);
  processor.sync_frame();

  EXPECT_TRUE(touches.empty());
}

TEST(TouchProcessor, GivenRotation_WhenFrameSyncs_ThenRotatedCoordinatesAreEmitted)
{
  LayoutConfig config{};
  config.rotation_degrees = 180;
  std::vector<Point> touches;
  rpi_ros2_hmi_panel::PressFrameTouchProcessor processor{
      config, [&touches](const Point point) { touches.push_back(point); }};

  processor.update_x(1);
  processor.update_y(2);
  processor.set_pressed(true);
  processor.sync_frame();

  ASSERT_EQ(touches.size(), 1U);
  EXPECT_EQ(touches[0].x, 798);
  EXPECT_EQ(touches[0].y, 477);
}

TEST(JsonProtocol, GivenRequests_WhenParsed_ThenReturnsExpectedResult)
{
  EXPECT_TRUE(rpi_ros2_hmi_panel::parse_json_request(R"({"command":"get_state"})", 1024).get_state);
  EXPECT_EQ(rpi_ros2_hmi_panel::parse_json_request("{", 1024).error, RequestError::MalformedJson);
  EXPECT_EQ(rpi_ros2_hmi_panel::parse_json_request(R"({})", 1024).error,
            RequestError::MissingCommand);
  EXPECT_EQ(rpi_ros2_hmi_panel::parse_json_request(R"({"command":"set_state"})", 1024).error,
            RequestError::UnsupportedCommand);
  EXPECT_EQ(rpi_ros2_hmi_panel::parse_json_request("{}", 1).error, RequestError::RequestTooLarge);
}

TEST(JsonProtocol, GivenMalformedCommandTypes_WhenParsed_ThenUnsupportedCommandIsReturned)
{
  EXPECT_EQ(rpi_ros2_hmi_panel::parse_json_request(R"({"command":2})", 1024).error,
            RequestError::UnsupportedCommand);
  EXPECT_EQ(rpi_ros2_hmi_panel::parse_json_request(R"({"command":null})", 1024).error,
            RequestError::UnsupportedCommand);
  EXPECT_EQ(rpi_ros2_hmi_panel::parse_json_request(R"({"command":""})", 1024).error,
            RequestError::UnsupportedCommand);
  EXPECT_EQ(rpi_ros2_hmi_panel::parse_json_request(R"([])", 1024).error,
            RequestError::MissingCommand);
}

TEST(JsonProtocol, GivenMaximumRequestSize_WhenParsed_ThenEqualSizeIsAccepted)
{
  const std::string request = R"({"command":"get_state"})";

  EXPECT_EQ(rpi_ros2_hmi_panel::parse_json_request(request, request.size()).error,
            RequestError::None);
  EXPECT_EQ(rpi_ros2_hmi_panel::parse_json_request(request, request.size() - 1).error,
            RequestError::RequestTooLarge);
}

TEST(JsonProtocol, GivenErrorCodes_WhenResponseBuilt_ThenExpectedErrorNamesAreUsed)
{
  EXPECT_EQ(rpi_ros2_hmi_panel::request_error_name(RequestError::MalformedJson),
            "malformed_json");
  EXPECT_EQ(rpi_ros2_hmi_panel::request_error_name(RequestError::MissingCommand),
            "missing_command");
  EXPECT_EQ(rpi_ros2_hmi_panel::request_error_name(RequestError::UnsupportedCommand),
            "unsupported_command");
  EXPECT_EQ(rpi_ros2_hmi_panel::request_error_name(RequestError::RequestTooLarge),
            "request_too_large");
  EXPECT_EQ(rpi_ros2_hmi_panel::request_error_name(RequestError::StateUnavailable),
            "state_unavailable");

  const auto response = rpi_ros2_hmi_panel::make_error_response(RequestError::StateUnavailable);
  EXPECT_NE(response.find(R"("success":false)"), std::string::npos);
  EXPECT_NE(response.find(R"("error":"state_unavailable")"), std::string::npos);
  EXPECT_EQ(response.back(), '\n');
}

TEST(JsonProtocol, GivenState_WhenResponseBuilt_ThenContainsExpectedFields)
{
  const auto response =
      rpi_ros2_hmi_panel::make_state_response(PanelState::Field2, "2026-05-04T12:34:56.123Z",
                                              "touchscreen_hmi_node");

  EXPECT_NE(response.find(R"("success":true)"), std::string::npos);
  EXPECT_NE(response.find(R"("state":2)"), std::string::npos);
  EXPECT_NE(response.find(R"("state_name":"FIELD_2")"), std::string::npos);
  EXPECT_EQ(response.back(), '\n');
}

}  // namespace
