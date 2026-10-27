#pragma once

#include "vacuum_controller/vacuum_driver.hpp"
#include <array>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace vacuum_controller {

struct DriveConfig {
  std::vector<std::string> ports{"/dev/ttyACM1", "/dev/ttyACM0"};
  std::vector<std::int64_t> addresses{128, 129};
  // Controller 0 is front; controller 1 is back. Negative duty drives forward.
  // Order: controller 0 M1, controller 0 M2, controller 1 M1, controller 1 M2.
  std::vector<std::string> motor_sides{"right", "left", "right", "left"};
  std::vector<std::int64_t> motor_signs{-1, -1, -1, -1};
  double wheel_separation_m{0.30};             // Example: measure your crawler.
  double wheel_speed_at_full_duty_mps{0.50};   // Example: needs calibration.
  double max_duty_fraction{0.25};
  double cmd_timeout_s{0.50};
  std::int64_t serial_timeout_ms{100};
};

// No ROS dependencies. Owns motion calculations, command age, and fault state.
class VacuumController {
public:
  using Clock = std::chrono::steady_clock;
  explicit VacuumController(DriveConfig config);
  ~VacuumController();
  void connect();
  void set_command(double linear_mps, double angular_rps,
                   Clock::time_point now = Clock::now());
  void update(Clock::time_point now = Clock::now());
  bool stop() noexcept;

  std::array<int, 4> calculate_duties(double linear_mps, double angular_rps) const;
  const std::array<int, 4> &last_duties() const noexcept { return last_duties_; }
  bool faulted() const noexcept { return !fault_reason_.empty(); }
  const std::string &fault_reason() const noexcept { return fault_reason_; }
  const std::array<std::string, 2> &versions() const noexcept { return versions_; }

private:
  void latch_fault(const std::string &reason);
  DriveConfig config_;
  std::array<std::unique_ptr<VacuumDriver>, 2> drivers_;
  std::array<std::string, 2> versions_;
  std::array<int, 4> last_duties_{};
  Clock::time_point last_command_{};
  double linear_{0};
  double angular_{0};
  bool have_command_{false};
  bool connected_{false};
  std::string fault_reason_;
};

}  // namespace vacuum_controller
