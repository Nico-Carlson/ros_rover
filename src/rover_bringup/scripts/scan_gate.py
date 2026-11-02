#!/usr/bin/env python3
import rclpy
from rclpy.node import Node
from rclpy.qos import qos_profile_sensor_data
from sensor_msgs.msg import LaserScan
from std_msgs.msg import Bool


class ScanGate(Node):
    def __init__(self):
        super().__init__("scan_gate")
        self.declare_parameter("lift_timeout", 1.0)
        self.lift_down = False
        self.last_lift_time = None
        self.pub = self.create_publisher(LaserScan, "scan_filtered", 10)
        self.create_subscription(
            LaserScan, "scan", self.on_scan, qos_profile_sensor_data
        )
        self.create_subscription(Bool, "lift_down", self.on_lift, 10)

    def on_lift(self, msg):
        self.lift_down = msg.data
        self.last_lift_time = self.get_clock().now()

    def on_scan(self, msg):
        if self.last_lift_time is None:
            return
        age = (self.get_clock().now() - self.last_lift_time).nanoseconds / 1e9
        if self.lift_down and age < self.get_parameter("lift_timeout").value:
            self.pub.publish(msg)


def main():
    rclpy.init()
    rclpy.spin(ScanGate())
    rclpy.shutdown()


if __name__ == "__main__":
    main()
