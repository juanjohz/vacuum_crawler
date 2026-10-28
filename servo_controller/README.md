# Teensy UART servo bridge

`/joy.axes[4]` → existing `vacuum_teleop_node` → `/cmd_vel.angular.x`
(joystick × 10) → `teensy_bridge` → Pi UART → Teensy `Serial1` → PWMServo pin 4.

The recovered sketch specifies pin 4, a positional camera servo limited to
60–150°, initial angle 149°, and Serial1 at 115200 baud. USB Serial was only for
debugging. Those details describe the intended wiring; the PCB routing is not yet
physically verified. The two USB ttyACM devices on this Pi are RoboClaws, not Teensy.

## Connection blocker on this Pi

`/dev/ttyAMA0` exists but is currently an active kernel console and serial login
port. `/boot/firmware/cmdline.txt` includes `console=serial0,115200`, and
`serial-getty@ttyAMA0.service` is active. The bridge refuses to open an active
kernel console. These system settings have not been changed: noninteractive sudo requires a password.
Run the prepared script locally (it backs up the boot settings and does not reboot):

```bash
sudo bash /home/vacuum/ros2_ws/src/vacuum_crawler/servo_controller/tools/configure_uart.sh
sudo reboot
```

The script also masks the serial login service to prevent automatic reactivation.
To reverse this later, restore the backed-up command line, run
`sudo systemctl unmask serial-getty@ttyAMA0.service`, then enable the service and reboot.

To dedicate that UART to Teensy, use a working SSH or local graphical session:

1. Back up `/boot/firmware/cmdline.txt`, then remove only
   `console=serial0,115200` from it. Keep the file on one line and keep `console=tty1`.
2. Run `sudo systemctl disable --now serial-getty@ttyAMA0.service`.
3. Reboot when ready. Check `cat /proc/consoles` no longer lists `ttyAMA0` and
   `systemctl is-active serial-getty@ttyAMA0.service` returns `inactive`.

`enable_uart=1` is already present in `/boot/firmware/config.txt`.
The existence of a UART device does not establish that a Teensy is connected.
Confirm the PCB traces: Pi TX should reach Teensy RX1 (pin 0), and Teensy TX1
(pin 1) should reach Pi RX, with common ground and 3.3 V logic. Both directions
are needed by the new firmware handshake. If the PCB uses the usual Pi header
UART, these are GPIO14/TX (physical pin 8) and GPIO15/RX (physical pin 10).
Do not assume `/dev/ttyAMA10` reaches the PCB.

References: [PJRC UART documentation](https://www.pjrc.com/teensy/td_uart.html),
[Teensy pinout](https://www.pjrc.com/teensy/card11a_rev4_web.pdf),
[PWMServo documentation](https://www.pjrc.com/teensy/td_libs_Servo.html).

## Firmware

Open `firmware/teensy_servo/teensy_servo.ino` in Arduino with Teensy support.
Select **Teensy 4.1**, **USB Type: Serial**, and upload through the Teensy's USB
device connector with Teensy Loader. UART control does not provide firmware
upload. Confirm PCB power routing before adding USB power.

The new firmware must replace the old sketch: it uses a different protocol.
`HELLO` returns `VACUUM_SERVO_UART_V2` over Serial1; `SPEED <degrees/second>`
commands movement; `STOP` holds position. Unlike the old `parseInt()` loop,
parsing does not block, movement is based on elapsed time, and angles clamp at
60 and 150 rather than bouncing back. A 500 ms firmware watchdog stops movement
if commands cease. Holding the last angle keeps PWM enabled.

Startup leaves PWM disabled until the first valid SPEED command, which applies
the original 149° startup position. This can move the camera even for SPEED 0;
check clearance before the first command. Full stick defaults to 20°/second,
center holds the current angle, and a 5% deadband suppresses small stick noise.

## Build and run

```bash
cd /home/vacuum/ros2_ws
source /opt/ros/jazzy/setup.bash
colcon build --packages-select servo_controller vacuum_teleop
source install/setup.bash
ros2 run servo_controller teensy_bridge --ros-args --params-file \
  "$(ros2 pkg prefix servo_controller)/share/servo_controller/config/servo.yaml"
```

Default port is `/dev/ttyAMA0`; override with `-p port:=<confirmed-port>` if needed.
The bridge requires firmware identification before sending movement commands,
retries connections every two seconds, and sends STOP after 300 ms without a
fresh command. Reconnection requires a fresh command after identification.

In separate sourced terminals:

```bash
ros2 run joy joy_node --ros-args -p autorepeat_rate:=20.0
ros2 run vacuum_teleop vacuum_teleop_node
```

There is no enable button in the existing teleop. It also publishes crawler drive
commands, so isolate drive power for initial testing. To test only the servo,
stop teleop and use a separate topic:

```bash
ros2 run servo_controller teensy_bridge --ros-args -r /cmd_vel:=/servo_test_cmd
ros2 topic pub --rate 20 /servo_test_cmd geometry_msgs/msg/Twist '{angular: {x: -1.0}}'
```

This requests -2°/second. Ctrl-C the publisher to stop movement and hold position.

## Validation

```bash
colcon test --packages-select servo_controller
colcon test-result --verbose
```

Pseudo-terminal tests cover firmware identity, fragmented replies, speed mapping,
deadband, stale/invalid commands, and reconnects. The ROS packages were built in
`/tmp/vacuum-servo-*` during development. No firmware upload or physical servo test
has been performed; a Teensy toolchain is not installed on this Pi.
