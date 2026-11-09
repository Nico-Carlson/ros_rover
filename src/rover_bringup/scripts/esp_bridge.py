#!/usr/bin/env python3
import math
import time

import rclpy
from rclpy.node import Node
from std_msgs.msg import Bool, Float32

from bridge import Bridge, LiftState


class EspBridge(Node):
    def __init__(self):
        super().__init__("esp_bridge")
        self.declare_parameter("port", "/dev/ttyUSB0")  # PLACEHOLDER
        self.declare_parameter("baud", 115200)
        self.declare_parameter("lift_timeout", 1.0)  # PLACEHOLDER, matches scan gate

        port = self.get_parameter("port").value
        baud = self.get_parameter("baud").value
        self.bridge = Bridge(port, baud)
        self.lift = LiftState(self.get_parameter("lift_timeout").value)

        self.lift_pub = self.create_publisher(Bool, "/lift_down", 10)
        self.bat_pub = self.create_publisher(Float32, "/battery_voltage", 10)

        self.create_timer(0.02, self.on_poll)  # read the serial port at 50 Hz
        self.create_timer(0.1, self.on_lift_timer)  # publish /lift_down at 10 Hz
        self.get_logger().info(f"esp_bridge reading {port} at {baud}")

    def on_poll(self):
        for kind, fields in self.bridge.poll_with_lift(self.lift):
            if kind == "BAT":
                self.publish_battery(fields)

    def publish_battery(self, fields):
        if len(fields) != 1:
            return
        try:
            volts = float(fields[0])
        except ValueError:
            return
        if not math.isfinite(volts):
            return
        msg = Float32()
        msg.data = volts
        self.bat_pub.publish(msg)

    def on_lift_timer(self):
        msg = Bool()
        msg.data = self.lift.lift_down(time.time())
        self.lift_pub.publish(msg)


def main():
    rclpy.init()
    node = EspBridge()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":
    main()
