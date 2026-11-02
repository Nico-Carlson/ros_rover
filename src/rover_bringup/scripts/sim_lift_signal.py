#!/usr/bin/env python3
import rclpy
from rclpy.node import Node
from sensor_msgs.msg import JointState
from std_msgs.msg import Bool


class SimLiftSignal(Node):
    def __init__(self):
        super().__init__("sim_lift_signal")
        self.declare_parameter("down_threshold", 0.01)
        self.pub = self.create_publisher(Bool, "lift_down", 10)
        self.create_subscription(JointState, "joint_states", self.on_joints, 10)

    def on_joints(self, msg):
        if "lift_joint" not in msg.name:
            return
        pos = msg.position[msg.name.index("lift_joint")]
        out = Bool()
        out.data = pos < self.get_parameter("down_threshold").value
        self.pub.publish(out)


def main():
    rclpy.init()
    rclpy.spin(SimLiftSignal())
    rclpy.shutdown()


if __name__ == "__main__":
    main()
