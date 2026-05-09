#include "rpi_ros2_hmi_panel/panel_state.hpp"
#include "rpi_ros2_hmi_panel/touch_debounce.hpp"
#include "rpi_ros2_hmi_panel/touch_layout.hpp"
#include "rpi_ros2_hmi_panel/touch_processor.hpp"

#include <fcntl.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <SDL2/SDL.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstring>
#include <ctime>
#include <functional>
#include <iomanip>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>

#include "panel_interfaces/msg/panel_state.hpp"
#include "panel_interfaces/srv/set_panel_state.hpp"
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/empty.hpp"

namespace {

using namespace std::chrono_literals;
using PanelStateMsg = panel_interfaces::msg::PanelState;
using SetPanelState = panel_interfaces::srv::SetPanelState;
using ShutdownMsg = std_msgs::msg::Empty;
using rpi_ros2_hmi_panel::LayoutConfig;
using rpi_ros2_hmi_panel::PanelState;
using rpi_ros2_hmi_panel::Point;

struct Color {
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t a{255};
};

[[nodiscard]] Color color_for_state(const PanelState state)
{
  switch (state) {
    case PanelState::Field1:
      return Color{210, 40, 40, 255};
    case PanelState::Field2:
      return Color{45, 105, 220, 255};
    case PanelState::Field3:
      return Color{35, 170, 80, 255};
    case PanelState::Field4:
      return Color{230, 205, 35, 255};
    case PanelState::Idle:
      return Color{90, 90, 90, 255};
    case PanelState::Unknown:
      return Color{45, 45, 45, 255};
  }

  return Color{45, 45, 45, 255};  // unreachable: all enum values handled above
}

void set_render_color(SDL_Renderer* renderer, const Color color)
{
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
}

void fill_circle(SDL_Renderer* renderer, const int center_x, const int center_y, const int radius)
{
  for (int dy = -radius; dy <= radius; ++dy) {
    const auto dx = static_cast<int>(std::sqrt((radius * radius) - (dy * dy)));
    SDL_RenderDrawLine(renderer, center_x - dx, center_y + dy, center_x + dx, center_y + dy);
  }
}

[[nodiscard]] std::array<std::string_view, 7> glyph_for(const char character)
{
  switch (static_cast<char>(std::toupper(static_cast<unsigned char>(character)))) {
    case '0':
      return {"111", "101", "101", "101", "101", "101", "111"};
    case '1':
      return {"010", "110", "010", "010", "010", "010", "111"};
    case '2':
      return {"111", "001", "001", "111", "100", "100", "111"};
    case '3':
      return {"111", "001", "001", "111", "001", "001", "111"};
    case '4':
      return {"101", "101", "101", "111", "001", "001", "001"};
    case '5':
      return {"111", "100", "100", "111", "001", "001", "111"};
    case '6':
      return {"111", "100", "100", "111", "101", "101", "111"};
    case '7':
      return {"111", "001", "001", "010", "010", "010", "010"};
    case '8':
      return {"111", "101", "101", "111", "101", "101", "111"};
    case '9':
      return {"111", "101", "101", "111", "001", "001", "111"};
    case 'A':
      return {"010", "101", "101", "111", "101", "101", "101"};
    case 'C':
      return {"111", "100", "100", "100", "100", "100", "111"};
    case 'D':
      return {"110", "101", "101", "101", "101", "101", "110"};
    case 'E':
      return {"111", "100", "100", "111", "100", "100", "111"};
    case 'H':
      return {"101", "101", "101", "111", "101", "101", "101"};
    case 'I':
      return {"111", "010", "010", "010", "010", "010", "111"};
    case 'L':
      return {"100", "100", "100", "100", "100", "100", "111"};
    case 'M':
      return {"101", "111", "111", "101", "101", "101", "101"};
    case 'N':
      return {"101", "111", "111", "111", "111", "111", "101"};
    case 'O':
      return {"111", "101", "101", "101", "101", "101", "111"};
    case 'P':
      return {"111", "101", "101", "111", "100", "100", "100"};
    case 'R':
      return {"110", "101", "101", "110", "101", "101", "101"};
    case 'S':
      return {"111", "100", "100", "111", "001", "001", "111"};
    case 'T':
      return {"111", "010", "010", "010", "010", "010", "010"};
    case 'U':
      return {"101", "101", "101", "101", "101", "101", "111"};
    case 'V':
      return {"101", "101", "101", "101", "101", "101", "010"};
    case 'W':
      return {"101", "101", "101", "101", "111", "111", "101"};
    case ':':
      return {"000", "010", "010", "000", "010", "010", "000"};
    case '-':
      return {"000", "000", "000", "111", "000", "000", "000"};
    case '_':
      return {"000", "000", "000", "000", "000", "000", "111"};
    case '/':
      return {"001", "001", "010", "010", "100", "100", "100"};
    case '.':
      return {"000", "000", "000", "000", "000", "010", "010"};
    default:
      return {"000", "000", "000", "000", "000", "000", "000"};
  }
}

void draw_text(
    SDL_Renderer* renderer,
    const std::string_view text,
    const int x,
    const int y,
    const int scale,
    const Color color,
    const rclcpp::Logger& logger)
{
  static constexpr std::array<std::string_view, 7> kBlankGlyph{
      "000", "000", "000", "000", "000", "000", "000"};
  set_render_color(renderer, color);
  int cursor_x = x;
  for (const auto character : text) {
    const auto glyph = glyph_for(character);
    if (character != ' ' && glyph == kBlankGlyph) {
      RCLCPP_WARN_ONCE(logger, "No glyph for character 0x%02X; renders blank",
                       static_cast<unsigned char>(character));
    }
    for (int row = 0; row < 7; ++row) {
      for (int col = 0; col < 3; ++col) {
        if (glyph[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)] == '1') {
          const SDL_Rect pixel{cursor_x + (col * scale), y + (row * scale), scale, scale};
          SDL_RenderFillRect(renderer, &pixel);
        }
      }
    }
    cursor_x += 4 * scale;
  }
}

