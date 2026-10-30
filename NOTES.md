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
- [x] Gazebo simulation (Ackermann drive, LIDAR, IMU, GPS)
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
- `$(find rover_description)` in xacro resolves to the INSTALLED copy. After editing any
  xacro, run `colcon build` BEFORE testing it with `xacro` or `gz sdf`, or the check reads
  stale files and misleads you.
- In the world file, every `<plugin>` must be a sibling. A plugin line pasted between another
  plugin's opening and closing tags is silently ignored (this disabled the IMU once). After
  editing, print the plugin block with `sed -n` and check the nesting by eye.
- The LIDAR scans a flat plane at `lidar_link` height. Anything at that height blocks it, and the
  sim LIDAR also appeared to detect the GPS box, which has a visual but no collision (guess:
  gpu_lidar renders the scene). The sim GPS was moved up (`gps_z` = chassis top + 0.15) to clear
  the scan plane. On the real rover, keep the GPS, IMU, and any mast out of the LIDAR's plane.
- RViz started by hand needs `--ros-args -p use_sim_time:=true` when the sim is running, or it
  shows nothing.



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

## Gazebo simulation (done, tag gazebo-sim-v1)
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
- Done: bridge node in `sim.launch.py` (`ros_gz_bridge parameter_bridge`) for `/clock`,
  `/cmd_vel` (ROS to Gazebo, `]`), `/odom`, `/tf` (Gazebo to ROS, `[`). Gazebo expands the
  plugin's relative topic names to `/cmd_vel`, `/odom`, `/tf`, `/clock` (checked with
  `gz topic -l`). Verified: `/clock` and `/odom` arrive in ROS.
- Sim odom covariance is all zeros. Revisit when configuring the EKF.
- `/joint_states` has no publisher yet (needs the Gazebo JointStatePublisher plugin + bridge).
- Verified: publishing `/cmd_vel` (linear x 0.3) drives the rover straight and `/odom` x increases.
  The sim plugin keeps the LAST command until a new one arrives (it only stopped when a zero
  command was sent). The real firmware needs a command-timeout watchdog that stops the motors.
- Verified: steering works (linear 0.3 + angular 0.3 drives a curve, front wheels turn,
  circle size looks right). Ackermann can't turn in place, so always send forward speed with turn rate.
- Done: `Sensors` system (ogre2) added to the world. `gpu_lidar` sensor on `lidar_link` in
  `rover.gazebo.xacro` publishes `/scan` (gz.msgs.LaserScan, 720 samples, 10 Hz, 0.05 to 30 m range,
  PLACEHOLDER values; get real RPLIDAR S2 specs from its datasheet). Gazebo merges `lidar_link` into
  `lift_platform` (fixed-joint lumping), so `gz_frame_id` is set to keep the frame name.
  `gz_frame_id` triggers a harmless "not defined in SDF" warning.
- Done: `/scan` bridged to ROS (`sensor_msgs/msg/LaserScan`, Gazebo to ROS). Verified: ~10 Hz and
  `frame_id: lidar_link`.
- Done: `JointStatePublisher` plugin in `rover.gazebo.xacro` publishes all joints, bridged to
  `/joint_states` (`sensor_msgs/msg/JointState[gz.msgs.Model`). Verified: TF `base_link` to
  `lidar_link` = (0.343, 0, 0.408) with the lift down. Without joint states the lift and wheel
  frames sit in a disconnected TF tree.
- Done: simulated IMU. `gz-sim-imu-system` in the world, `imu` sensor on `imu_link` in
  `rover.gazebo.xacro` (noise values are PLACEHOLDERS), bridged as `/imu`
  (`sensor_msgs/msg/Imu[gz.msgs.IMU`). Verified: ~100 Hz, `frame_id: imu_link`, z accel ~9.8.
- Done: simulated GPS. `gz-sim-navsat-system` plus `<spherical_coordinates>` (origin lat 45.0,
  lon -110.0, PLACEHOLDER; set to the real test site later, ENU orientation) in the world, and a
  `navsat` sensor on `gps_link` publishing `gps/fix`, bridged as `/gps/fix`
  (`sensor_msgs/msg/NavSatFix[gz.msgs.NavSat`). Verified: ~10 Hz, `frame_id: gps_link`.
- Known gaps: the bridge zeroes `position_covariance`, and the sim GPS has no noise (unrealistically
  perfect). Both odom and GPS covariances need handling in the EKF step.
- World is still a flat empty plane. Add obstacles/trees before testing navigation.
- Branch goal complete: drive, steering, LIDAR, IMU, GPS, joint states all verified in Gazebo.
- Done: obstacle world (tag `obstacle-world-v1`): `tree_1` to `tree_4` (cylinders, r 0.2 to 0.35 m,
  2 m tall), `rock_1` (0.5 m tall) and `boulder_1` (0.9 m tall). Verified: the LIDAR scan plane is at
  ~0.57 m (chassis top 0.537 + 0.03), so it misses `rock_1` but sees the boulder. Anything lower
  than ~0.57 m is a blind spot for the 2D LIDAR; the ultrasonics are meant to cover it.



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



## Ideas for later (not committed to)
- SLAM (slam_toolbox) as an optional branch after GPS navigation works. Not needed for the
  stated goals; a 2D LIDAR on rough forest terrain may map poorly. Keep the lift down.
