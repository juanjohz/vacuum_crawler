# Vacuum controller

Receives `geometry_msgs/msg/Twist` on `cmd_vel` at up to 20 Hz output.
`linear.x` is forward speed (m/s); `angular.z` is turning speed (rad/s).
Positive turning turns left. Other Twist fields are ignored.

## Build and run

```bash
cd /home/vacuum/ros2_ws
source /opt/ros/jazzy/setup.bash
colcon build --packages-select vacuum_controller
source install/setup.bash
ros2 run vacuum_controller control_node --ros-args --params-file src/vacuum_crawler/vacuum_controller/config/controller.yaml
```

The node opens both USB controllers on startup and sends motor commands directly.
To run from a new terminal after building:

```bash
source /opt/ros/jazzy/setup.bash
source /home/vacuum/ros2_ws/install/setup.bash
ros2 run vacuum_controller control_node --ros-args --params-file "$(ros2 pkg prefix vacuum_controller)/share/vacuum_controller/config/controller.yaml"
```

The installed config is under `share/vacuum_controller/config`.

## Wiring and settings

`config/controller.yaml` contains the settings. Defaults follow the verified
mapping in the current Python test scripts: front uses ttyACM1/address 128;
back uses ttyACM0/address 129. The older testing README has outdated addresses. Each controller has M1 right and M2 left.
All motors use negative duty for forward. USB names can change on reconnect.

Measure `wheel_separation_m` and calibrate `wheel_speed_at_full_duty_mps` before
relying on speed commands. This is open-loop duty control, with no encoder feedback
or odometry. The default duty limit is 25%; both sides scale together to preserve
turning proportions. Topic remapping and ROS namespaces are supported.

After 0.5 seconds without a command the node sends zero duty. Invalid commands or
serial failures stop both controllers and latch a fault until restart. Normal
shutdown attempts a stop. Process death or USB loss cannot guarantee a stop;
configure the RoboClaw hardware timeout separately.

## Code map

- `control_node.cpp`: ROS parameters, subscription, timer, and logs.
- `vacuum_controller.cpp`: velocity mixing, limits, command timeout, and faults.
- `vacuum_driver.cpp`: USB connection, packets, CRC, and acknowledgements.

Run regression checks with `colcon test --packages-select vacuum_controller`.
