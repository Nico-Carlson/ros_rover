import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    bringup_share = get_package_share_directory("rover_bringup")
    nav2_params = os.path.join(bringup_share, "config", "nav2_params.yaml")

    use_sim_time_arg = DeclareLaunchArgument("use_sim_time", default_value="true")
    use_sim_time = LaunchConfiguration("use_sim_time")

    def nav_node(package, executable, remappings=None):
        return Node(
            package=package,
            executable=executable,
            name=executable,
            output="screen",
            parameters=[nav2_params, {"use_sim_time": use_sim_time}],
            remappings=remappings or [],
        )

    to_nav = [("cmd_vel", "cmd_vel_nav")]

    controller = nav_node("nav2_controller", "controller_server", to_nav)
    smoother = nav_node("nav2_smoother", "smoother_server")
    planner = nav_node("nav2_planner", "planner_server")
    behavior = nav_node("nav2_behaviors", "behavior_server", to_nav)
    bt_nav = nav_node("nav2_bt_navigator", "bt_navigator")
    waypoints = nav_node("nav2_waypoint_follower", "waypoint_follower")
    vel_smoother = nav_node("nav2_velocity_smoother", "velocity_smoother", to_nav)
    collision = nav_node("nav2_collision_monitor", "collision_monitor")

    lifecycle = Node(
        package="nav2_lifecycle_manager",
        executable="lifecycle_manager",
        name="lifecycle_manager_navigation",
        output="screen",
        parameters=[
            {
                "use_sim_time": use_sim_time,
                "autostart": True,
                "node_names": [
                    "controller_server",
                    "smoother_server",
                    "planner_server",
                    "behavior_server",
                    "velocity_smoother",
                    "collision_monitor",
                    "bt_navigator",
                    "waypoint_follower",
                ],
            }
        ],
    )

    return LaunchDescription(
        [
            use_sim_time_arg,
            controller,
            smoother,
            planner,
            behavior,
            bt_nav,
            waypoints,
            vel_smoother,
            collision,
            lifecycle,
        ]
    )
