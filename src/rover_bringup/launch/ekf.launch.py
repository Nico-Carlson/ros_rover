import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node
from launch.conditions import IfCondition
from launch.substitutions import PythonExpression


def generate_launch_description():
    bringup_share = get_package_share_directory("rover_bringup")
    use_sim_time_arg = DeclareLaunchArgument("use_sim_time", default_value="true")
    use_sim_time = LaunchConfiguration("use_sim_time")
    sim_arg = DeclareLaunchArgument("sim", default_value="true")
    sim = LaunchConfiguration("sim")

    sim_params = os.path.join(bringup_share, "config", "ekf.yaml")
    hardware_params = os.path.join(bringup_share, "config", "ekf_hardware.yaml")
    ekf_params = PythonExpression(
        ["'", sim_params, "' if '", sim, "' == 'true' else '", hardware_params, "'"]
    )

    ekf_odom = Node(
        package="robot_localization",
        executable="ekf_node",
        name="ekf_filter_node",
        output="screen",
        parameters=[ekf_params, {"use_sim_time": use_sim_time}],
        remappings=[("odometry/filtered", "odometry/local")],
    )

    ekf_map = Node(
        package="robot_localization",
        executable="ekf_node",
        name="ekf_filter_node_map",
        output="screen",
        parameters=[ekf_params, {"use_sim_time": use_sim_time}],
        remappings=[("odometry/filtered", "odometry/global")],
    )

    navsat = Node(
        package="robot_localization",
        executable="navsat_transform_node",
        name="navsat_transform",
        output="screen",
        parameters=[ekf_params, {"use_sim_time": use_sim_time}],
        remappings=[
            ("odometry/filtered", "odometry/global"),
            (
                "gps/fix",
                PythonExpression(
                    ["'gps/fix_cov' if '", sim, "' == 'true' else 'gps/fix'"]
                ),
            ),
        ],
    )

    gps_cov = Node(
        package="rover_bringup",
        executable="gps_covariance.py",
        name="gps_covariance",
        output="screen",
        parameters=[{"use_sim_time": use_sim_time}],
        condition=IfCondition(sim),
    )

    return LaunchDescription(
        [use_sim_time_arg, sim_arg, ekf_odom, ekf_map, navsat, gps_cov]
    )
