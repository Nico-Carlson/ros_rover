#!/usr/bin/env python3
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


if __name__ == "__main__":
    path = os.path.expanduser("~/rover_ws/src/rover_bringup/config/boundary.yaml")
    polygon = load_boundary(path)
    grid = VisitGrid(polygon)
    print("cells:", len(grid.counts))
    grid.mark(0.0, 0.0)
    grid.mark(0.5, 0.5)
    grid.mark(6.0, 0.0)
    print("origin cell count:", grid.counts[grid.index(0.0, 0.0)])
    print("least visited:", grid.least_visited())
    print("covered around origin:", grid.covered_around(0.0, 0.0, 10.0, 1))
