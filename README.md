# vacuum_crawler

Run the vacuum controller from a terminal after building the workspace:

```bash
source /opt/ros/jazzy/setup.bash
source /home/vacuum/ros2_ws/install/setup.bash
ros2 run vacuum_controller control_node --ros-args --params-file "$(ros2 pkg prefix vacuum_controller)/share/vacuum_controller/config/controller.yaml"
```

The node connects to both USB motor controllers on startup. See the
[vacuum controller README](vacuum_controller/README.md) for build instructions and settings.
