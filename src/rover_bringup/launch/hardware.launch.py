import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription, TimerAction
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def include(package, launch_file, args=None):
    share = get_package_share_directory(package)
    return IncludeLaunchDescription(
        PythonLaunchDescriptionSource(os.path.join(share, "launch", launch_file)),
        launch_arguments=(args or {}).items(),
    )


def generate_launch_description():
    # PLACEHOLDERS: ttyUSB numbers can swap between boots. Replace with stable
    # /dev/serial/by-id/... paths once the Pi is connected to both ESPs.
    master_port_arg = DeclareLaunchArgument(
        "master_port", default_value="/dev/ttyUSB0"
    )  # PLACEHOLDER (MODE out, CMD in)
    sensor_port_arg = DeclareLaunchArgument(
        "sensor_port", default_value="/dev/ttyUSB1"
    )  # PLACEHOLDER (LIFT and BAT out)
    master_port = LaunchConfiguration("master_port")
    sensor_port = LaunchConfiguration("sensor_port")

    # TODO: the RPLIDAR S2 driver goes here, publishing /scan with frame lidar_link.
    # TODO: GPS and IMU publishers (/gps/fix with real covariance, /imu) are not wired to the Pi yet.
    # esp_bridge has two ports (master and sensor ESP); it exits at startup if either is missing.

    esp_bridge = Node(
        package="rover_bringup",
        executable="esp_bridge.py",
        name="esp_bridge",
        output="screen",
        parameters=[
            {
                "master_port": master_port,
                "sensor_port": sensor_port,
                "use_sim_time": False,
            }
        ],
    )

    lidar_odom = include(
        "rover_bringup",
        "lidar_odom.launch.py",
        {"use_sim_time": "false", "sim": "false"},
    )
    ekf = include(
        "rover_bringup",
        "ekf.launch.py",
        {"use_sim_time": "false", "sim": "false"},
    )
    nav2 = include("rover_bringup", "nav2.launch.py", {"use_sim_time": "false"})

    boundary_canvas = Node(
        package="rover_bringup",
        executable="boundary_canvas.py",
        parameters=[{"use_sim_time": False}],
        output="screen",
    )
    roam = Node(
        package="rover_bringup",
        executable="roam.py",
        parameters=[{"use_sim_time": False}],
        output="screen",
    )

    return LaunchDescription(
        [
            master_port_arg,
            sensor_port_arg,
            esp_bridge,
            TimerAction(period=5.0, actions=[lidar_odom]),
            TimerAction(period=10.0, actions=[ekf]),
            TimerAction(period=17.0, actions=[nav2]),
            TimerAction(period=21.0, actions=[boundary_canvas]),
            TimerAction(period=25.0, actions=[roam]),
        ]
    )
