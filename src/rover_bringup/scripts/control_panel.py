#!/usr/bin/env python3
import sys

from PyQt5.QtWidgets import (
    QApplication,
    QButtonGroup,
    QHBoxLayout,
    QPushButton,
    QRadioButton,
    QVBoxLayout,
    QWidget,
)


class ControlPanel(QWidget):
    def __init__(self):
        super().__init__()
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
        self.new_button.clicked.connect(lambda: print("New Boundary clicked"))
        self.start_button.clicked.connect(lambda: print("Start Patrol clicked"))
        self.stop_button.clicked.connect(lambda: print("Stop Patrol clicked"))

        mode_row = QHBoxLayout()
        mode_row.addWidget(self.point_radio)
        mode_row.addWidget(self.boundary_radio)

        layout = QVBoxLayout()
        layout.addLayout(mode_row)
        layout.addWidget(self.new_button)
        layout.addWidget(self.start_button)
        layout.addWidget(self.stop_button)
        self.setLayout(layout)

    def on_mode_changed(self):
        boundary = self.boundary_radio.isChecked()
        self.new_button.setEnabled(boundary)
        self.start_button.setEnabled(boundary)
        self.stop_button.setEnabled(boundary)
        print("Mode:", "Boundary" if boundary else "Point to point")


def main():
    app = QApplication(sys.argv)
    panel = ControlPanel()
    panel.show()
    sys.exit(app.exec_())


if __name__ == "__main__":
    main()
