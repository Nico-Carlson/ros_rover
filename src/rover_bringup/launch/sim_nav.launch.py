from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch_ros.actions import Node
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription, TimerAction
from launch.substitutions import LaunchConfiguration
import os


def include(package, launch_file, launch_arguments=None):
    share = get_package_share_directory(package)
    return IncludeLaunchDescription(
        PythonLaunchDescriptionSource(os.path.join(share, "launch", launch_file)),
        launch_arguments=(launch_arguments or {}).items(),
    )


def generate_launch_description():
    sim = include("rover_description", "sim.launch.py")
    lidar_odom = include("rover_bringup", "lidar_odom.launch.py")
    ekf = include("rover_bringup", "ekf.launch.py")
    nav2_params_arg = DeclareLaunchArgument(
        "nav2_params",
        default_value=os.path.join(
            get_package_share_directory("rover_bringup"), "config", "nav2_params.yaml"
        ),
    )
    nav2 = include(
        "rover_bringup",
        "nav2.launch.py",
        {"params_file": LaunchConfiguration("nav2_params")},
    )

    rviz_config = os.path.join(
        get_package_share_directory("rover_description"), "rviz", "rover.rviz"
    )
    rviz = Node(
        package="rviz2",
        executable="rviz2",
        arguments=["-d", rviz_config],
        parameters=[{"use_sim_time": True}],
        output="screen",
    )

    boundary_canvas = Node(
        package="rover_bringup",
        executable="boundary_canvas.py",
        parameters=[{"use_sim_time": True}],
        output="screen",
    )
    roam = Node(
        package="rover_bringup",
        executable="roam.py",
        parameters=[{"use_sim_time": True}],
        output="screen",
    )

    control_panel = Node(
        package="rover_bringup",
        executable="control_panel.py",
        output="screen",
    )

    us_to_range = Node(
        package="rover_bringup",
        executable="us_to_range.py",
        output="screen",
    )

    return LaunchDescription(
        [
            nav2_params_arg,
            sim,
            TimerAction(period=10.0, actions=[lidar_odom]),
            TimerAction(period=10.0, actions=[us_to_range]),
            TimerAction(period=15.0, actions=[ekf]),
            TimerAction(period=22.0, actions=[nav2]),
            TimerAction(period=25.0, actions=[rviz]),
            TimerAction(period=26.0, actions=[boundary_canvas]),
            TimerAction(period=30.0, actions=[roam]),
            TimerAction(period=30.0, actions=[control_panel]),
        ]
    )
