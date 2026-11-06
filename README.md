# ROS Rover

ROS 2 software and simulation for an autonomous Ackermann-steered rover intended for forest navigation and outdoor data collection. The project models the rover in Gazebo, publishes its simulated sensors into ROS 2, estimates its position, and uses Nav2 to plan and follow paths around obstacles.

The long-term rover is intended to support:

- Navigation to a GPS location while avoiding obstacles
- Roaming within a defined boundary
- Recovery when the rover becomes stuck
- Camera-data collection for a separate school project

The repository currently focuses on the robot model, simulation, localization, SLAM, scan gating, simulated Nav2 navigation, and boundary roaming with an operator control panel. Real-hardware integration is planned but is not complete.

## Current status

Implemented and tested in simulation:

- Ackermann rover model with four wheels and steering
- Gazebo Harmonic simulation with an obstacle world
- RPLIDAR-style 2D laser scan
- Simulated IMU, GPS, wheel/joint states, and six ultrasonic sensors
- LIDAR odometry using `rf2o_laser_odometry`
- Dual `robot_localization` EKFs for local and global state estimates
- GPS covariance processing and GPS fusion in the global filter
- Scan gating so navigation uses the scan only when the simulated lift is down
- SLAM Toolbox mapping (separate session)
- Nav2 path planning and control using rolling costmaps
- RViz visualization and interactive goal selection
- Boundary drawing in RViz and roaming inside the boundary
- Goal watchdog that cancels goals that run too long
- Qt control panel: boundary and patrol buttons, plus live parameter sliders for roaming and Nav2

Not yet implemented:

- Stuck detection and recovery
- ESP32 firmware and the Raspberry Pi hardware bridge
- Real sensor drivers and field-tested calibration
- Final sensor measurements and navigation tuning
- A reliable real-world heading source for GPS fusion

The planned hardware path is to run the high-level ROS 2 stack on a Raspberry Pi and use two ESP32 microcontrollers for low-level rover control. The ESP32 firmware will be developed in the Arduino IDE and will handle motor and steering commands, sensor interfaces, lift limit switches, watchdog behavior, and communication with the Pi. The Pi-side bridge will translate between ROS 2 topics and the ESP32 protocol.

## System overview

```
Gazebo Harmonic
    |
    +-- ROS-Gazebo bridge --> /scan, /imu, /gps/fix, /joint_states
    |
    +-- scan_gate --> /scan_filtered --> rf2o_laser_odometry --> /odom_rf2o_cov
    |
    +-- robot_localization EKFs
            |
            +--> /odometry/local  --> Nav2
            +--> /odometry/global --> map-frame navigation

RViz 2D Goal Pose --------+
roam.py (boundary goals) -+--> Nav2 --> /cmd_vel --> Gazebo rover
control_panel.py --> services and parameters on roam, boundary_canvas, Nav2
```

The rover uses `base_footprint` as its ground-level navigation frame and `base_link` above it. The simulated LIDAR is mounted on a lift platform; the navigation stack is designed to operate with the lift fully lowered.

## Repository layout

```
src/
├── rover_basics/           # Beginner publisher/subscriber examples
├── rover_description/
│   ├── urdf/               # Rover model and Gazebo sensor plugins
│   ├── worlds/             # Gazebo world
│   ├── launch/             # Visualization and simulation launch files
│   └── rviz/               # Saved RViz configuration
└── rover_bringup/
    ├── config/             # EKF, SLAM, and Nav2 parameters
    ├── launch/             # Localization, SLAM, odometry, Nav2, full sim
    └── scripts/            # Helper nodes (see below)
```

Helper nodes in `rover_bringup/scripts/`:

| Script | Purpose |
|---|---|
| `scan_gate.py` | Passes LIDAR scans on only while the lift is down |
| `sim_lift_signal.py` | Simulation stand-in for the lift limit switch |
| `gps_covariance.py` | Adds covariance to the simulated GPS (simulation only) |
| `odom_covariance.py` | Adds covariance to rf2o odometry |
| `boundary_canvas.py` | Draws and saves a boundary polygon from RViz clicks |
| `roam.py` | Sends roaming goals inside the boundary; patrol services |
| `control_panel.py` | Qt panel for patrol control and live tuning |

The `rf2o_laser_odometry` package is an external dependency. It is intentionally ignored by this repository and must be cloned into the same workspace.

## Requirements

- Ubuntu 24.04
- ROS 2 Jazzy
- Gazebo Harmonic
- colcon
- Nav2, `robot_localization`, `slam_toolbox`
- `ros_gz_sim` and `ros_gz_bridge`
- `rf2o_laser_odometry`
- PyQt5 (control panel)
- `teleop_twist_keyboard` (optional manual driving)

## Build the workspace

```bash
mkdir -p ~/rover_ws/src
cd ~/rover_ws/src
git clone https://github.com/Nico-Carlson/ros_rover.git
git clone -b ros2 https://github.com/MAPIRlab/rf2o_laser_odometry.git

cd ~/rover_ws
source /opt/ros/jazzy/setup.bash
rosdep install --from-paths src --ignore-src -r -y
colcon build
source install/setup.bash
```

ROS launch files use the installed copy of each package, so rebuild after changing a package:

```bash
colcon build --packages-select rover_description rover_bringup
source install/setup.bash
```

## Run the simulation

One command starts Gazebo, scan gating, LIDAR odometry, both EKFs, Nav2, RViz, the boundary canvas, the roaming node, and the control panel, in sequence on timers:

