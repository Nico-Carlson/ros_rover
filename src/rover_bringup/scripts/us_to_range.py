#!/usr/bin/env python3
import math

import rclpy
from rclpy.node import Node
from rclpy.qos import qos_profile_sensor_data
from sensor_msgs.msg import LaserScan, Range

NAMES = [
    "front_left",
    "front_mid",
    "front_right",
    "rear_left",
    "rear_mid",
    "rear_right",
]


class UsToRange(Node):
    def __init__(self):
        super().__init__("us_to_range")
        for name in NAMES:
            pub = self.create_publisher(Range, f"us_range/{name}", 10)
            self.create_subscription(
                LaserScan,
                f"us/{name}",
                lambda msg, p=pub: self.convert(msg, p),
                qos_profile_sensor_data,
            )

    def convert(self, scan, pub):
        valid = [
            r
            for r in scan.ranges
            if math.isfinite(r) and scan.range_min <= r <= scan.range_max
        ]
        out = Range()
        out.header = scan.header
        out.radiation_type = Range.ULTRASOUND
        out.field_of_view = 0.262
        out.min_range = scan.range_min
        out.max_range = scan.range_max
        out.range = min(valid) if valid else float("inf")
        pub.publish(out)


def main():
    rclpy.init()
    rclpy.spin(UsToRange())


if __name__ == "__main__":
    main()
