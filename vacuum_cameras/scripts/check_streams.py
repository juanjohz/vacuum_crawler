#!/usr/bin/env python3
"""Verify both cameras deliver raw images, JPEGs, and CameraInfo together."""
import time

import rclpy
from rclpy.qos import qos_profile_sensor_data
from sensor_msgs.msg import CameraInfo, CompressedImage, Image


def main():
    rclpy.init()
    node = rclpy.create_node('check_camera_streams')
    counts = {}
    subscriptions = []
    errors = set()
    for camera in range(2):
        for suffix, kind in [('image_raw', Image),
                             ('image_raw/compressed', CompressedImage),
                             ('camera_info', CameraInfo)]:
            topic = f'/cameras/camera{camera}/{suffix}'
            counts[topic] = 0

            def receive(msg, topic=topic):
                if isinstance(msg, Image):
                    valid = (msg.width > 0 and msg.height > 0 and
                             len(msg.data) == msg.step * msg.height)
                elif isinstance(msg, CompressedImage):
                    valid = len(msg.data) > 2 and bytes(msg.data[:2]) == b'\xff\xd8'
                else:
                    # Uncalibrated camera_ros CameraInfo has zero dimensions.
                    valid = bool(msg.header.frame_id) and (
                        msg.header.stamp.sec > 0 or msg.header.stamp.nanosec > 0)
                if not valid:
                    errors.add(topic)
                counts[topic] += 1

            subscriptions.append(node.create_subscription(
                kind, topic, receive, qos_profile_sensor_data))
    start = time.monotonic()
    try:
        while time.monotonic() - start < 20 and min(counts.values()) < 60:
            rclpy.spin_once(node, timeout_sec=0.2)
        for topic, count in counts.items():
            print(f'{topic}: {count} messages')
        success = min(counts.values()) >= 60 and not errors
        print('PASS' if success else f'FAIL: missing messages or invalid data {sorted(errors)}')
        return 0 if success else 1
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    raise SystemExit(main())
