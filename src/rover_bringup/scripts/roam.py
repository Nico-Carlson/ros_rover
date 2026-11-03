#!/usr/bin/env python3
import rclpy
from geometry_msgs.msg import PoseStamped
from nav2_msgs.action import NavigateToPose
from rclpy.action import ActionClient
from rclpy.node import Node
from tf2_ros import Buffer, TransformListener

import random
import math
import os

import yaml

EARTH_RADIUS = 6378137.0


def load_boundary(path):
    with open(path) as f:
        data = yaml.safe_load(f)
    lat0 = data["datum"]["lat"]
    lon0 = data["datum"]["lon"]
    polygon = []
    for c in data["corners"]:
        y = math.radians(c["lat"] - lat0) * EARTH_RADIUS
        x = math.radians(c["lon"] - lon0) * EARTH_RADIUS * math.cos(math.radians(lat0))
        polygon.append((x, y))
    return polygon


def point_in_polygon(x, y, polygon):
    inside = False
    n = len(polygon)
    for i in range(n):
        x1, y1 = polygon[i]
        x2, y2 = polygon[(i + 1) % n]
        if (y1 > y) != (y2 > y):
            x_cross = x1 + (y - y1) * (x2 - x1) / (y2 - y1)
            if x < x_cross:
                inside = not inside
    return inside


class VisitGrid:
    def __init__(self, polygon, cell_size=5.0):
        self.cell = cell_size
        self.counts = {}
        self.last_cell = None
        xs = [p[0] for p in polygon]
        ys = [p[1] for p in polygon]
        for i in range(self.index(min(xs), 0)[0], self.index(max(xs), 0)[0] + 1):
            for j in range(self.index(0, min(ys))[1], self.index(0, max(ys))[1] + 1):
                cx, cy = self.center(i, j)
                if point_in_polygon(cx, cy, polygon):
                    self.counts[(i, j)] = 0

    def index(self, x, y):
        return (math.floor(x / self.cell), math.floor(y / self.cell))

    def center(self, i, j):
        return ((i + 0.5) * self.cell, (j + 0.5) * self.cell)

    def mark(self, x, y):
        c = self.index(x, y)
        if c == self.last_cell:
            return
        self.last_cell = c
        if c in self.counts:
            self.counts[c] += 1

    def least_visited(self):
        lowest = min(self.counts.values())
        choices = [c for c, n in self.counts.items() if n == lowest]
        return self.center(*random.choice(choices))

    def covered_around(self, x, y, radius, threshold):
        for (i, j), n in self.counts.items():
            cx, cy = self.center(i, j)
            if math.hypot(cx - x, cy - y) <= radius and n < threshold:
                return False
        return True


class SmoothNoise:
    def __init__(self):
        self.values = {}

    def value(self, i):
        if i not in self.values:
            self.values[i] = random.uniform(-1.0, 1.0)
        return self.values[i]

    def at(self, t):
        i = math.floor(t)
        s = (1.0 - math.cos((t - i) * math.pi)) / 2.0
        return self.value(i) * (1.0 - s) + self.value(i + 1) * s


class Walker:
    def __init__(
        self,
        polygon,
        grid,
        hop_min=5.0,
        hop_max=8.0,
        max_turn=math.radians(60),
        radius=10.0,
        threshold=1,
    ):
        self.polygon = polygon
        self.grid = grid
        self.hop_min = hop_min
        self.hop_max = hop_max
        self.max_turn = max_turn
        self.radius = radius
        self.threshold = threshold
        self.noise = SmoothNoise()
        self.t = 0.0
        self.heading = 0.0
        self.target = None

    def update_target(self, x, y):
        if self.target is None and self.grid.covered_around(
            x, y, self.radius, self.threshold
        ):
            self.target = self.grid.least_visited()
        if self.target is not None:
            tx, ty = self.target
            if math.hypot(tx - x, ty - y) < self.grid.cell:
                self.target = None
            else:
                self.heading = math.atan2(ty - y, tx - x)

    def next_goal(self, x, y):
        self.update_target(x, y)
        self.t += 0.4
        self.heading += self.noise.at(self.t) * self.max_turn
        hop = random.uniform(self.hop_min, self.hop_max)
        for attempt in range(20):
            h = self.heading if attempt == 0 else random.uniform(-math.pi, math.pi)
            gx = x + hop * math.cos(h)
            gy = y + hop * math.sin(h)
            mx = x + hop / 2 * math.cos(h)
            my = y + hop / 2 * math.sin(h)
            if point_in_polygon(gx, gy, self.polygon) and point_in_polygon(
                mx, my, self.polygon
            ):
                self.heading = h
                return gx, gy
        return None


class RoamNode(Node):
    def __init__(self):
        super().__init__("roam")
        self.declare_parameter(
            "boundary_file",
            os.path.expanduser("~/rover_ws/src/rover_bringup/config/boundary.yaml"),
        )
        polygon = load_boundary(self.get_parameter("boundary_file").value)
        self.grid = VisitGrid(polygon)
        self.walker = Walker(polygon, self.grid)
        self.tf_buffer = Buffer()
        self.tf_listener = TransformListener(self.tf_buffer, self)
        self.client = ActionClient(self, NavigateToPose, "navigate_to_pose")
        self.started = False
        self.create_timer(1.0, self.try_start)

    def get_pose(self):
        try:
            t = self.tf_buffer.lookup_transform(
                "map", "base_footprint", rclpy.time.Time()
            )
        except Exception as e:
            self.get_logger().warn(f"No pose yet: {e}")
            return None
        return t.transform.translation.x, t.transform.translation.y

    def try_start(self):
        if self.started:
            return
        if not self.client.server_is_ready():
            self.get_logger().info("Waiting for Nav2...")
            return
        if self.send_next_goal():
            self.started = True

    def send_next_goal(self):
        pose = self.get_pose()
        if pose is None:
            return False
        self.grid.mark(*pose)
        goal = self.walker.next_goal(*pose)
        if goal is None:
            self.get_logger().warn("No goal found inside the boundary")
            return False
        msg = NavigateToPose.Goal()
        msg.pose.header.frame_id = "map"
        msg.pose.header.stamp = self.get_clock().now().to_msg()
        msg.pose.pose.position.x = goal[0]
        msg.pose.pose.position.y = goal[1]
        msg.pose.pose.orientation.z = math.sin(self.walker.heading / 2)
        msg.pose.pose.orientation.w = math.cos(self.walker.heading / 2)
        self.get_logger().info(f"Sending goal ({goal[0]:.1f}, {goal[1]:.1f})")
        self.client.send_goal_async(msg).add_done_callback(self.on_goal_response)
        return True

    def on_goal_response(self, future):
        handle = future.result()
        if not handle.accepted:
            self.get_logger().warn("Goal rejected")
            return
        handle.get_result_async().add_done_callback(self.on_result)

    def on_result(self, future):
        status = future.result().status
        self.get_logger().info(f"Goal finished with status {status}")


def main():
    rclpy.init()
    rclpy.spin(RoamNode())


if __name__ == "__main__":
    main()
