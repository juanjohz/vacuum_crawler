#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace vacuum_controller {

// One instance owns one USB connection. This class has no ROS dependencies.
class VacuumDriver {
public:
  VacuumDriver(std::string port, int address, int timeout_ms = 100);
  ~VacuumDriver();
  VacuumDriver(const VacuumDriver &) = delete;
  VacuumDriver &operator=(const VacuumDriver &) = delete;

  void connect();
  void close() noexcept;
  std::string read_version();
  void set_duty(int m1, int m2);
  void stop();
  bool is_open() const noexcept { return fd_ >= 0; }

private:
  using Deadline = std::chrono::steady_clock::time_point;
  std::string port_;
  std::uint8_t address_;
  int timeout_ms_;
  int fd_{-1};
  void wait_ready(short events, Deadline deadline);
  void write_bytes(const std::vector<std::uint8_t> &data, Deadline deadline);
  std::uint8_t read_byte(Deadline deadline);
  static std::uint16_t crc16(const std::vector<std::uint8_t> &data);
};

}  // namespace vacuum_controller