[[nodiscard]] std::string current_time_hhmmss()
{
  const auto now = std::time(nullptr);
  std::tm local_time{};
  localtime_r(&now, &local_time);
  std::ostringstream output;
  output << std::put_time(&local_time, "%H:%M:%S");
  return output.str();
}

[[nodiscard]] int scale_raw_axis(const int value, const input_absinfo& info, const int target_max)
{
  if (info.maximum <= info.minimum || target_max <= 1) {
    return value;
  }

  const auto clamped = std::clamp(value, info.minimum, info.maximum);
  const auto normalized =
      static_cast<double>(clamped - info.minimum) / static_cast<double>(info.maximum - info.minimum);
  return static_cast<int>(normalized * static_cast<double>(target_max - 1));
}

class EvdevTouchReader {
 public:
  using TouchCallback = std::function<void(Point)>;

  EvdevTouchReader(
      std::string input_device_path,
      LayoutConfig layout_config,
      rclcpp::Logger logger,
      TouchCallback callback)
      : input_device_path_(std::move(input_device_path)),
        layout_config_(std::move(layout_config)),
        logger_(logger),
        callback_(std::move(callback))
  {
  }

  void start()
  {
    running_.store(true);
    thread_ = std::thread([this]() { run(); });
  }

  void stop()
  {
    running_.store(false);
    if (thread_.joinable()) {
      thread_.join();
    }
  }

 private:
  void run()
  {
    while (running_.load()) {
      const auto fd = ::open(input_device_path_.c_str(), O_RDONLY | O_NONBLOCK);
      if (fd < 0) {
        const auto now = std::chrono::steady_clock::now();
        if (now - last_open_warning_ > 5s) {
          RCLCPP_WARN(logger_, "Unable to open input device '%s': %s", input_device_path_.c_str(),
                      std::strerror(errno));
          last_open_warning_ = now;
        }
        std::this_thread::sleep_for(1s);
        continue;
      }

      read_loop(fd);
      ::close(fd);
      std::this_thread::sleep_for(250ms);
    }
  }

