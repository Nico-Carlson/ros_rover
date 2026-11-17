import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description():
    rviz_config = os.path.join(
        get_package_share_directory("rover_description"), "rviz", "rover.rviz"
    )

    rviz = Node(
        package="rviz2",
        executable="rviz2",
        arguments=["-d", rviz_config],
        parameters=[{"use_sim_time": False}],
        output="screen",
    )

    control_panel = Node(
        package="rover_bringup",
        executable="control_panel.py",
        output="screen",
    )

    return LaunchDescription([rviz, control_panel])
