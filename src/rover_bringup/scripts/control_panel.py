#!/usr/bin/env python3
import sys
import time
from PyQt5.QtCore import Qt
from PyQt5.QtWidgets import QGroupBox, QSlider
from rcl_interfaces.msg import Parameter as ParamMsg
from rcl_interfaces.msg import ParameterType, ParameterValue
from rcl_interfaces.srv import SetParameters
import rclpy
from std_srvs.srv import Empty, Trigger
from std_msgs.msg import String
from rclpy.node import Node
from PyQt5.QtCore import QTimer

from PyQt5.QtWidgets import (
    QApplication,
    QButtonGroup,
    QHBoxLayout,
    QLabel,
    QPushButton,
    QRadioButton,
    QVBoxLayout,
    QWidget,
)


class ParamSlider(QWidget):
    def __init__(self, node, target, name, title, low, high, step, initial):
        super().__init__()
        self.name = name
        self.title = title
        self.step = step
        self.client = node.create_client(SetParameters, f"/{target}/set_parameters")
        self.label = QLabel()
        self.slider = QSlider(Qt.Horizontal)
        self.slider.setRange(round(low / step), round(high / step))
        self.slider.setValue(round(initial / step))
        self.slider.valueChanged.connect(self.on_changed)
        self.slider.sliderReleased.connect(self.send)
        layout = QVBoxLayout()
        layout.setContentsMargins(0, 0, 0, 0)
        layout.addWidget(self.label)
        layout.addWidget(self.slider)
        self.setLayout(layout)
        self.on_changed()

    def value(self):
        return self.slider.value() * self.step

    def on_changed(self):
        self.label.setText(f"{self.title}: {self.value():g}")
        if not self.slider.isSliderDown():
            self.send()

    def send(self):
        if not self.client.service_is_ready():
            self.label.setText(f"{self.title}: {self.value():g} (node not running)")
            return
        p = ParamMsg()
        p.name = self.name
        p.value = ParameterValue(
            type=ParameterType.PARAMETER_DOUBLE, double_value=float(self.value())
        )
        req = SetParameters.Request()
        req.parameters = [p]
        future = self.client.call_async(req)
        future.add_done_callback(self.on_reply)

    def on_reply(self, future):
        result = future.result().results[0]
        if not result.successful:
            self.label.setText(f"{self.title}: {self.value():g} (rejected)")


