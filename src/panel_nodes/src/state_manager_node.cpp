#include "rpi_ros2_hmi_panel/panel_state.hpp"

#include <chrono>
#include <memory>
#include <mutex>
#include <string>

#include "panel_interfaces/msg/panel_state.hpp"
#include "panel_interfaces/srv/get_panel_state.hpp"
#include "panel_interfaces/srv/set_panel_state.hpp"
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/empty.hpp"

namespace {

using namespace std::chrono_literals;
using PanelStateMsg = panel_interfaces::msg::PanelState;
using GetPanelState = panel_interfaces::srv::GetPanelState;
using SetPanelState = panel_interfaces::srv::SetPanelState;
using ShutdownMsg = std_msgs::msg::Empty;
using rpi_ros2_hmi_panel::PanelState;

class StateManagerNode final : public rclcpp::Node {
 public:
  StateManagerNode() : rclcpp::Node("state_manager_node")
  {
    state_publisher_ = create_publisher<PanelStateMsg>(
        "/panel/state", rclcpp::QoS(rclcpp::KeepLast(10)).reliable());

    get_state_service_ = create_service<GetPanelState>(
        "/panel/get_state",
        [this](
            const std::shared_ptr<GetPanelState::Request>,
            const std::shared_ptr<GetPanelState::Response> response) {
          const auto snapshot = snapshot_state();
          response->state = rpi_ros2_hmi_panel::panel_state_to_wire(snapshot.state);
          response->stamp = snapshot.stamp;
          response->source = snapshot.source;
        });

    set_state_service_ = create_service<SetPanelState>(
        "/panel/set_state",
        [this](
            const std::shared_ptr<SetPanelState::Request> request,
            const std::shared_ptr<SetPanelState::Response> response) {
          const auto requested_state =
              rpi_ros2_hmi_panel::panel_state_from_wire(request->state);
          if (!rpi_ros2_hmi_panel::is_valid_requested_state(requested_state)) {
            response->success = false;
            response->message = "invalid_state";
            return;
          }

          accept_state(requested_state, request->source.empty() ? name_ : request->source);
          response->success = true;
          response->message = "accepted";
        });

    republish_timer_ = create_wall_timer(1s, [this]() { publish_state(snapshot_state()); });
    shutdown_subscription_ = create_subscription<ShutdownMsg>(
        "/panel/shutdown", rclcpp::QoS(rclcpp::KeepLast(1)).reliable(),
        [this](const ShutdownMsg&) {
          RCLCPP_INFO(get_logger(), "Application shutdown requested");
          rclcpp::shutdown();
        });

    accept_state(PanelState::Idle, name_);
  }

 private:
  struct StateSnapshot {
    PanelState state;
    builtin_interfaces::msg::Time stamp;
    std::string source;
  };

  [[nodiscard]] StateSnapshot snapshot_state() const
  {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return current_state_;
  }

  void accept_state(const PanelState state, const std::string& source)
  {
    StateSnapshot snapshot;
    {
      std::lock_guard<std::mutex> lock(state_mutex_);
      current_state_ = StateSnapshot{state, now(), source};
      snapshot = current_state_;
    }
    publish_state(snapshot);
  }

  void publish_state(const StateSnapshot& snapshot) const
  {
    PanelStateMsg message;
    message.state = rpi_ros2_hmi_panel::panel_state_to_wire(snapshot.state);
    message.stamp = snapshot.stamp;
    message.source = snapshot.source;
    state_publisher_->publish(message);
  }

  const std::string name_{"state_manager_node"};
  mutable std::mutex state_mutex_;
  StateSnapshot current_state_{PanelState::Unknown, builtin_interfaces::msg::Time{}, name_};
  rclcpp::Publisher<PanelStateMsg>::SharedPtr state_publisher_;
  rclcpp::Service<GetPanelState>::SharedPtr get_state_service_;
  rclcpp::Service<SetPanelState>::SharedPtr set_state_service_;
  rclcpp::TimerBase::SharedPtr republish_timer_;
  rclcpp::Subscription<ShutdownMsg>::SharedPtr shutdown_subscription_;
};

}  // namespace

int main(int argc, char* argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<StateManagerNode>());
  rclcpp::shutdown();
  return 0;
}
