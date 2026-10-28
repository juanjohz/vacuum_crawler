# vacuum_crawler

Run the vacuum controller from a terminal after building the workspace:

```bash
source /opt/ros/jazzy/setup.bash
source /home/vacuum/ros2_ws/install/setup.bash
ros2 run vacuum_controller control_node --ros-args --params-file "$(ros2 pkg prefix vacuum_controller)/share/vacuum_controller/config/controller.yaml"
```

The node connects to both USB motor controllers on startup. See the
[vacuum controller README](vacuum_controller/README.md) for build instructions and settings.

The controller YAML uses persistent USB paths. Keep each RoboClaw connected to
the same physical USB port and rebuild after changing the source config when
using the installed config shown above.

For the dual cameras, see the [camera README](vacuum_cameras/README.md).
The launch defaults are 320×240 at 10 FPS with JPEG quality 60:

```bash
ros2 launch vacuum_cameras cameras.launch.xml
```

For firmware connection checks and standalone motor tests, see the
[testing guide](vacuum_testing/testing_README.md). The Python scripts use their
own port constants rather than the ROS controller YAML.

For joystick axis 4 control of a Teensy-connected servo, see the
[servo bridge guide](servo_controller/README.md) for firmware, wiring checks, and ROS commands.