class ControlPanel(QWidget):
    def __init__(self, node):
        super().__init__()
        self.node = node
        self.start_client = node.create_client(Trigger, "start_patrol")
        self.stop_client = node.create_client(Trigger, "stop_patrol")
        self.clear_client = node.create_client(Empty, "clear_boundary")
        self.setWindowTitle("Rover Control")

        self.point_radio = QRadioButton("Point to point")
        self.boundary_radio = QRadioButton("Boundary")
        self.boundary_radio.setChecked(True)
        self.mode_group = QButtonGroup(self)
        self.mode_group.addButton(self.point_radio)
        self.mode_group.addButton(self.boundary_radio)
        self.point_radio.toggled.connect(self.on_mode_changed)

        self.new_button = QPushButton("New Boundary")
        self.start_button = QPushButton("Start Patrol")
        self.stop_button = QPushButton("Stop Patrol")
        self.new_button.clicked.connect(self.on_new_boundary)
        self.start_button.clicked.connect(
            lambda: self.call_trigger(self.start_client, "Start")
        )
        self.stop_button.clicked.connect(
            lambda: self.call_trigger(self.stop_client, "Stop")
        )
        self.status = QLabel("Ready")
        self.patrol_label = QLabel("Patrol: waiting for status")
        self.patrol_label.setWordWrap(True)
        self.last_status_time = None
        node.create_subscription(String, "patrol_status", self.on_patrol_status, 10)
        self.stale_timer = QTimer(self)
        self.stale_timer.timeout.connect(self.check_stale)
        self.stale_timer.start(1000)

        mode_row = QHBoxLayout()
        mode_row.addWidget(self.point_radio)
        mode_row.addWidget(self.boundary_radio)

        layout = QVBoxLayout()
        layout.addLayout(mode_row)
        layout.addWidget(self.new_button)
        layout.addWidget(self.start_button)
        layout.addWidget(self.stop_button)
        roam_box = QGroupBox("Roam")
        roam_layout = QVBoxLayout()
        roam_layout.addWidget(
            ParamSlider(
                node, "roam", "goal_timeout", "Goal timeout (s)", 10, 300, 5, 90
            )
        )
        roam_box.setLayout(roam_layout)
        nav_box = QGroupBox("Nav2")
        nav_layout = QVBoxLayout()
        nav_layout.addWidget(
            ParamSlider(
                node,
                "controller_server",
                "FollowPath.vx_max",
                "Max speed (m/s)",
                0.1,
                1.0,
                0.05,
                0.65,
            )
        )
        nav_layout.addWidget(
            ParamSlider(
                node,
                "controller_server",
                "FollowPath.wz_max",
                "Max turn rate (rad/s)",
                0.2,
                2.0,
                0.1,
                1.0,
            )
        )
        nav_layout.addWidget(
            ParamSlider(
                node,
                "controller_server",
                "general_goal_checker.yaw_goal_tolerance",
                "Yaw tolerance (rad)",
                0.1,
                3.14,
                0.1,
                3.14,
            )
        )
        nav_layout.addWidget(
            ParamSlider(
                node,
                "controller_server",
                "general_goal_checker.xy_goal_tolerance",
                "XY tolerance (m)",
                0.1,
                2.0,
                0.1,
                0.5,
            )
        )
        nav_layout.addWidget(
            ParamSlider(
                node,
                "local_costmap/local_costmap",
                "inflation_layer.inflation_radius",
                "Local inflation (m)",
                0.2,
                3.5,
                0.1,
                1.5,
            )
        )
        nav_layout.addWidget(
            ParamSlider(
                node,
                "global_costmap/global_costmap",
                "inflation_layer.inflation_radius",
                "Global inflation (m)",
                0.2,
                3.5,
                0.1,
                1.5,
            )
        )
        nav_layout.addWidget(
            ParamSlider(
                node,
                "local_costmap/local_costmap",
                "inflation_layer.cost_scaling_factor",
                "Local cost falloff",
                0.1,
                10.0,
                0.1,
                1.0,
            )
        )
        nav_layout.addWidget(
            ParamSlider(
                node,
                "global_costmap/global_costmap",
                "inflation_layer.cost_scaling_factor",
                "Global cost falloff",
                0.1,
                10.0,
                0.1,
                1.0,
            )
        )

        nav_box.setLayout(nav_layout)
        layout.addWidget(nav_box)
        layout.addWidget(roam_box)
        layout.addWidget(self.patrol_label)
        layout.addWidget(self.status)
        self.setLayout(layout)

    def call_trigger(self, client, name):
        if not client.service_is_ready():
            self.status.setText(f"{name}: roam is not running")
            return
        future = client.call_async(Trigger.Request())
        future.add_done_callback(
            lambda f: self.status.setText(f"{name}: {f.result().message}")
        )

    def on_new_boundary(self):
        if self.stop_client.service_is_ready():
            self.stop_client.call_async(Trigger.Request())
        if not self.clear_client.service_is_ready():
            self.status.setText("New: boundary canvas is not running")
            return
        future = self.clear_client.call_async(Empty.Request())
        future.add_done_callback(
            lambda f: self.status.setText("New: click corners in RViz")
        )

    def on_patrol_status(self, msg):
        self.last_status_time = time.monotonic()
        self.patrol_label.setText(f"Patrol: {msg.data}")

    def check_stale(self):
        if self.last_status_time is None:
            return
        if time.monotonic() - self.last_status_time > 5.0:
            self.patrol_label.setText("Patrol: no status (roam not running?)")

    def on_mode_changed(self):
        boundary = self.boundary_radio.isChecked()
        self.new_button.setEnabled(boundary)
        self.start_button.setEnabled(boundary)
        self.stop_button.setEnabled(boundary)
        print("Mode:", "Boundary" if boundary else "Point to point")


def main():
    rclpy.init()
    node = Node("control_panel")
    app = QApplication(sys.argv)
    panel = ControlPanel(node)
    panel.show()
    timer = QTimer()
    timer.timeout.connect(lambda: rclpy.spin_once(node, timeout_sec=0))
    timer.start(50)
    code = app.exec_()
    node.destroy_node()
    rclpy.shutdown()
    sys.exit(code)


if __name__ == "__main__":
    main()
