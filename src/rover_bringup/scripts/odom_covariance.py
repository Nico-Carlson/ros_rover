#!/usr/bin/env python3
import rclpy
import math
from rclpy.node import Node
from nav_msgs.msg import Odometry


class OdomCovariance(Node):
    def __init__(self):
        super().__init__("odom_covariance")
        self.declare_parameter("speed_variance", 0.01)
        self.declare_parameter("yaw_rate_variance", 0.01)
        self.declare_parameter("max_speed", 1.5)
        self.declare_parameter("max_yaw_rate", 2.0)
        self.pub = self.create_publisher(Odometry, "odom_rf2o_cov", 10)
        self.sub = self.create_subscription(Odometry, "odom_rf2o", self.on_odom, 10)

    def on_odom(self, msg):
        speed = msg.twist.twist.linear.x
        yaw_rate = msg.twist.twist.angular.z
        if (
            not math.isfinite(speed)
            or not math.isfinite(yaw_rate)
            or abs(speed) > self.get_parameter("max_speed").value
            or abs(yaw_rate) > self.get_parameter("max_yaw_rate").value
        ):
            self.get_logger().warn(
                f"Dropped rf2o reading: {speed:.2f} m/s, {yaw_rate:.2f} rad/s",
                throttle_duration_sec=2.0,
            )
            return
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
