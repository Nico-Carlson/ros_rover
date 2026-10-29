from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description():
    return LaunchDescription(
        [
            Node(
                package="rover_basics",
                executable="hello_publisher",
                output="screen",
            ),
            Node(
                package="rover_basics",
                executable="hello_subscriber",
                output="screen",
            ),
        ]
    )
