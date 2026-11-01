#!/usr/bin/env python3
import rclpy
from rclpy.node import Node
from nav_msgs.msg import Odometry


class OdomCovariance(Node):
    def __init__(self):
        super().__init__("odom_covariance")
        self.declare_parameter("speed_variance", 0.01)
        self.declare_parameter("yaw_rate_variance", 0.01)
        self.pub = self.create_publisher(Odometry, "odom_rf2o_cov", 10)
        self.sub = self.create_subscription(Odometry, "odom_rf2o", self.on_odom, 10)

    def on_odom(self, msg):
        cov = [0.0] * 36
        cov[0] = self.get_parameter("speed_variance").value
        cov[35] = self.get_parameter("yaw_rate_variance").value
        msg.twist.covariance = cov
        self.pub.publish(msg)


def main():
    rclpy.init()
    rclpy.spin(OdomCovariance())
    rclpy.shutdown()


if __name__ == "__main__":
    main()