  void read_loop(const int fd)
  {
    input_absinfo abs_x{};
    input_absinfo abs_y{};
    if (::ioctl(fd, EVIOCGABS(ABS_X), &abs_x) < 0) {
      RCLCPP_WARN(logger_, "EVIOCGABS(ABS_X) failed: %s; touch X coordinates will be unscaled",
                  std::strerror(errno));
    }
    if (::ioctl(fd, EVIOCGABS(ABS_Y), &abs_y) < 0) {
      RCLCPP_WARN(logger_, "EVIOCGABS(ABS_Y) failed: %s; touch Y coordinates will be unscaled",
                  std::strerror(errno));
    }

    rpi_ros2_hmi_panel::PressFrameTouchProcessor processor{
        layout_config_, [this](const Point point) { callback_(point); }};

    while (running_.load()) {
      input_event event{};
      const auto bytes_read = ::read(fd, &event, sizeof(event));
      if (bytes_read == static_cast<ssize_t>(sizeof(event))) {
        if (event.type == EV_ABS && event.code == ABS_X) {
          processor.update_x(scale_raw_axis(event.value, abs_x, layout_config_.screen_width_px));
        } else if (event.type == EV_ABS && event.code == ABS_Y) {
          processor.update_y(scale_raw_axis(event.value, abs_y, layout_config_.screen_height_px));
        } else if (event.type == EV_KEY && event.code == BTN_TOUCH) {
          processor.set_pressed(event.value != 0);
        } else if (event.type == EV_SYN && event.code == SYN_REPORT) {
          processor.sync_frame();
        }
      } else if (bytes_read < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        std::this_thread::sleep_for(5ms);
      } else if (bytes_read == 0 || (bytes_read < 0 && errno != EINTR)) {
        break;
      }
    }
  }

  std::string input_device_path_;
  LayoutConfig layout_config_;
  rclcpp::Logger logger_;
  TouchCallback callback_;
  std::chrono::steady_clock::time_point last_open_warning_{};
  std::atomic<bool> running_{false};
  std::thread thread_;
};

class TouchscreenHmiNode final : public rclcpp::Node {
 public:
  TouchscreenHmiNode() : rclcpp::Node("touchscreen_hmi_node")
  {
    input_device_path_ = declare_parameter<std::string>(
        "input_device_path", "/dev/input/by-path/platform-3f204000.spi-cs-1-event");
    debounce_ms_ = declare_parameter<int>("debounce_ms", 250);
    layout_config_.screen_width_px = declare_parameter<int>("screen_width_px", 800);
    layout_config_.screen_height_px = declare_parameter<int>("screen_height_px", 480);
    layout_config_.rotation_degrees = declare_parameter<int>("rotation_degrees", 0);
    if (layout_config_.rotation_degrees != 0 && layout_config_.rotation_degrees != 90 &&
        layout_config_.rotation_degrees != 180 && layout_config_.rotation_degrees != 270) {
      RCLCPP_ERROR(get_logger(),
                   "rotation_degrees must be 0, 90, 180, or 270; got %d, defaulting to 0",
                   layout_config_.rotation_degrees);
      layout_config_.rotation_degrees = 0;
    }
    layout_config_.status_bar_height_px = declare_parameter<int>("status_bar_height_px", 48);
    layout_config_.center_indicator_radius_px =
        declare_parameter<int>("center_indicator_radius_px", 64);
    layout_config_.layout_mode = declare_parameter<std::string>("layout_mode", "quadrants");
    device_name_ = declare_parameter<std::string>("device_name", "RPI-HMI-PANEL");
    fullscreen_ = declare_parameter<bool>("fullscreen", true);

    set_state_client_ = create_client<SetPanelState>("/panel/set_state");
    shutdown_publisher_ = create_publisher<ShutdownMsg>(
        "/panel/shutdown", rclcpp::QoS(rclcpp::KeepLast(1)).reliable());
    shutdown_subscription_ = create_subscription<ShutdownMsg>(
        "/panel/shutdown", rclcpp::QoS(rclcpp::KeepLast(1)).reliable(),
        [this](const ShutdownMsg&) {
          RCLCPP_INFO(get_logger(), "Application shutdown requested");
          rclcpp::shutdown();
        });
    state_subscription_ = create_subscription<PanelStateMsg>(
        "/panel/state", rclcpp::QoS(rclcpp::KeepLast(10)).reliable(),
        [this](const PanelStateMsg& message) {
          std::lock_guard<std::mutex> lock(state_mutex_);
          current_state_ = rpi_ros2_hmi_panel::panel_state_from_wire(message.state);
        });

    initialize_sdl();

    debouncer_ = std::make_unique<rpi_ros2_hmi_panel::TouchDebouncer>(debounce_ms_);
    touch_reader_ = std::make_unique<EvdevTouchReader>(
        input_device_path_, layout_config_, get_logger(), [this](const Point point) {
          handle_touch(point);
        });
    touch_reader_->start();

    render_timer_ = create_wall_timer(100ms, [this]() { render(); });
  }

