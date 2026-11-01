import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description():
    bringup_share = get_package_share_directory("rover_bringup")
    ekf_params = os.path.join(bringup_share, "config", "ekf.yaml")

    ekf_odom = Node(
        package="robot_localization",
        executable="ekf_node",
        name="ekf_filter_node",
        output="screen",
        parameters=[ekf_params, {"use_sim_time": True}],
        remappings=[("odometry/filtered", "odometry/local")],
    )

    ekf_map = Node(
        package="robot_localization",
        executable="ekf_node",
        name="ekf_filter_node_map",
        output="screen",
        parameters=[ekf_params, {"use_sim_time": True}],
        remappings=[("odometry/filtered", "odometry/global")],
    )

    navsat = Node(
        package="robot_localization",
        executable="navsat_transform_node",
        name="navsat_transform",
        output="screen",
        parameters=[ekf_params, {"use_sim_time": True}],
        remappings=[("odometry/filtered", "odometry/global")],
    )

    return LaunchDescription([ekf_odom, ekf_map, navsat])
