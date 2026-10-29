import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import Command
from launch_ros.actions import Node
from launch_ros.parameter_descriptions import ParameterValue


def generate_launch_description():
    pkg_share = get_package_share_directory("rover_description")
    ros_gz_sim_share = get_package_share_directory("ros_gz_sim")

    world_file = os.path.join(pkg_share, "worlds", "rover_world.sdf")

    xacro_file = os.path.join(pkg_share, "urdf", "rover.urdf.xacro")
    robot_description = ParameterValue(Command(["xacro ", xacro_file]), value_type=str)

    gz_sim = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(ros_gz_sim_share, "launch", "gz_sim.launch.py")
        ),
        launch_arguments={"gz_args": f"-r {world_file}"}.items(),
    )

    robot_state_publisher = Node(
        package="robot_state_publisher",
        executable="robot_state_publisher",
        parameters=[{"robot_description": robot_description, "use_sim_time": True}],
        output="screen",
    )

    spawn_rover = Node(
        package="ros_gz_sim",
        executable="create",
        arguments=["-topic", "robot_description", "-name", "rover", "-z", "0.3"],
        output="screen",
    )

    return LaunchDescription([gz_sim, robot_state_publisher, spawn_rover])
