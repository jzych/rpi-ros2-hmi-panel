#include "rpi_ros2_hmi_panel/json_protocol.hpp"
#include "rpi_ros2_hmi_panel/panel_state.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <ctime>
#include <future>
#include <iomanip>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "panel_interfaces/msg/panel_state.hpp"
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/empty.hpp"

namespace {

using PanelStateMsg = panel_interfaces::msg::PanelState;
using ShutdownMsg = std_msgs::msg::Empty;
using rpi_ros2_hmi_panel::PanelState;

[[nodiscard]] std::string stamp_to_iso8601(const builtin_interfaces::msg::Time& stamp)
{
  std::time_t seconds = stamp.sec;
  std::tm utc_time{};
  gmtime_r(&seconds, &utc_time);

  std::ostringstream output;
  output << std::put_time(&utc_time, "%Y-%m-%dT%H:%M:%S") << '.'
         << std::setfill('0') << std::setw(3) << (stamp.nanosec / 1000000U) << 'Z';
  return output.str();
}

void send_all(const int socket_fd, const std::string& payload)
{
  const char* data = payload.data();
  auto remaining = payload.size();
  while (remaining > 0) {
    const auto sent = ::send(socket_fd, data, remaining, MSG_NOSIGNAL);
    if (sent <= 0) {
      return;
    }
    data += sent;
    remaining -= static_cast<std::size_t>(sent);
  }
}

class EthernetStateServerNode final : public rclcpp::Node {
 public:
  EthernetStateServerNode() : rclcpp::Node("ethernet_state_server_node")
  {
    bind_address_ = declare_parameter<std::string>("bind_address", "0.0.0.0");
    port_ = declare_parameter<int>("port", 5050);
    maximum_request_size_bytes_ = declare_parameter<int>("maximum_request_size_bytes", 1024);
    client_timeout_ms_ = declare_parameter<int>("client_timeout_ms", 5000);
    max_clients_ = declare_parameter<int>("max_clients", 8);

    state_subscription_ = create_subscription<PanelStateMsg>(
        "/panel/state", rclcpp::QoS(rclcpp::KeepLast(10)).reliable(),
        [this](const PanelStateMsg& message) {
          std::lock_guard<std::mutex> lock(state_mutex_);
          latest_state_ = StateSnapshot{
              rpi_ros2_hmi_panel::panel_state_from_wire(message.state),
              message.stamp,
              message.source,
          };
        });
    shutdown_subscription_ = create_subscription<ShutdownMsg>(
        "/panel/shutdown", rclcpp::QoS(rclcpp::KeepLast(1)).reliable(),
        [this](const ShutdownMsg&) {
          RCLCPP_INFO(get_logger(), "Application shutdown requested");
          rclcpp::shutdown();
        });

    server_thread_ = std::thread([this]() { run_server(); });
  }

  ~EthernetStateServerNode() override
  {
    running_.store(false);
    if (server_socket_fd_ >= 0) {
      ::shutdown(server_socket_fd_, SHUT_RDWR);
      ::close(server_socket_fd_);
    }
    if (server_thread_.joinable()) {
      server_thread_.join();
    }

    std::vector<std::future<void>> futures;
    {
      std::lock_guard<std::mutex> lock(client_threads_mutex_);
      futures.swap(client_futures_);
    }
    // Each future's destructor blocks until the client handler returns.
  }

 private:
  struct StateSnapshot {
    PanelState state;
    builtin_interfaces::msg::Time stamp;
    std::string source;
  };

  [[nodiscard]] std::optional<StateSnapshot> latest_state() const
  {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return latest_state_;
  }

  void run_server()
  {
    server_socket_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_fd_ < 0) {
      RCLCPP_ERROR(get_logger(), "Failed to create TCP socket: %s", std::strerror(errno));
      return;
    }

    int reuse_address = 1;
    ::setsockopt(server_socket_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse_address, sizeof(reuse_address));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(static_cast<uint16_t>(port_));
    if (::inet_pton(AF_INET, bind_address_.c_str(), &address.sin_addr) != 1) {
      RCLCPP_ERROR(get_logger(), "Invalid bind_address '%s'", bind_address_.c_str());
      return;
    }

