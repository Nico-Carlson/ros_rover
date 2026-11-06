#!/usr/bin/env python3
import sys
import rclpy
from std_srvs.srv import Empty, Trigger
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

        mode_row = QHBoxLayout()
        mode_row.addWidget(self.point_radio)
        mode_row.addWidget(self.boundary_radio)

        layout = QVBoxLayout()
        layout.addLayout(mode_row)
        layout.addWidget(self.new_button)
        layout.addWidget(self.start_button)
        layout.addWidget(self.stop_button)
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