```bash
source /opt/ros/jazzy/setup.bash
source ~/rover_ws/install/setup.bash
ros2 launch rover_bringup sim_nav.launch.py
```

Nav2 may take about 30 seconds after launch to become active.

**Point-to-point:** in RViz, use *2D Goal Pose*. The rover plans and drives to the goal.

**Boundary patrol:**

1. In the control panel, press **New Boundary**. This stops any patrol and clears the old shape.
2. In RViz, set the Fixed Frame to `map`, choose *Publish Point*, and click three or more corners. Click within 3 m of the first corner to close the shape. The boundary is saved to `config/boundary.yaml` and loaded by the roaming node.
3. Press **Start Patrol**. The rover roams inside the boundary, favoring less-visited areas. Press **Stop Patrol** to cancel the current goal and stop.

The panel's sliders (goal timeout, speed limits, goal tolerances, costmap inflation and cost falloff) change parameters on the running nodes. They are not saved: copy values you want to keep into `nav2_params.yaml`.

To drive manually when Nav2 has no active goal:

```bash
ros2 run teleop_twist_keyboard teleop_twist_keyboard
```

Stop the launch with Ctrl+C. Closing the Gazebo window alone does not stop all ROS and Gazebo processes.

## Run individual components

```bash
ros2 launch rover_description sim.launch.py      # Gazebo, model, bridge
ros2 launch rover_bringup lidar_odom.launch.py   # Scan gate and LIDAR odometry
ros2 launch rover_bringup ekf.launch.py          # Local and global EKFs
ros2 launch rover_bringup nav2.launch.py         # Nav2 (start last)
```

For hardware-oriented launches, these accept `use_sim_time:=false`:

```bash
ros2 launch rover_bringup ekf.launch.py use_sim_time:=false
ros2 launch rover_bringup lidar_odom.launch.py use_sim_time:=false
ros2 launch rover_bringup nav2.launch.py use_sim_time:=false
```

Other launch files:

```bash
ros2 launch rover_description display.launch.py   # RViz with the model and joint GUI
ros2 launch rover_bringup slam.launch.py          # SLAM Toolbox mapping session
ros2 launch rover_basics hello.launch.py          # Beginner example
```

`slam.launch.py` is a separate mapping workflow. Do not run it together with the EKF workflow, because both publish the `map -> odom` transform.

## Navigation details

Nav2 is configured for an Ackermann-style rover rather than a holonomic robot:

- Rolling-window local and global costmaps are used instead of a static map.
- The Smac Hybrid-A* planner uses Reeds-Shepp motion and permits reversing.
- The MPPI controller uses the Ackermann motion model.
- Nav2 consumes the local filtered estimate on `/odometry/local`.
- The command chain ends at `/cmd_vel`, which is bridged to the simulator.
- Obstacle and collision-monitor scans use `/scan_filtered`.

| File | Purpose |
|---|---|
| `src/rover_bringup/config/nav2_params.yaml` | Nav2 planners, controller, costmaps, tolerances |
| `src/rover_bringup/config/ekf.yaml` | Local/global EKF and GPS transform parameters |
| `src/rover_bringup/config/slam_mapping.yaml` | SLAM Toolbox parameters |
| `src/rover_description/rviz/rover.rviz` | Saved RViz displays and tools |

Several navigation values are still simulation-tuned placeholders, including the maximum steering angle, turning radius, speed limits, costmap inflation, and some sensor covariances.

## Robot model and sensors

The model is defined in `src/rover_description/urdf/rover.urdf.xacro`, with Gazebo plugins and sensors in `rover.gazebo.xacro`. Modeled hardware:

- Ackermann steering, wheelbase about 0.686 m
- Wheel radius about 0.159 m
- RPLIDAR-style laser scanner on a lift platform
- IMU and GPS receiver
- Six simulated ultrasonic range sensors
- Joint states and wheel odometry (simulation only; the real rover has no wheel encoders)

Dimensions are a mixture of measured values and placeholders. See `NOTES.md` for which is which.

## Planned physical rover integration

```
Raspberry Pi running ROS 2
    |
    +-- ROS 2 hardware bridge
            |
            +-- Serial or network link
                    |
                    +-- ESP32 motor and steering controller
                    +-- ESP32 sensor and lift controller
```

Planned steps:

1. Measure the physical rover and replace placeholder dimensions, masses, steering limits, sensor positions, and covariances.
2. Write Arduino IDE firmware for both ESP32 boards, including motor and steering control, sensor acquisition, lift limit-switch handling, and a command-timeout watchdog.
3. Implement the Raspberry Pi bridge between ROS 2 topics and the ESP32 boards.
4. Replace simulated topics and Gazebo bridges with real sensor drivers while keeping the ROS 2 interfaces used by localization and Nav2.
5. Add stuck detection and recovery.
6. Test at low speed, tune covariances and turning-radius limits with recorded field data, and add physical emergency-stop behavior.

The real rover is expected to use a Raspberry Pi 5, two ESP32 boards, an RPLIDAR S2, GPS, IMU, and ultrasonic sensors. Exact sensor models and several measurements are still open.

## Project documentation

- `NOTES.md`: current state, how to run things, gotchas, and placeholder values.
- `DEVLOG.md`: full engineering record with every step, test result, and decision.

The `main` branch is intended to remain usable. Work is organized on descriptive branches, and completed milestones are tagged.

## License

The ROS packages include Apache-2.0 license files. See the individual package directories for the license text.
