#include "vacuum_controller/vacuum_driver.hpp"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <stdexcept>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <utility>

namespace vacuum_controller {

VacuumDriver::VacuumDriver(std::string port, int address, int timeout_ms)
    : port_(std::move(port)),
      address_(static_cast<std::uint8_t>(address)),
      timeout_ms_(timeout_ms) {
  if (port_.empty() || address < 0x80 || address > 0x87 || timeout_ms < 20) {
    throw std::invalid_argument("Invalid port, address, or timeout");
  }
}

VacuumDriver::~VacuumDriver() {
  close();
}

void VacuumDriver::connect() {
  if (is_open()) {
    return;
  }

  fd_ = ::open(
      port_.c_str(),
      O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);

  if (fd_ < 0) {
    throw std::runtime_error(
        port_ + ": open failed: " + std::strerror(errno));
  }

  try {
    if (::flock(fd_, LOCK_EX | LOCK_NB) < 0) {
      throw std::runtime_error(port_ + ": port already locked");
    }

    termios settings{};

    if (::tcgetattr(fd_, &settings) < 0) {
      throw std::runtime_error(port_ + ": tcgetattr failed");
    }

    ::cfmakeraw(&settings);
    ::cfsetispeed(&settings, B38400);
    ::cfsetospeed(&settings, B38400);

    settings.c_cflag |= CLOCAL | CREAD;
    settings.c_cflag &= ~CRTSCTS;
    settings.c_cc[VMIN] = 0;
    settings.c_cc[VTIME] = 0;

    if (::tcsetattr(fd_, TCSANOW, &settings) < 0) {
      throw std::runtime_error(port_ + ": tcsetattr failed");
    }

    int lines = TIOCM_DTR | TIOCM_RTS;

    if (::ioctl(fd_, TIOCMBIS, &lines) < 0 &&
        errno != ENOTTY && errno != EINVAL) {
      throw std::runtime_error(port_ + ": USB control-line error");
    }

    ::tcflush(fd_, TCIOFLUSH);
  } catch (...) {
    close();
    throw;
  }
}

void VacuumDriver::close() noexcept {
  if (fd_ >= 0) {
    ::close(fd_);
    fd_ = -1;
  }
}

void VacuumDriver::wait_ready(short events, Deadline deadline) {
  if (!is_open()) {
    throw std::runtime_error(port_ + ": port is closed");
  }

  while (true) {
    const auto remaining =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now()).count();

    if (remaining <= 0) {
      throw std::runtime_error(port_ + ": response timeout");
    }

    pollfd item{fd_, events, 0};
    const int result = ::poll(&item, 1, static_cast<int>(remaining));

    if (result < 0 && errno == EINTR) {
      continue;
    }
    if (result < 0) {
      throw std::runtime_error(port_ + ": poll failed");
    }
    if (result == 0) {
      throw std::runtime_error(port_ + ": response timeout");
    }
    if (item.revents & (POLLERR | POLLHUP | POLLNVAL)) {
      throw std::runtime_error(port_ + ": USB disconnected or serial error");
    }
    if (item.revents & events) {
      return;
    }
  }
}

void VacuumDriver::write_bytes(
    const std::vector<std::uint8_t> &data, Deadline deadline) {
  std::size_t sent = 0;

  while (sent < data.size()) {
    wait_ready(POLLOUT, deadline);

    const auto count =
        ::write(fd_, data.data() + sent, data.size() - sent);

    if (count < 0 && (errno == EAGAIN || errno == EINTR)) {
      continue;
    }
    if (count <= 0) {
      throw std::runtime_error(port_ + ": write failed");
    }

    sent += static_cast<std::size_t>(count);
  }
}

std::uint8_t VacuumDriver::read_byte(Deadline deadline) {
  while (true) {
    wait_ready(POLLIN, deadline);

    std::uint8_t value{};
    const auto count = ::read(fd_, &value, 1);

    if (count == 1) {
      return value;
    }
    if (count < 0 && errno != EAGAIN && errno != EINTR) {
      throw std::runtime_error(port_ + ": read failed");
    }
  }
}

std::uint16_t VacuumDriver::crc16(
    const std::vector<std::uint8_t> &data) {
  std::uint16_t crc = 0;

  for (const auto byte : data) {
    crc ^= static_cast<std::uint16_t>(byte) << 8;

    for (int bit = 0; bit < 8; ++bit) {
      crc = static_cast<std::uint16_t>(
          (crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1);
    }
  }

  return crc;
}

std::string VacuumDriver::read_version() {
  if (!is_open()) {
    throw std::runtime_error(port_ + ": port is closed");
  }

  const auto deadline = std::chrono::steady_clock::now() +
      std::chrono::milliseconds(timeout_ms_);

  std::vector<std::uint8_t> packet{address_, 21};

  ::tcflush(fd_, TCIFLUSH);
  write_bytes(packet, deadline);

  std::string version;

  for (int index = 0; index < 128; ++index) {
    const auto value = read_byte(deadline);
    packet.push_back(value);

    if (value == 0) {
      const auto high = read_byte(deadline);
      const auto low = read_byte(deadline);
      const auto received =
          static_cast<std::uint16_t>((high << 8) | low);

      if (received != crc16(packet)) {
        throw std::runtime_error(port_ + ": firmware CRC mismatch");
      }

      return version;
    }

    version.push_back(static_cast<char>(value));
  }

  throw std::runtime_error(port_ + ": unterminated firmware reply");
}

void VacuumDriver::set_duty(int m1, int m2) {
  if (m1 < -32767 || m1 > 32767 || m2 < -32767 || m2 > 32767) {
    throw std::invalid_argument("Duty outside -32767 to 32767");
  }
  if (!is_open()) {
    throw std::runtime_error(port_ + ": port is closed");
  }

  std::vector<std::uint8_t> packet{address_, 34};

  for (const int duty : {m1, m2}) {
    const auto value =
        static_cast<std::uint16_t>(static_cast<std::int16_t>(duty));

    packet.push_back(static_cast<std::uint8_t>(value >> 8));
    packet.push_back(static_cast<std::uint8_t>(value & 0xff));
  }

  const auto crc = crc16(packet);
  packet.push_back(static_cast<std::uint8_t>(crc >> 8));
  packet.push_back(static_cast<std::uint8_t>(crc & 0xff));

  const auto deadline = std::chrono::steady_clock::now() +
      std::chrono::milliseconds(timeout_ms_);

  ::tcflush(fd_, TCIFLUSH);
  write_bytes(packet, deadline);

  if (read_byte(deadline) != 0xff) {
    throw std::runtime_error(port_ + ": invalid acknowledgement");
  }
}

void VacuumDriver::stop() {
  set_duty(0, 0);
}

}  // namespace vacuum_controller
