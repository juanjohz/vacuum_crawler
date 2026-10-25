
#include <memory>
#include <functional>
#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/joy.hpp"
#include "geometry_msgs/msg/twist.hpp"

using std::placeholders::_1;

class JoyVacuumController : public rclcpp::Node
{
public:
    JoyVacuumController() : Node("vacuum_teleop")
    {
        subscription_ = this->create_subscription<sensor_msgs::msg::Joy>(
            "/joy",
            rclcpp::SensorDataQoS(),
            std::bind(&JoyVacuumController::joy_callback, this, _1)
        );

        publisher_ = this->create_publisher<geometry_msgs::msg::Twist>(
            "/cmd_vel",
            10
        );

        RCLCPP_INFO(this->get_logger(),
                    "Joystick Vacuum Controller Started");
    }

private:
    void joy_callback(const sensor_msgs::msg::Joy::SharedPtr msg)
    {
        // Make sure all required axes exist
        if (msg->axes.size() < 5) {
            RCLCPP_WARN(this->get_logger(), "Missing joystick axes");
            return;
        }

        // Joystick inputs
        float left_x  = msg->axes[0];
        float left_y  = msg->axes[1];
        float right_y = msg->axes[4];

        RCLCPP_INFO(this->get_logger(),
                    "axes[0]=%.2f, axes[1]=%.2f, axes[4]=%.2f",
                    left_x, left_y, right_y);

        // Create velocity command
        geometry_msgs::msg::Twist twist;

        twist.linear.x  = left_y * 2.0;
        twist.angular.z = left_x * 2.0;
        twist.angular.x = right_y * 10.0;

        publisher_->publish(twist);
    }

    rclcpp::Subscription<sensor_msgs::msg::Joy>::SharedPtr subscription_;
    rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr publisher_;
};

int main(int argc, char ** argv)
{
    rclcpp::init(argc, argv);

    auto node = std::make_shared<JoyVacuumController>();

    rclcpp::spin(node);

    rclcpp::shutdown();
    return 0;
}
