import rclpy
from rclpy.node import Node
from std_msgs.msg import String


class HelloPublisher(Node):
    def __init__(self):
        super().__init__("hello_publisher")
        self.publisher = self.create_publisher(String, "hello", 10)
        self.timer = self.create_timer(1.0, self.timer_callback)
        self.count = 0

    def timer_callback(self):
        msg = String()
        msg.data = f"Hello from the rover: {self.count}"
        self.publisher.publish(msg)
        self.get_logger().info(f"Publishing: {msg.data}")
        self.count += 1


def main(args=None):
    rclpy.init(args=args)
    node = HelloPublisher()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        if rclpy.ok():
            rclpy.shutdown()


if __name__ == "__main__":
    main()
