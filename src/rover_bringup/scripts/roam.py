#!/usr/bin/env python3
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


if __name__ == "__main__":
    path = os.path.expanduser("~/rover_ws/src/rover_bringup/config/boundary.yaml")
    polygon = load_boundary(path)
    print(polygon)
    print("origin inside:", point_in_polygon(0.0, 0.0, polygon))
    print("far point inside:", point_in_polygon(100.0, 100.0, polygon))
