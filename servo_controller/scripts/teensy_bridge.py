#!/usr/bin/env python3
"""Bridge the existing teleop's angular.x command to identified Teensy firmware."""
import math
import time

import rclpy
from rclpy.node import Node
from geometry_msgs.msg import Twist
import serial


def speed_for(value, scale, max_speed, deadband):
    if not math.isfinite(value):
        raise ValueError('Command must be finite')
    axis = max(-1.0, min(1.0, value / scale))
    return 0.0 if abs(axis) <= deadband else axis * max_speed


class TeensyBridge(Node):
    def __init__(self):
        super().__init__('teensy_bridge')
        defaults = {'port': '/dev/ttyAMA0', 'baud': 115200, 'command_scale': 10.0,
                    'max_speed_deg_s': 20.0, 'deadband': 0.05,
                    'command_timeout': 0.3}
        for name, value in defaults.items():
            self.declare_parameter(name, value)
        p = lambda name: self.get_parameter(name).value
        self.port = p('port')
        self.baud = p('baud')
        self.scale = p('command_scale')
        self.max_speed, self.deadband = p('max_speed_deg_s'), p('deadband')
        self.timeout = p('command_timeout')
        if not (math.isfinite(self.scale) and self.scale > 0 and
                math.isfinite(self.timeout) and 0 < self.timeout <= 0.5 and
                math.isfinite(self.max_speed) and 0 < self.max_speed <= 90 and
                0 <= self.deadband < 1 and self.baud > 0 and self.port):
            raise ValueError('Invalid scale, timeout, baud, port, speed, or deadband')
        self.link = None
        self.ready = False
        self.buffer = b''
        self.last_command = -math.inf
        self.target = 0.0
        self.next_connect = 0.0
        self.last_probe = 0.0
        self.opened = 0.0
        self.create_subscription(Twist, '/cmd_vel', self.command, 1)
        self.create_timer(0.05, self.tick)

    def command(self, msg):
        # Commands received before identification must not move the servo later.
        if not self.ready:
            return
        try:
            self.target = speed_for(msg.angular.x, self.scale, self.max_speed, self.deadband)
            self.last_command = time.monotonic()
        except ValueError:
            self.last_command = -math.inf

    def disconnect(self):
        if self.link:
            self.link.close()
        self.link = None
        self.ready = False
        self.last_command = -math.inf
        self.next_connect = time.monotonic() + 2.0

    def send(self, message):
        data = message.encode('ascii')
        if self.link.write(data) != len(data):
            raise serial.SerialException('Incomplete serial write')

    def tick(self):
        now = time.monotonic()
        try:
            if self.link is None:
                if now < self.next_connect:
                    return
                self.next_connect = now + 2.0
                port = self.port
                # Never share a UART with the kernel console.
                from pathlib import Path
                device = Path(port).resolve().name
                consoles = Path('/proc/consoles').read_text().splitlines()
                if any(line.split()[0] == device for line in consoles if line.split()):
                    self.get_logger().error(f'{port} is a kernel console; free it and reboot first')
                    return
                self.link = serial.Serial(port, self.baud, timeout=0, write_timeout=0.1, exclusive=True)
                self.buffer = b''
                self.opened = now
                self.last_probe = 0.0
                self.get_logger().info(f'Identifying firmware on {port}')
            chunk = self.link.read(256)
            self.buffer += chunk
            while b'\n' in self.buffer:
                line, self.buffer = self.buffer.split(b'\n', 1)
                if line.strip() == b'VACUUM_SERVO_UART_V2' and not self.ready:
                    self.ready = True
                    self.get_logger().info('Teensy servo firmware identified')
            if len(self.buffer) > 256:
                raise serial.SerialException('Oversized firmware response')
            if not self.ready:
                if now - self.opened > 3.0:
                    raise serial.SerialException('Firmware identity timeout')
                if now - self.last_probe >= 0.5:
                    self.send('HELLO\n')
                    self.last_probe = now
                return
            self.send(f'SPEED {self.target:.3f}\n' if now - self.last_command < self.timeout else 'STOP\n')
        except (serial.SerialException, OSError) as exc:
            self.get_logger().warning(f'Serial connection unavailable: {exc}')
            self.disconnect()

    def destroy_node(self):
        try:
            if self.link and self.ready:
                self.send('STOP\n')
        except (serial.SerialException, OSError):
            pass
        finally:
            self.disconnect()
            super().destroy_node()


def main():
    rclpy.init()
    node = None
    try:
        node = TeensyBridge()
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        if node:
            node.destroy_node()
        if rclpy.ok():
            rclpy.shutdown()


if __name__ == '__main__':
    main()
