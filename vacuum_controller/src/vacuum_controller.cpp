#include "vacuum_controller/vacuum_controller.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace vacuum_controller {

VacuumController::VacuumController(DriveConfig config) : config_(std::move(config)) {
  if (config_.ports.size() != 2 || config_.addresses.size() != 2 ||
      config_.motor_sides.size() != 4 || config_.motor_signs.size() != 4) {
    throw std::invalid_argument("Expected 2 ports, 2 addresses, 4 motor_sides, and 4 motor_signs");
  }
  if (config_.ports[0] == config_.ports[1]) {
    throw std::invalid_argument("Each RoboClaw needs its own USB port");
  }
  const auto positive = [](double x) { return std::isfinite(x) && x > 0; };
  if (!positive(config_.wheel_separation_m) ||
      !positive(config_.wheel_speed_at_full_duty_mps) ||
      !positive(config_.max_duty_fraction) || config_.max_duty_fraction > 1 ||
      !positive(config_.cmd_timeout_s) || config_.serial_timeout_ms < 20 ||
      config_.serial_timeout_ms > 500 ||
      !positive(config_.wheel_speed_at_full_duty_mps * config_.max_duty_fraction)) {
    throw std::invalid_argument("Invalid geometry, speed scale, duty limit, or timeout");
  }
  int left_count = 0;
  for (std::size_t i = 0; i < 4; ++i) {
    if (config_.motor_sides[i] == "left") {
      ++left_count;
    }
    else if (config_.motor_sides[i] != "right") {
      throw std::invalid_argument("motor_sides entries must be left or right");
    }
    if (config_.motor_signs[i] != 1 && config_.motor_signs[i] != -1) {
      throw std::invalid_argument("motor_signs entries must be +1 or -1");
    }
  }
  if (left_count != 2) {
    throw std::invalid_argument("Expected two motors per side");
  }
  for (std::size_t i = 0; i < 2; ++i) {
    if (config_.addresses[i] < 128 || config_.addresses[i] > 135) {
      throw std::invalid_argument("RoboClaw addresses must be between 128 and 135");
    }
    drivers_[i] = std::make_unique<VacuumDriver>(config_.ports[i],
        static_cast<int>(config_.addresses[i]), static_cast<int>(config_.serial_timeout_ms));
  }
}

VacuumController::~VacuumController() { stop(); }

void VacuumController::connect() {
  if (connected_) {
    return;
  }
  if (faulted()) {
    throw std::runtime_error("Restart required after a drive fault");
  }
  try {
    if (!config_.dry_run) {
      for (auto &driver : drivers_) {
        driver->connect();
      }
      for (auto &driver : drivers_) {
        driver->stop();
      }
      for (std::size_t i = 0; i < 2; ++i) {
        versions_[i] = drivers_[i]->read_version();
      }
    }
    // Commands received before connect are discarded; startup always remains stopped.
    have_command_ = false;
    connected_ = true;
  } catch (const std::exception &error) {
    latch_fault(error.what());
    throw std::runtime_error(fault_reason_);
  }
}

std::array<int, 4> VacuumController::calculate_duties(double linear_mps, double angular_rps) const {
  if (!std::isfinite(linear_mps) || !std::isfinite(angular_rps)) {
    throw std::invalid_argument("Nonfinite velocity command");
  }
  const double turn_speed = angular_rps * (config_.wheel_separation_m / 2);
  const double left = linear_mps - turn_speed;
  const double right = linear_mps + turn_speed;
  if (!std::isfinite(left) || !std::isfinite(right)) {
    throw std::invalid_argument("Velocity calculation overflow");
  }
  const double allowed_speed = config_.wheel_speed_at_full_duty_mps * config_.max_duty_fraction;
  const double peak = std::max(std::abs(left), std::abs(right));
  const double scale = peak > allowed_speed ? allowed_speed / peak : 1.0;
  const int limit = static_cast<int>(std::floor(32767 * config_.max_duty_fraction));
  std::array<int, 4> duties{};
  for (std::size_t i = 0; i < 4; ++i) {
    const double speed = config_.motor_sides[i] == "left" ? left : right;
    const int duty = static_cast<int>(std::lround(
        (speed * scale / config_.wheel_speed_at_full_duty_mps) * 32767));
    duties[i] = static_cast<int>(config_.motor_signs[i]) * std::clamp(duty, -limit, limit);
  }
  return duties;
}

void VacuumController::set_command(double linear_mps, double angular_rps, Clock::time_point now) {
  if (faulted()) {
    return;
  }
  if (!std::isfinite(linear_mps) || !std::isfinite(angular_rps)) {
    latch_fault("Nonfinite cmd_vel");
    return;
  }
  linear_ = linear_mps;
  angular_ = angular_rps;
  last_command_ = now;
  have_command_ = true;
}

void VacuumController::update(Clock::time_point now) {
  if (faulted()) {
    return;
  }
  try {
    if (!connected_) {
      throw std::runtime_error("Controller has not been connected");
    }
    const auto age = std::chrono::duration<double>(now - last_command_).count();
    const bool stale = !have_command_ || age < 0 || age >= config_.cmd_timeout_s;
    const auto duties = calculate_duties(stale ? 0 : linear_, stale ? 0 : angular_);
    if (!config_.dry_run) {
      drivers_[0]->set_duty(duties[0], duties[1]);
      drivers_[1]->set_duty(duties[2], duties[3]);
    }
    last_duties_ = duties;
  } catch (const std::exception &error) {
    latch_fault(error.what());
  }
}

bool VacuumController::stop() noexcept {
  // A public stop also clears the previous demand, so update cannot resume it.
  have_command_ = false;
  linear_ = angular_ = 0;
  last_duties_.fill(0);
  bool acknowledged = true;
  if (!config_.dry_run) {
    for (auto &driver : drivers_) {
      if (!driver || !driver->is_open()) {
        continue;
      }
      try { driver->stop(); }
      catch (...) { acknowledged = false; }
    }
  }
  return acknowledged;
}

void VacuumController::latch_fault(const std::string &reason) {
  fault_reason_ = reason;
  if (!stop()) {
    fault_reason_ += "; stop was not acknowledged by every open controller";
  }
}

}  // namespace vacuum_controller
