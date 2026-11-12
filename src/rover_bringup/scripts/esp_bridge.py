#!/usr/bin/env python3
import math
import time

import rclpy
from rclpy.node import Node
from geometry_msgs.msg import Twist
from std_msgs.msg import Bool, Float32

from bridge import Bridge, LiftState
from kinematics import twist_to_steer_deg
from protocol import parse_mode


class EspBridge(Node):
    def __init__(self):
        super().__init__("esp_bridge")
        self.declare_parameter("port", "/dev/ttyUSB0")  # PLACEHOLDER
        self.declare_parameter("baud", 115200)
        self.declare_parameter("lift_timeout", 1.0)  # PLACEHOLDER, matches scan gate
        self.declare_parameter("wheelbase", 0.686)
        self.declare_parameter("max_steer_deg", 30.0)  # PLACEHOLDER
        self.declare_parameter("cmd_rate_hz", 20.0)
        self.declare_parameter("cmd_timeout", 0.5)  # PLACEHOLDER, seconds
        self.declare_parameter("mode_timeout", 0.5)  # PLACEHOLDER, seconds

        port = self.get_parameter("port").value
        baud = self.get_parameter("baud").value
        self.bridge = Bridge(port, baud)
        self.lift = LiftState(self.get_parameter("lift_timeout").value)

        self.lift_pub = self.create_publisher(Bool, "/lift_down", 10)
        self.bat_pub = self.create_publisher(Float32, "/battery_voltage", 10)

        self.mode_timeout = self.get_parameter("mode_timeout").value
        self.mode = (False, False, False, False)  # autonomous, danger, warning, brake
        self.mode_time = None
        self.auto_pub = self.create_publisher(Bool, "/rover/autonomous", 10)
        self.danger_pub = self.create_publisher(Bool, "/rover/danger", 10)
        self.warning_pub = self.create_publisher(Bool, "/rover/warning", 10)
        self.brake_pub = self.create_publisher(Bool, "/rover/brake", 10)
        self.link_pub = self.create_publisher(Bool, "/rover/esp_link_ok", 10)
        self.create_timer(0.1, self.on_mode_timer)  # publish the MODE topics at 10 Hz

        self.create_timer(0.02, self.on_poll)  # read the serial port at 50 Hz
        self.create_timer(0.1, self.on_lift_timer)  # publish /lift_down at 10 Hz
        self.wheelbase = self.get_parameter("wheelbase").value
        self.max_steer_deg = self.get_parameter("max_steer_deg").value
        self.cmd_timeout = self.get_parameter("cmd_timeout").value
        self.cmd_v = 0.0
        self.cmd_wz = 0.0
        self.cmd_time = None
        self.create_subscription(Twist, "/cmd_vel", self.on_cmd_vel, 10)
        self.create_timer(
            1.0 / self.get_parameter("cmd_rate_hz").value, self.on_send_timer
        )
        self.get_logger().info(f"esp_bridge reading {port} at {baud}")

    def on_poll(self):
        for kind, fields in self.bridge.poll_with_lift(self.lift):
            if kind == "BAT":
                self.publish_battery(fields)
            elif kind == "MODE":
                parsed = parse_mode(fields)
                if parsed is not None:
                    self.mode = parsed
                    self.mode_time = time.time()

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

    def publish_bool(self, pub, value):
        msg = Bool()
        msg.data = bool(value)
        pub.publish(msg)

    def on_mode_timer(self):
        link_ok = (
            self.mode_time is not None
            and time.time() - self.mode_time <= self.mode_timeout
        )
        autonomous, danger, warning, brake = self.mode
        if not link_ok:
            autonomous = False  # silent ESP: do not claim the rover is in auto
        self.publish_bool(self.auto_pub, autonomous)
        self.publish_bool(self.danger_pub, danger)
        self.publish_bool(self.warning_pub, warning)
        self.publish_bool(self.brake_pub, brake)
        self.publish_bool(self.link_pub, link_ok)

    def on_cmd_vel(self, msg):
        self.cmd_v = msg.linear.x
        self.cmd_wz = msg.angular.z
        self.cmd_time = time.time()

    def on_send_timer(self):
        fresh = (
            self.cmd_time is not None
            and time.time() - self.cmd_time <= self.cmd_timeout
        )
        speed = self.cmd_v if fresh else 0.0
        wz = self.cmd_wz if fresh else 0.0
        if not (math.isfinite(speed) and math.isfinite(wz)):
            speed = 0.0
            wz = 0.0
        steer = twist_to_steer_deg(speed, wz, self.wheelbase, self.max_steer_deg)
        self.bridge.send_cmd(speed, steer, 0.0)


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
