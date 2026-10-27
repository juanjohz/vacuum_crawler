#include "vacuum_controller/vacuum_controller.hpp"
#include <limits>
#include <stdexcept>

using vacuum_controller::VacuumController;

void check(bool condition) {
  if (!condition) { throw std::runtime_error("Controller regression check failed"); }
}

int main() {
  VacuumController controller({});  // Calculation checks do not open USB.
  const auto forward = controller.calculate_duties(0.1, 0);
  check(forward == (std::array<int, 4>{-6553, -6553, -6553, -6553}));
  const auto turn = controller.calculate_duties(0, 1);
  check(turn[0] < 0 && turn[1] > 0 && turn[2] < 0 && turn[3] > 0);
  const auto fast = controller.calculate_duties(100, 0);
  check(fast == (std::array<int, 4>{-8191, -8191, -8191, -8191}));
  const auto now = VacuumController::Clock::now();
  check(controller.stop());
  controller.set_command(std::numeric_limits<double>::quiet_NaN(), 0, now);
  check(controller.faulted());
  check(controller.fault_reason() == "Nonfinite cmd_vel");
  controller.set_command(0.1, 0, now);
  controller.update(now);
  check(controller.last_duties() == (std::array<int, 4>{}));

  VacuumController disconnected({});
  disconnected.update(now);
  check(disconnected.faulted());
  check(disconnected.fault_reason() == "Controller has not been connected");
}
