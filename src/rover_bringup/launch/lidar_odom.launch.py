from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description():
    rf2o = Node(
        package="rf2o_laser_odometry",
        executable="rf2o_laser_odometry_node",
        name="rf2o_laser_odometry",
        output="screen",
        parameters=[
            {
                "laser_scan_topic": "/scan_filtered",
                "odom_topic": "/odom_rf2o",
                "publish_tf": False,
                "base_frame_id": "base_footprint",
                "odom_frame_id": "odom",
                "init_pose_from_topic": "",
                "freq": 10.0,
                "use_sim_time": True,
            }
        ],
    )

    odom_cov = Node(
        package="rover_bringup",
        executable="odom_covariance.py",
        name="odom_covariance",
        output="screen",
        parameters=[{"use_sim_time": True}],
    )

    scan_gate = Node(
        package="rover_bringup",
        executable="scan_gate.py",
        name="scan_gate",
        output="screen",
        parameters=[{"use_sim_time": True}],
    )

    sim_lift = Node(
        package="rover_bringup",
        executable="sim_lift_signal.py",
        name="sim_lift_signal",
        output="screen",
        parameters=[{"use_sim_time": True}],
    )

    return LaunchDescription([scan_gate, sim_lift, rf2o, odom_cov])
