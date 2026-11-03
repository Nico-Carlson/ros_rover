# ROS Rover

ROS 2 software and simulation for an autonomous Ackermann-steered rover intended
for forest navigation and outdoor data collection. The project models the rover
in Gazebo, publishes its simulated sensors into ROS 2, estimates its position,
and uses Nav2 to plan and follow paths around obstacles.

The long-term rover is intended to support:

- Navigation to a GPS location while avoiding obstacles
- Roaming within a defined boundary
- Recovery when the rover becomes stuck
- Camera-data collection for a separate school project

The repository currently focuses on the robot model, simulation, localization,
SLAM, scan gating, and simulated Nav2 navigation. Real-hardware integration is
planned but is not complete.

## Current status

The following workflows are implemented and tested in simulation:

- Ackermann rover model with four wheels and steering
- Gazebo Harmonic simulation with an obstacle world
- RPLIDAR-style 2D laser scan
- Simulated IMU, GPS, wheel/joint states, and six ultrasonic sensors
- LIDAR odometry using `rf2o_laser_odometry`
- Dual `robot_localization` EKFs for local and global state estimates
- GPS covariance processing and GPS fusion in the global filter
- Scan gating so navigation uses the scan only when the simulated lift is down
- SLAM Toolbox mapping
- Nav2 path planning and control using rolling costmaps
- RViz visualization and interactive goal selection

Not yet implemented for the physical rover:

- ESP32 firmware and the Raspberry Pi hardware bridge
- Real sensor drivers and field-tested calibration
- Boundary roaming and stuck recovery
- Final sensor measurements and navigation tuning
- A reliable real-world heading source for GPS fusion

## System overview

The main simulated data flow is:

```text
Gazebo Harmonic
    |
    +-- ROS-Gazebo bridge --> /scan, /imu, /gps/fix, /odom, /joint_states
    |
    +-- scan_gate --> /scan_filtered --> rf2o_laser_odometry
    |                                   |
    |                                   +--> /odom_rf2o_cov
    |
    +-- robot_localization EKFs
            |
            +--> /odometry/local  --> Nav2
            +--> /odometry/global --> map-frame navigation

RViz 2D Goal Pose --> Nav2 --> /cmd_vel --> Gazebo rover
```

The rover uses `base_footprint` as its ground-level navigation frame and
`base_link` above it. The simulated LIDAR is mounted on a lift platform; the
navigation stack is designed to operate with the lift fully lowered.

## Repository layout

```text
src/
├── rover_basics/
│   ├── rover_basics/       # Simple publisher/subscriber examples
│   ├── launch/             # Launches both example nodes
│   └── test/               # Python package checks
├── rover_description/
│   ├── urdf/               # Rover model and Gazebo sensor plugins
│   ├── worlds/             # Gazebo world
│   ├── launch/             # Visualization and simulation launch files
│   └── rviz/               # Saved RViz configuration
└── rover_bringup/
    ├── config/             # EKF, SLAM, and Nav2 parameters
    ├── launch/             # Localization, SLAM, odometry, and Nav2
    └── scripts/            # Covariance and scan-gating helper nodes
```

The `rf2o_laser_odometry` package is an external dependency. It is intentionally
ignored by this repository and must be available in the same ROS workspace.

## Requirements

The project is developed with:

- Ubuntu 24.04
- ROS 2 Jazzy
- Gazebo Harmonic
- `colcon`
- Nav2
- `robot_localization`
- `slam_toolbox`
- `ros_gz_sim` and `ros_gz_bridge`
- `rf2o_laser_odometry`
- `teleop_twist_keyboard` for optional manual driving

Install the ROS packages required by the launch files using the package manager
for your ROS 2 Jazzy installation. The external `rf2o_laser_odometry` package
must also be cloned into the workspace `src` directory.

## Build the workspace

Clone the repository into a ROS 2 workspace, install the dependencies, and
build the packages:

```bash
mkdir -p ~/rover_ws/src
cd ~/rover_ws/src
git clone https://github.com/Nico-Carlson/ros_rover.git

# Place rf2o_laser_odometry in ~/rover_ws/src as well.

cd ~/rover_ws
source /opt/ros/jazzy/setup.bash
rosdep install --from-paths src --ignore-src -r -y
colcon build
source install/setup.bash
```

After changing a package, rebuild that package before launching it. ROS launch
files use the installed copy of the package:

```bash
colcon build --packages-select rover_description rover_bringup
source install/setup.bash
```

## Run the simulation

