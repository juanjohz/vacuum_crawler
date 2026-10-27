# RoboClaw Testing Guide

Setup notes and test instructions for the Raspberry Pi vacuum crawler.

## 1. Project layout

The Python environment lives outside the ROS 2 workspace. Test scripts and dependency versions stay in Git.

```text
/home/vacuum/
├── roboclaw_env/
└── ros2_ws/
    └── src/
        └── vacuum_crawler/
            ├── .gitignore
            ├── vacuum_cameras/
            ├── vacuum_controller/
            ├── vacuum_teleop/
            └── vacuum_testing/
                ├── check_connection.py
                ├── test_one_motor.py
                ├── test_four_motors.py
                ├── requirements-roboclaw.txt
                └── testing_README.md
```

Do not commit `roboclaw_env`. Recreate it from the dependency file when needed.

## 2. Python environment setup

On the Raspberry Pi:

```bash
sudo apt install python3-venv
/usr/bin/python3 -m venv /home/vacuum/roboclaw_env
```

The verified dependency versions are:

```text
basicmicro==2.0.10
pyserial==3.5
```

These are saved in `vacuum_testing/requirements-roboclaw.txt`.

Install them with:

```bash
/home/vacuum/roboclaw_env/bin/python -m pip install \
  -r /home/vacuum/ros2_ws/src/vacuum_crawler/vacuum_testing/requirements-roboclaw.txt
```

Packages installed here are available to the environment’s Python. They are not installed globally.

## 3. Verify the installation

```bash
/home/vacuum/roboclaw_env/bin/python - <<'PY'
import sys
import serial
import basicmicro
from basicmicro import Basicmicro
from importlib.metadata import version

print("Python:", sys.executable)
print("Basicmicro version:", version("basicmicro"))
print("PySerial version:", version("pyserial"))
print("Basicmicro location:", basicmicro.__file__)
print("PySerial location:", serial.__file__)
print("Imports OK")
PY
```

Verified results:

- Python environment: `/home/vacuum/roboclaw_env`
- Python version used: 3.12
- Basicmicro: 2.0.10
- PySerial: 3.5

This check does not communicate with the controllers or move motors.

## 4. Git ignore rules

The following rules were added to `.gitignore`:

```gitignore
# Python virtual environments and cache
.venv/
venv/
roboclaw_env/
__pycache__/
```

The actual environment is outside the repository, so Git does not track it anyway.

## 5. USB connections and permissions

Both controllers are connected by USB and identify as Basicmicro RoboClaw 2x7A devices.

List the serial ports:

```bash
ls -l /dev/ttyACM*
```

Check user groups:

```bash
id -nG
```

The `vacuum` user already belongs to `dialout`, which provides access to the serial devices.

Both controllers reported the same USB serial identifier. As a result, `/dev/serial/by-id/` showed only one distinguishable link. Do not rely on that link to identify both controllers.

USB port numbers can change after rebooting or reconnecting. The mapping below was verified during testing, but should be checked again if communication fails.

## 6. Verified controller and wheel mapping

| Position | USB port during testing | Packet address | Firmware |
|---|---|---|---|
| Front | `/dev/ttyACM0` | `0x80` | USB Roboclaw 2x7A v4.4.9 |
| Back | `/dev/ttyACM1` | `0x81` | USB Roboclaw 2x7a v4.2.8 |

The port/address and firmware pairs above were checked on October 27, 2026.
Front/back roles follow the controller configuration.
For stable paths with the current physical USB connections, use:

- Front (`0x80`): `/dev/serial/by-path/platform-xhci-hcd.1-usb-0:2:1.0`
- Back (`0x81`): `/dev/serial/by-path/platform-xhci-hcd.0-usb-0:2:1.0`

Keep each cable in the same USB port. The ROS controller YAML already uses these
paths, but the Python scripts do not read that YAML. `check_connection.py`
currently uses the numbered ports in the table. `test_four_motors.py` still has
front `/dev/ttyACM1` and back `/dev/ttyACM0`; `test_one_motor.py` still pairs
`/dev/ttyACM0` with `0x81`. Update their port constants to the matching persistent
paths before running motor tests.

Both controllers use the following motor mapping:

| Output | Wheel |
|---|---|
| M1 | Right |
| M2 | Left |

For all four wheels, **negative duty was verified as forward**.

Test settings:

```python
BAUD = 38400
DUTY = -15000
DURATION = 2
```

The duty range is approximately `-32767` through `32767`. A value of `-15000` is about 46% negative duty; this does not guarantee a particular wheel speed.

## 7. Run tests from the terminal

Activate the environment:

```bash
source /home/vacuum/roboclaw_env/bin/activate
```

Enter the testing directory:

```bash
cd /home/vacuum/ros2_ws/src/vacuum_crawler/vacuum_testing
```

Stop the ROS control node before running these scripts so only one process
uses the serial connections. `check_connection.py` only reads firmware; the
other two scripts move motors. Check their port constants as described above.

Run one test at a time:

```bash
python check_connection.py
```

```bash
python test_one_motor.py
```

```bash
python test_four_motors.py
```

Leave the environment when finished:

```bash
deactivate
```

Activation applies to the current terminal session. Activate again in a new terminal.

Alternatively, run the connection check using the environment's full Python path:

```bash
/home/vacuum/roboclaw_env/bin/python /home/vacuum/ros2_ws/src/vacuum_crawler/vacuum_testing/check_connection.py
```