  ~TouchscreenHmiNode() override
  {
    if (touch_reader_) {
      touch_reader_->stop();
    }
    if (renderer_ != nullptr) {
      SDL_DestroyRenderer(renderer_);
    }
    if (window_ != nullptr) {
      SDL_DestroyWindow(window_);
    }
    SDL_Quit();
  }

 private:
  void initialize_sdl()
  {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
      RCLCPP_ERROR(get_logger(), "SDL_Init failed: %s", SDL_GetError());
      return;
    }

    uint32_t flags = SDL_WINDOW_SHOWN;
    if (fullscreen_) {
      flags |= SDL_WINDOW_FULLSCREEN;
    }

    window_ = SDL_CreateWindow("ROS 2 HMI Panel", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                               layout_config_.screen_width_px, layout_config_.screen_height_px,
                               flags);
    if (window_ == nullptr && fullscreen_) {
      RCLCPP_WARN(get_logger(), "Fullscreen SDL window failed: %s; retrying windowed",
                  SDL_GetError());
      window_ = SDL_CreateWindow("ROS 2 HMI Panel", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                 layout_config_.screen_width_px, layout_config_.screen_height_px,
                                 SDL_WINDOW_SHOWN);
    }

    if (window_ == nullptr) {
      RCLCPP_ERROR(get_logger(), "SDL_CreateWindow failed: %s", SDL_GetError());
      return;
    }

    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer_ == nullptr) {
      renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_SOFTWARE);
    }

    if (renderer_ == nullptr) {
      RCLCPP_ERROR(get_logger(), "SDL_CreateRenderer failed: %s", SDL_GetError());
    }
  }

  void handle_touch(const Point point)
  {
    const auto state = rpi_ros2_hmi_panel::map_touch_to_state(layout_config_, point);
    if (!state.has_value() || !rpi_ros2_hmi_panel::is_touch_selectable_state(*state)) {
      return;
    }

    const auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now().time_since_epoch())
                            .count();
    if (!debouncer_->should_accept(*state, now_ms)) {
      return;
    }

    if (!set_state_client_->service_is_ready()) {
      RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 2000, "/panel/set_state is not ready");
      return;
    }

    auto request = std::make_shared<SetPanelState::Request>();
    request->state = rpi_ros2_hmi_panel::panel_state_to_wire(*state);
    request->source = "touchscreen_hmi_node";
    set_state_client_->async_send_request(request);
  }

  void request_application_shutdown()
  {
    ShutdownMsg message;
    shutdown_publisher_->publish(message);
    RCLCPP_INFO(get_logger(), "Published application shutdown request");
    rclcpp::shutdown();
  }

  [[nodiscard]] PanelState current_state() const
  {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return current_state_;
  }

  void render()
  {
    if (renderer_ == nullptr) {
      return;
    }

    SDL_Event event{};
    while (SDL_PollEvent(&event) != 0) {
      if (event.type == SDL_QUIT) {
        request_application_shutdown();
      } else if (event.type == SDL_KEYDOWN) {
        const auto key = event.key.keysym.sym;
        const auto modifiers = event.key.keysym.mod;
        if (key == SDLK_ESCAPE || (key == SDLK_c && ((modifiers & KMOD_CTRL) != 0))) {
          request_application_shutdown();
        }
      }
    }

    draw_panel_background();
    draw_status_bar();
    draw_center_indicator();
    SDL_RenderPresent(renderer_);
  }

  void draw_panel_background()
  {
    set_render_color(renderer_, Color{15, 15, 15, 255});
    SDL_RenderClear(renderer_);

    for (const auto& field : rpi_ros2_hmi_panel::make_touch_fields(layout_config_)) {
      set_render_color(renderer_, color_for_state(field.state));
      const SDL_Rect rect{field.rect.x_min, field.rect.y_min, field.rect.x_max - field.rect.x_min,
                          field.rect.y_max - field.rect.y_min};
      SDL_RenderFillRect(renderer_, &rect);
    }
  }

  void draw_status_bar()
  {
    set_render_color(renderer_, Color{0, 0, 0, 255});
    const SDL_Rect status{0, 0, layout_config_.screen_width_px,
                          layout_config_.status_bar_height_px};
    SDL_RenderFillRect(renderer_, &status);

    const auto text_scale = std::max(2, layout_config_.status_bar_height_px / 14);
    draw_text(renderer_, device_name_, 12, 10, text_scale, Color{235, 235, 235, 255},
              get_logger());

    const auto time_text = current_time_hhmmss();
    const auto time_width = static_cast<int>(time_text.size()) * 4 * text_scale;
    draw_text(renderer_, time_text, layout_config_.screen_width_px - time_width - 12, 10,
              text_scale, Color{235, 235, 235, 255}, get_logger());
  }

  void draw_center_indicator()
  {
    const auto center_x = layout_config_.screen_width_px / 2;
    const auto center_y = layout_config_.status_bar_height_px +
                          ((layout_config_.screen_height_px -
                            layout_config_.status_bar_height_px) /
                           2);

    set_render_color(renderer_, Color{15, 15, 15, 255});
    fill_circle(renderer_, center_x, center_y, layout_config_.center_indicator_radius_px + 6);
    set_render_color(renderer_, color_for_state(current_state()));
    fill_circle(renderer_, center_x, center_y, layout_config_.center_indicator_radius_px);
  }

  std::string input_device_path_;
  int debounce_ms_{250};
  LayoutConfig layout_config_;
  std::string device_name_{"RPI-HMI-PANEL"};
  bool fullscreen_{true};

  SDL_Window* window_{nullptr};
  SDL_Renderer* renderer_{nullptr};

  std::unique_ptr<rpi_ros2_hmi_panel::TouchDebouncer> debouncer_;
  std::unique_ptr<EvdevTouchReader> touch_reader_;

  mutable std::mutex state_mutex_;
  PanelState current_state_{PanelState::Unknown};
  rclcpp::Subscription<PanelStateMsg>::SharedPtr state_subscription_;
  rclcpp::Client<SetPanelState>::SharedPtr set_state_client_;
  rclcpp::Publisher<ShutdownMsg>::SharedPtr shutdown_publisher_;
  rclcpp::Subscription<ShutdownMsg>::SharedPtr shutdown_subscription_;
  rclcpp::TimerBase::SharedPtr render_timer_;
};

}  // namespace

int main(int argc, char* argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<TouchscreenHmiNode>());
  rclcpp::shutdown();
  return 0;
}