The recommended command starts Gazebo, scan gating, LIDAR odometry, both EKFs,
Nav2, and RViz in sequence:

```bash
source /opt/ros/jazzy/setup.bash
source ~/rover_ws/install/setup.bash
ros2 launch rover_bringup sim_nav.launch.py
```

The combined launch uses startup delays so each subsystem can provide the
topics and transforms required by the next one. Nav2 may take approximately
30 seconds after launch to become active.

In RViz, use **2D Goal Pose** to send a navigation goal. The rover should plan
and drive toward the goal in the simulated obstacle world.

To drive manually when Nav2 has no active goal, open another terminal and run:

```bash
source /opt/ros/jazzy/setup.bash
source ~/rover_ws/install/setup.bash
ros2 run teleop_twist_keyboard teleop_twist_keyboard
```

Stop the launch process with `Ctrl+C`. Closing the Gazebo window alone does not
stop all of the ROS and Gazebo processes.

## Run individual components

The combined launch is convenient for navigation testing, but the components
can also be started separately:

```bash
# Terminal 1: Gazebo, robot model, and ROS-Gazebo bridge
ros2 launch rover_description sim.launch.py

# Terminal 2: Scan gate and LIDAR odometry
ros2 launch rover_bringup lidar_odom.launch.py

# Terminal 3: Local and global EKFs
ros2 launch rover_bringup ekf.launch.py

# Terminal 4: Nav2
ros2 launch rover_bringup nav2.launch.py
```

For hardware-oriented launches, the localization and Nav2 launch files accept
`use_sim_time:=false`:

```bash
ros2 launch rover_bringup ekf.launch.py use_sim_time:=false
ros2 launch rover_bringup lidar_odom.launch.py use_sim_time:=false
ros2 launch rover_bringup nav2.launch.py use_sim_time:=false
```

Other useful launch files include:

```bash
# Start RViz with the robot model and joint-state GUI
ros2 launch rover_description display.launch.py

# Start SLAM Toolbox for a mapping session
ros2 launch rover_bringup slam.launch.py

# Run the beginner publisher/subscriber example
ros2 launch rover_basics hello.launch.py
```

`slam.launch.py` is a separate mapping workflow. Do not run it at the same time
as the EKF workflow because both systems attempt to publish the `map -> odom`
transform.

## Navigation details

Nav2 is configured specifically for an Ackermann-style rover rather than a
holonomic robot:

- Rolling-window local and global costmaps are used instead of a static map.
- The Smac Hybrid-A* planner uses Reeds-Shepp motion and permits reversing.
- The MPPI controller uses the Ackermann motion model.
- Nav2 consumes the local filtered estimate on `/odometry/local`.
- The command chain ends at `/cmd_vel`, which is bridged to the simulator.
- Obstacle and collision-monitor scans use `/scan_filtered`.

The main configuration files are:

| File | Purpose |
| --- | --- |
| `src/rover_bringup/config/nav2_params.yaml` | Nav2 planners, controller, costmaps, and tolerances |
| `src/rover_bringup/config/ekf.yaml` | Local/global EKF and GPS transform parameters |
| `src/rover_bringup/config/slam_mapping.yaml` | SLAM Toolbox mapping parameters |
| `src/rover_description/rviz/rover.rviz` | Saved RViz displays and tools |

Several navigation values are still simulation-tuned placeholders, including
the maximum steering angle, turning radius, speed limits, costmap inflation,
and some sensor covariances.

## Robot model and sensors

The model is defined in
`src/rover_description/urdf/rover.urdf.xacro`, with Gazebo plugins and sensor
configuration in `rover.gazebo.xacro`. Current modeled hardware includes:

- Ackermann steering with a wheelbase of approximately 0.686 m
- Wheel radius of approximately 0.159 m
- RPLIDAR-style laser scanner on a lift platform
- IMU
- GPS receiver
- Six simulated ultrasonic range sensors
- Joint states and wheel odometry

The measurements in the model are a mixture of measured values and placeholders.
Check the comments in the Xacro and `NOTES.md` before using the model for
hardware calibration.

## Development notes

`NOTES.md` contains the project's detailed engineering record, including:

- Frame conventions and measured dimensions
- Known ROS, Gazebo, and RViz environment issues
- Simulation milestones and tags
- Navigation tuning notes
- Open hardware and field-testing questions

The repository's `main` branch is intended to remain usable. Work is organized
on descriptive branches, and completed milestones are tagged.

## License

The ROS packages include Apache-2.0 license files. See the individual package
directories for the applicable license text.