    if (::bind(server_socket_fd_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
      RCLCPP_ERROR(get_logger(), "Failed to bind TCP server on %s:%d: %s", bind_address_.c_str(),
                   port_, std::strerror(errno));
      return;
    }

    if (::listen(server_socket_fd_, max_clients_) < 0) {
      RCLCPP_ERROR(get_logger(), "Failed to listen on TCP socket: %s", std::strerror(errno));
      return;
    }

    RCLCPP_INFO(get_logger(), "Ethernet state server listening on %s:%d", bind_address_.c_str(),
                port_);

    while (running_.load()) {
      sockaddr_in client_address{};
      socklen_t client_length = sizeof(client_address);
      const auto client_fd =
          ::accept(server_socket_fd_, reinterpret_cast<sockaddr*>(&client_address), &client_length);
      if (client_fd < 0) {
        if (running_.load()) {
          RCLCPP_WARN(get_logger(), "TCP accept failed: %s", std::strerror(errno));
        }
        continue;
      }

      std::lock_guard<std::mutex> lock(client_threads_mutex_);
      // Remove futures for connections that have already finished.
      client_futures_.erase(
          std::remove_if(
              client_futures_.begin(), client_futures_.end(),
              [](std::future<void>& f) {
                return f.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
              }),
          client_futures_.end());

      if (static_cast<int>(client_futures_.size()) >= max_clients_) {
        RCLCPP_WARN(get_logger(), "Connection limit reached (%d), rejecting new client",
                    max_clients_);
        ::close(client_fd);
        continue;
      }
      client_futures_.push_back(
          std::async(std::launch::async, [this, client_fd]() { handle_client(client_fd); }));
    }
  }

  void handle_client(const int client_fd)
  {
    std::string buffer;
    buffer.reserve(static_cast<std::size_t>(maximum_request_size_bytes_) + 1U);

    while (running_.load()) {
      fd_set read_set;
      FD_ZERO(&read_set);
      FD_SET(client_fd, &read_set);

      timeval timeout{};
      timeout.tv_sec = client_timeout_ms_ / 1000;
      timeout.tv_usec = (client_timeout_ms_ % 1000) * 1000;

      const auto ready = ::select(client_fd + 1, &read_set, nullptr, nullptr, &timeout);
      if (ready <= 0) {
        break;
      }

      char chunk[256];
      const auto bytes_read = ::recv(client_fd, chunk, sizeof(chunk), 0);
      if (bytes_read <= 0) {
        break;
      }

      buffer.append(chunk, static_cast<std::size_t>(bytes_read));
      if (buffer.size() > static_cast<std::size_t>(maximum_request_size_bytes_)) {
        send_all(client_fd, rpi_ros2_hmi_panel::make_error_response(
                                rpi_ros2_hmi_panel::RequestError::RequestTooLarge));
        break;
      }

      std::size_t newline_position = std::string::npos;
      while ((newline_position = buffer.find('\n')) != std::string::npos) {
        auto line = buffer.substr(0, newline_position);
        if (!line.empty() && line.back() == '\r') {
          line.pop_back();
        }
        buffer.erase(0, newline_position + 1);
        handle_request_line(client_fd, line);
      }
    }

    ::shutdown(client_fd, SHUT_RDWR);
    ::close(client_fd);
  }

  void handle_request_line(const int client_fd, const std::string& line)
  {
    const auto parsed =
        rpi_ros2_hmi_panel::parse_json_request(line, static_cast<std::size_t>(maximum_request_size_bytes_));
    if (parsed.error != rpi_ros2_hmi_panel::RequestError::None) {
      send_all(client_fd, rpi_ros2_hmi_panel::make_error_response(parsed.error));
      return;
    }

    const auto snapshot = latest_state();
    if (!snapshot.has_value()) {
      send_all(client_fd, rpi_ros2_hmi_panel::make_error_response(
                              rpi_ros2_hmi_panel::RequestError::StateUnavailable));
      return;
    }

    send_all(client_fd,
             rpi_ros2_hmi_panel::make_state_response(
                 snapshot->state, stamp_to_iso8601(snapshot->stamp), snapshot->source));
  }

  std::string bind_address_{"0.0.0.0"};
  int port_{5050};
  int maximum_request_size_bytes_{1024};
  int client_timeout_ms_{5000};
  int max_clients_{8};

  std::atomic<bool> running_{true};
  int server_socket_fd_{-1};
  std::thread server_thread_;

  mutable std::mutex state_mutex_;
  std::optional<StateSnapshot> latest_state_;
  rclcpp::Subscription<PanelStateMsg>::SharedPtr state_subscription_;
  rclcpp::Subscription<ShutdownMsg>::SharedPtr shutdown_subscription_;

  std::mutex client_threads_mutex_;
  std::vector<std::future<void>> client_futures_;
};

}  // namespace

int main(int argc, char* argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<EthernetStateServerNode>());
  rclcpp::shutdown();
  return 0;
}
