#include "vacuum_controller/vacuum_controller.hpp"
#include <geometry_msgs/msg/twist.hpp>
#include <rclcpp/rclcpp.hpp>
#include <chrono>
#include <memory>
#include <utility>

class ControlNode : public rclcpp::Node {
public:
  ControlNode() : Node("vacuum_controller") {
    vacuum_controller::DriveConfig config;
    config.ports = declare_parameter("ports", config.ports);
    config.addresses = declare_parameter("addresses", config.addresses);
    config.motor_sides = declare_parameter("motor_sides", config.motor_sides);
    config.motor_signs = declare_parameter("motor_signs", config.motor_signs);
    config.wheel_separation_m = declare_parameter("wheel_separation_m", config.wheel_separation_m);
    config.wheel_speed_at_full_duty_mps = declare_parameter(
        "wheel_speed_at_full_duty_mps", config.wheel_speed_at_full_duty_mps);
    config.max_duty_fraction = declare_parameter("max_duty_fraction", config.max_duty_fraction);
    config.cmd_timeout_s = declare_parameter("cmd_timeout_s", config.cmd_timeout_s);
    config.serial_timeout_ms = declare_parameter("serial_timeout_ms", config.serial_timeout_ms);
    controller_ = std::make_unique<vacuum_controller::VacuumController>(std::move(config));
    controller_->connect();
    subscription_ = create_subscription<geometry_msgs::msg::Twist>(
        "cmd_vel", rclcpp::QoS(1),
        [this](geometry_msgs::msg::Twist::ConstSharedPtr message) {
          on_command(*message);
        });
    timer_ = create_wall_timer(
        std::chrono::milliseconds(50), [this]() { update(); });
    RCLCPP_INFO(get_logger(), "Listening on %s (Twist). LIVE motor output",
        subscription_->get_topic_name());
  }
  ~ControlNode() override {
    if (controller_ && !controller_->stop()) {
      RCLCPP_ERROR(get_logger(), "Shutdown stop not acknowledged by every open controller");
    }
  }

private:
  void on_command(const geometry_msgs::msg::Twist &message) {
    controller_->set_command(message.linear.x, message.angular.z);
  }

  void update() {
    controller_->update();
    if (controller_->faulted()) {
      RCLCPP_ERROR_THROTTLE(get_logger(), *get_clock(), 2000,
          "Drive fault; restart required: %s", controller_->fault_reason().c_str());
      return;
    }
  }

  std::unique_ptr<vacuum_controller::VacuumController> controller_;
  rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr subscription_;
  rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char **argv) {
  rclcpp::init(argc, argv);
  int status = 0;
  try { rclcpp::spin(std::make_shared<ControlNode>()); }
  catch (const std::exception &error) {
    RCLCPP_FATAL(rclcpp::get_logger("vacuum_controller"), "%s", error.what());
    status = 1;
  }
  rclcpp::shutdown();
  return status;
}
