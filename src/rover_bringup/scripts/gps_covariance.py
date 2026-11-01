#!/usr/bin/env python3
import rclpy
from rclpy.node import Node
from sensor_msgs.msg import NavSatFix


class GpsCovariance(Node):
    def __init__(self):
        super().__init__("gps_covariance")
        self.declare_parameter("horizontal_variance", 2.25)
        self.declare_parameter("vertical_variance", 9.0)
        self.pub = self.create_publisher(NavSatFix, "gps/fix_cov", 10)
        self.sub = self.create_subscription(NavSatFix, "gps/fix", self.on_fix, 10)

    def on_fix(self, msg):
        h = self.get_parameter("horizontal_variance").value
        v = self.get_parameter("vertical_variance").value
        msg.position_covariance = [h, 0.0, 0.0, 0.0, h, 0.0, 0.0, 0.0, v]
        msg.position_covariance_type = NavSatFix.COVARIANCE_TYPE_DIAGONAL_KNOWN
        self.pub.publish(msg)


def main():
    rclpy.init()
    rclpy.spin(GpsCovariance())
    rclpy.shutdown()


if __name__ == "__main__":
    main()
