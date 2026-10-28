import importlib.util
import math
import os
from pathlib import Path
import pty
import time

import pytest
import rclpy
from geometry_msgs.msg import Twist

spec = importlib.util.spec_from_file_location(
    'bridge', Path(__file__).parents[1] / 'scripts' / 'teensy_bridge.py')
bridge = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bridge)


def test_mapping():
    for value, expected in [(-20, -20), (-10, -20), (0, 0), (0.4, 0), (5, 10), (10, 20), (20, 20)]:
        assert bridge.speed_for(value, 10, 20, 0.05) == expected
    for value in [math.nan, math.inf, -math.inf]:
        with pytest.raises(ValueError):
            bridge.speed_for(value, 10, 20, 0.05)


def test_serial_identity_commands_and_watchdog():
    master, slave = pty.openpty()
    os.set_blocking(master, False)
    rclpy.init(args=['--ros-args', '-p', f'port:={os.ttyname(slave)}'])
    node = bridge.TeensyBridge()

    def read():
        time.sleep(0.01)
        try:
            return os.read(master, 4096)
        except BlockingIOError:
            return b''

    try:
        msg = Twist()
        msg.angular.x = 10.0
        node.command(msg)
        node.tick()
        assert read() == b'HELLO\n'
        os.write(master, b'WRONG_DEVICE\n')
        time.sleep(0.01)
        node.tick()
        assert not node.ready
        assert b'SPEED' not in read()
        # Identification may arrive split across reads.
        os.write(master, b'VACUUM_SERVO_')
        time.sleep(0.01)
        node.tick()
        assert not node.ready
        os.write(master, b'UART_V2\n')
        time.sleep(0.01)
        node.tick()
        assert node.ready
        assert read() == b'STOP\n'
        node.command(msg)
        node.tick()
        assert read() == b'SPEED 20.000\n'
        node.last_command -= 1.0
        node.tick()
        assert read() == b'STOP\n'
        msg.angular.x = math.nan
        node.command(msg)
        node.tick()
        assert read() == b'STOP\n'
        node.disconnect()
        assert not node.ready and node.last_command == -math.inf
        node.next_connect = 0
        node.tick()
        assert read() == b'HELLO\n'
        node.opened -= 4.0
        node.tick()
        assert node.link is None
    finally:
        node.destroy_node()
        rclpy.shutdown()
        os.close(master)
        os.close(slave)


def test_refuses_kernel_console(monkeypatch):
    rclpy.init(args=['--ros-args', '-p', 'port:=/dev/ttyAMA0'])
    node = bridge.TeensyBridge()
    monkeypatch.setattr(Path, 'read_text', lambda self: 'ttyAMA0 -W- (E p a) 204:64\n')
    def forbidden_open(*args, **kwargs):
        pytest.fail('Must not open a kernel console')
    monkeypatch.setattr(bridge.serial, 'Serial', forbidden_open)
    try:
        node.tick()
        assert node.link is None
    finally:
        node.destroy_node()
        rclpy.shutdown()
