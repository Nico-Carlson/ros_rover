#!/usr/bin/env python3
import os
import yaml
import math
from std_srvs.srv import Empty
import rclpy
from rclpy.node import Node
from visualization_msgs.msg import Marker
from geometry_msgs.msg import Point, PointStamped

EARTH_RADIUS = 6378137.0


class BoundaryCanvas(Node):
    def __init__(self):
        super().__init__("boundary_canvas")
        self.pub = self.create_publisher(Marker, "boundary_canvas", 1)
        self.create_timer(1.0, self.publish_canvas)
        self.points = []
        self.closed = False
        self.declare_parameter("datum_lat", 45.0)
        self.declare_parameter("datum_lon", -110.0)
        self.declare_parameter(
            "output_file",
            os.path.expanduser("~/rover_ws/src/rover_bringup/config/boundary.yaml"),
        )
        self.datum_lat = self.get_parameter("datum_lat").value
        self.datum_lon = self.get_parameter("datum_lon").value
        self.output_file = self.get_parameter("output_file").value
        self.create_service(Empty, "clear_boundary", self.on_clear)
        self.line_pub = self.create_publisher(Marker, "boundary_line", 1)
        self.create_subscription(PointStamped, "clicked_point", self.on_click, 10)

    def publish_canvas(self):
        m = Marker()
        m.header.frame_id = "map"
        m.ns = "canvas"
        m.id = 0
        m.type = Marker.CUBE
        m.action = Marker.ADD
        m.pose.position.z = -0.02
        m.pose.orientation.w = 1.0
        m.scale.x = 200.0
        m.scale.y = 200.0
        m.scale.z = 0.02
        m.color.r = 0.1
        m.color.g = 0.25
        m.color.b = 0.1
        m.color.a = 1.0
        self.pub.publish(m)

    def on_click(self, msg):
        if msg.header.frame_id != "map":
            self.get_logger().warn("Click is not in the map frame, ignored")
            return
        if self.closed:
            self.get_logger().warn("Boundary already closed, ignored")
            return
        if len(self.points) >= 3 and self.near_first(msg.point):
            self.closed = True
            self.save()
            self.get_logger().info(f"Boundary closed with {len(self.points)} corners")
            self.publish_line()
            return
        self.points.append((msg.point.x, msg.point.y))
        self.get_logger().info(
            f"Point {len(self.points)}: ({msg.point.x:.1f}, {msg.point.y:.1f})"
        )
        self.publish_line()

    def near_first(self, p):
        x0, y0 = self.points[0]
        return math.hypot(p.x - x0, p.y - y0) < 3.0

    def on_clear(self, request, response):
        self.points = []
        self.closed = False
        self.publish_line()
        self.get_logger().info("Boundary cleared")
        return response

    def save(self):
        corners = []
        for x, y in self.points:
            lat = self.datum_lat + math.degrees(y / EARTH_RADIUS)
            lon = self.datum_lon + math.degrees(
                x / (EARTH_RADIUS * math.cos(math.radians(self.datum_lat)))
            )
            corners.append({"lat": round(lat, 7), "lon": round(lon, 7)})
        data = {
            "datum": {"lat": self.datum_lat, "lon": self.datum_lon},
            "corners": corners,
        }
        with open(self.output_file, "w") as f:
            yaml.safe_dump(data, f, sort_keys=False)
        self.get_logger().info(f"Saved {len(corners)} corners to {self.output_file}")

    def publish_line(self):
        m = Marker()
        m.header.frame_id = "map"
        m.ns = "boundary"
        m.id = 0
        m.type = Marker.LINE_STRIP
        m.action = Marker.ADD
        m.pose.orientation.w = 1.0
        m.scale.x = 0.5
        m.color.r = 1.0
        m.color.g = 0.8
        m.color.a = 1.0
        for x, y in self.points:
            m.points.append(Point(x=x, y=y, z=0.0))
        if self.closed:
            m.points.append(Point(x=self.points[0][0], y=self.points[0][1], z=0.0))
        if not self.points:
            m.action = Marker.DELETE
        self.line_pub.publish(m)


def main():
    rclpy.init()
    rclpy.spin(BoundaryCanvas())


if __name__ == "__main__":
    main()
