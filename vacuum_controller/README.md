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

`config/controller.yaml` uses persistent USB paths, in front/back order:

| Position | Device path | Packet address |
| --- | --- | --- |
| Front | `/dev/serial/by-path/platform-xhci-hcd.1-usb-0:2:1.0` | 128 (`0x80`) |
| Back | `/dev/serial/by-path/platform-xhci-hcd.0-usb-0:2:1.0` | 129 (`0x81`) |

Keep each controller in the same physical USB port. These paths remain useful
when `ttyACM` numbering changes; moving cables to different ports changes the
mapping. Both controllers report the same USB identifier, so `by-id` cannot
uniquely distinguish them.

The compiled defaults use these same persistent paths, so the plain command
`ros2 run vacuum_controller control_node` also works after rebuilding and sourcing
the workspace. USB enumeration order no longer swaps the configured addresses.
Keep the cables in their assigned physical ports. The YAML can override defaults
for a different wiring arrangement or tuning. Rebuild after changing source YAML
when using the installed copy.

Each controller has M1 right and M2 left. All motors use negative duty for forward.
The standalone Python scripts have their own port settings; they do not load
this YAML. See the [testing guide](../vacuum_testing/testing_README.md).

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
