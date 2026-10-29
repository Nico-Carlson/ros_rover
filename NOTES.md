# Rover Project Notes

## Goal
Autonomous Ackermann rover for forest navigation (data collection for another
school's camera project). Modes: (1) navigate to a GPS pin avoiding obstacles,
(2) roam inside a boundary, (3) get unstuck. Mapping is secondary.

## Stack
- ROS 2 Jazzy + Gazebo Harmonic, Ubuntu 24.04 
- Hardware: Pi 5, 2x ESP32 (Arduino IDE, C++), RPLIDAR S2, GPS, IMU, 6 ultrasonics (removed for now)
- Pi code in Python, firmware in C++

## Environment quirks
- Login shell is fish, but ROS work happens in zsh (ROS has no fish setup script)
- zsh config is `~/.config/zsh/.zshrc` (ZDOTDIR), NOT `~/.zshrc`
- `QT_QPA_PLATFORM=xcb` is set there so Gazebo/RViz run through XWayland
- Hyprland custom settings go in `~/hakuspace-control/hyprland-custom.lua`
  (`xwayland.force_zero_scaling = true` keeps RViz sharp at 0.67 monitor scale)
- Do NOT set QT_SCALE_FACTOR (causes black flashing in RViz)

## Git conventions
- `main` always works; one descriptive branch per piece of work (no phase numbers)
- Commit after each working step; merge to main when a branch's goal is done

## Frame conventions
- `base_footprint`: on the ground under the rear axle center
- `base_link`: one wheel radius above it. X forward, Y left, Z up
- `lidar_link` rides on `lift_platform` (prismatic `lift_joint`, 0 to 1.2192 m)
- Rule: drive/navigate only with the lift fully down. The lift has limit
  switches only (no position feedback)

## Measurements (meters)
| Item | Value | Status |
|---|---|---|
| Wheelbase | 0.686 | measured |
| Track width | 0.559 | measured (overlaps chassis width, recheck) |
| Wheel radius / width | 0.15875 / 0.1016 | measured |
| Chassis L x W x H | 1.118 x 0.562 x 0.457 | measured |
| Total mass | ~30 kg | PLACEHOLDER |
| Ground clearance | 0.08 | PLACEHOLDER |
| Max steer angle | 0.7 rad | PLACEHOLDER |
| LIDAR / GPS / IMU positions | see xacro | PLACEHOLDER |

## Status
- [x] ROS 2, Gazebo, shell, git set up
- [x] ROS basics (rover_basics package: publisher, subscriber, launch file)
- [x] rover_description: chassis, 4 wheels, steering, sensors, lift, RViz launch
- [ ] Gazebo simulation (Ackermann drive, LIDAR, IMU, GPS)
- [ ] EKF + navsat_transform
- [ ] Nav2 (reuse old params; footprint must be updated, wheels make it wider than 0.56)
- [ ] Boundary roaming + stuck recovery
- [ ] ESP32 firmware + Pi hardware bridge
- [ ] Field testing


## How to work with me (instructions for any assistant)
- ONE step at a time. Never give a long multi-step list; I hit problems mid-way
  and a big list gets buried. Wait for my output before the next step.
- I type all code myself to learn it. Explain what each part does, briefly.
- I'm new to ROS and weak in terminal and C++. Python is fine. Firmware is C++.
- Editor is LazyVim (use `nvim`). Give commands for zsh unless I say otherwise.
- Branch names are descriptive (`robot-model`), never phase numbers.
- Tell me when to commit, push, branch, and merge.
- Say when something is a guess or can't be verified. Don't state it as fact.
- After merging any branch make a tag before deleting any branches.
- Keep this file current: as progress is made, record relevant context and
  instructions here. It is the project's "second brain" so a new model or a new
  maintainer never has to rebuild context from scratch.

## Workflow gotchas (these have bitten us)
- After editing any file in a package, run `colcon build --packages-select <pkg>`
  from `~/rover_ws` before launching. Launch uses the installed copy, not `src/`.
- After creating a NEW package, open a new terminal (or `source ~/.config/zsh/.zshrc`).
- A stale launch left running causes odd symptoms (old GUI, wheels stacked at
  the origin). Check with `ros2 node list` and `pkill -f` if needed.
- Xacro macros must be defined above the lines that call them. Properties can
  only use properties defined above them.
- Launch files need their folder installed (`data_files` in setup.py for Python
  packages, `install(DIRECTORY ...)` in CMakeLists.txt for rover_description).

## Mistakes already made (don't repeat)
- ROS 2 has NO fish setup script. Use zsh with `setup.zsh`.
- Edit `~/.config/zsh/.zshrc`, not `~/.zshrc` (ZDOTDIR points to the former).
- Hyprland here uses the Lua config format. Custom changes go in
  `~/hakuspace-control/hyprland-custom.lua`, never in `config.lua`.
- `QT_SCALE_FACTOR` caused black flashing in RViz. Don't use it.

## Repo layout
- Repo: github.com/Nico-Carlson/ros_rover (private), local folder `~/rover_ws`
- `src/rover_basics`: ament_python practice package (hello publisher/subscriber)
- `src/rover_description`: ament_cmake. `urdf/rover.urdf.xacro`, `launch/display.launch.py`,
  `rviz/rover.rviz`
- Measurements live as `xacro:property` values at the top of the xacro. Edit those,
  not the geometry.

## Gazebo simulation (branch `gazebo-sim`, in progress)
- Verified installed: Gazebo Sim 8.15.0, ros_gz (ros_gz_sim, ros_gz_bridge,
  ros_gz_image, ros_gz_interfaces), robot_localization.
- Plan: Gazebo tags live in a separate `urdf/rover.gazebo.xacro` in
  rover_description, included from `rover.urdf.xacro`. World and sim launch file
  to follow.
- Old `robot.gazebo` uses the old model's names. Translate when reusing:
  `wheel1/2_joint` -> `rear_right/left_wheel_joint`, `body_link` -> `base_link`,
  `wheel_separation` -> `track_width`, `wheelbase_length` -> `wheelbase`,
  `steering_limit` 0.785 -> `max_steer_angle` (0.7, placeholder).
- Plugin tag names (AckermannSteering, JointStatePublisher, Imu, Sensors, navsat)
  come from the old file and memory; verify against the Gazebo Harmonic docs.
- Done: `urdf/rover.gazebo.xacro` has wheel friction (mu1/mu2 1.0) and the
  AckermannSteering plugin. The xacro converts and validates
  (`xacro ... > /tmp/rover.urdf && gz sdf -k /tmp/rover.urdf`), but the plugin is
  NOT yet tested in a running sim.
- Done: `worlds/rover_world.sdf` has 1 ms physics, the Physics, UserCommands and
  SceneBroadcaster systems, a sun, and a 100 m x 100 m ground plane. It loads with
  `gz sim -r <installed path>`. `worlds` is installed in CMakeLists.txt.
- Everything in a world file must sit between `<world>` and `</world>`. The error
  "Root object can only contain one of model, light or actor" means something is outside.
- Still to add to the world: Sensors, Imu and NavSat systems, once those sensors exist
  in the xacro.
- Done: `launch/sim.launch.py` starts Gazebo with the world (`-r`), runs
  robot_state_publisher with `use_sim_time`, and spawns the rover with
  `ros_gz_sim create` (`-topic robot_description -z 0.3`). Spawn works. The
  "gazebo process has died, exit code -2" message on Ctrl+C is harmless.
- Next: bridge `/cmd_vel` into Gazebo and drive the rover.

## Old files (uploaded earlier, NOT trusted, reuse selectively)
- `ekf.yaml`: robot_localization EKF + navsat_transform, fuses /odom, /imu, GPS.
  Reuse as a starting point when we reach localization.
- `nav2_params.yaml` (and a duplicate `nav2_params_1.yaml`): Ackermann MPPI + Smac Hybrid,
  rolling-window costmaps, no static map. Known issues: footprint is 0.56 m wide but the
  real rover is about 0.66 m with wheels; `minimum_turning_radius` 0.8 depends on the
  real max steer angle; AMCL section is irrelevant without a map.
- `robot.xacro` / `robot.gazebo`: old model, replaced by rover_description. The Gazebo file
  is a useful reference for the Ackermann, IMU, LIDAR, and navsat plugins.
- NOT yet reviewed: `Rover_Documentation.docx`, `ackermann_bt.xml`,
  `ackermann_through_poses_bt.xml`.
- Wiring PDFs (unverified): master ESP drives a BTS7960 H-bridge for the drive motor, a
  steering servo, an RC receiver (7 channels), and the lift controller (relay, limit
  switches). Sensor ESP handles ultrasonics, gyro (I2C), battery voltage, and an override
  switch.
