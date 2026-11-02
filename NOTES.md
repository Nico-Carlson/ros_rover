# Rover Project Notes

## Goal
Autonomous Ackermann rover for forest navigation (data collection for another
school's camera project). Modes: (1) navigate to a GPS pin avoiding obstacles,
(2) roam inside a boundary, (3) get unstuck. Mapping is secondary.

## Stack
- ROS 2 Jazzy + Gazebo Harmonic, Ubuntu 24.04 in a distrobox on Arch (Hyprland)
- Hardware: Pi 5, 2x ESP32 (Arduino IDE, C++), RPLIDAR S2, GPS, IMU (MPU6050 per old
  comments, unverified), 6 ultrasonics (removed from the real rover for now; simulated)
- Pi code in Python, firmware in C++

## Environment quirks
- Login shell is fish, but ROS work happens in zsh (ROS has no fish setup script)
- zsh config is `~/.config/zsh/.zshrc` (ZDOTDIR), NOT `~/.zshrc`
- `QT_QPA_PLATFORM=xcb` is set there so Gazebo/RViz run through XWayland
- Hyprland custom settings go in `~/hakuspace-control/hyprland-custom.lua`
  (`xwayland.force_zero_scaling = true` keeps RViz sharp at 0.67 monitor scale)
- Do NOT set QT_SCALE_FACTOR (causes black flashing in RViz)

## Quick start (sim)
1. `ros2 launch rover_description sim.launch.py` (wait ~10 s)
2. `ros2 launch rover_bringup lidar_odom.launch.py` (rf2o LIDAR odometry; start it BEFORE the EKF)
3. `ros2 launch rover_bringup ekf.launch.py` (SLAM is a separate mapping mode, not run with this: see "SLAM")
4. `ros2 launch rover_bringup nav2.launch.py` (start it LAST: the costmaps need `odom -> base_footprint` to exist first, or the first attempt times out and you relaunch; takes ~30 s to come up)
5. RViz: `ros2 run rviz2 rviz2 -d ~/rover_ws/src/rover_description/rviz/rover.rviz --ros-args -p use_sim_time:=true` (Fixed Frame `odom`; add Map on `/local_costmap/costmap`, Path on `/plan`; send goals with "2D Goal Pose")
6. Drive by hand: `ros2 run teleop_twist_keyboard teleop_twist_keyboard` (not while Nav2 has a goal)

- Stop everything with Ctrl+C in each launch terminal. Closing the Gazebo window does NOT stop the sim.
- Start fresh (stop everything first) before any test where positions are compared: the EKFs start at (0, 0).
- Scan gating is wired into `lidar_odom.launch.py`: rf2o reads `/scan_filtered`, and `sim_lift_signal.py` stands in for the lift switch in the sim.
- both launches take use_sim_time:=false for hardware (default true)

## Git conventions
- `main` always works; one descriptive branch per piece of work (no phase numbers)
- Commit after each working step; merge to main (`--no-ff`) when a branch's goal is done
- Tag each finished milestone before deleting any branch. Tags so far: `robot-model-v1`,
  `gazebo-sim-v1`, `obstacle-world-v1`, `ultrasonics-v1`, `slam-mapping-v1`, `ekf-localization-v1`, `gps-covariance-v1`,  `hardware-matched-ekf-v1`, `scan-gating-v1` 
- Finished branches still exist (all merged). Deleting them is optional; the tags preserve the work.

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
| Ultrasonic mounting height | 3 in (0.0762 m) above the top of the wheels | measured |
| Total mass | ~30 kg | PLACEHOLDER |
| Ground clearance | 0.08 | PLACEHOLDER |
| Max steer angle | 0.7 rad | PLACEHOLDER |
| Lift platform mass / size / speed | 5 kg / 0.4 x 0.4 m / 0.05 m/s | PLACEHOLDER |
| Ultrasonic positions, angles, cone | see xacro | PLACEHOLDER |
| LIDAR / GPS / IMU positions | see xacro (GPS flat on the chassis) | PLACEHOLDER |

## Status
- [x] ROS 2, Gazebo, shell, git set up
- [x] ROS basics (rover_basics package: publisher, subscriber, launch file)
- [x] rover_description: chassis, 4 wheels, steering, sensors, lift, RViz launch
- [x] Gazebo simulation (Ackermann drive, LIDAR, IMU, GPS, joint states)
- [x] Obstacle world and simulated ultrasonics
- [x] SLAM in the sim (slam_toolbox online_async)
- [x] EKF (wheel odometry + IMU yaw rate)
- [x] GPS fusion wiring in the sim (dual EKF, corrects blocked-wheel error)
- [x] Realistic sim GPS noise and covariance, datum, and IMU yaw in the map EKF (GPS fusion retested)
- [x] Hardware-matched EKFs: wheel odometry dropped, rf2o LIDAR odometry fused (retested against the stump)
- [ ] Realistic sim noise for the IMU, and tune the covariances (GPS, rf2o speed, IMU) against real data
- [ ] Heading source for GPS fusion on real hardware (real IMU likely has no compass)
- [x] Scan-gating node (lift down only)
- [ ] Nav2 (reuse old params; footprint must be updated, wheels make it wider than 0.56)
- [ ] Boundary roaming + stuck recovery
- [ ] Replace placeholder measurements and sensor specs with real ones
- [ ] ESP32 firmware (with watchdog) + Pi hardware bridge
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
- Closing the Gazebo window does NOT stop the sim: the server, bridge, and robot_state_publisher keep
  running and the rover keeps its position on relaunch. Stop with Ctrl+C in the launch terminal; if stuck:
  `pkill -9 -f "gz sim"; pkill -9 -f parameter_bridge; pkill -9 -f robot_state_publisher; pkill -9 -f ekf_node`.
  (`pgrep` needs `-f` for names longer than 15 characters.)
- Xacro macros must be defined above the lines that call them. Properties can
  only use properties defined above them.
- Launch files need their folder installed (`data_files` in setup.py for Python
  packages, `install(DIRECTORY ...)` in CMakeLists.txt for the CMake packages).
- `$(find rover_description)` in xacro resolves to the INSTALLED copy. After editing any
  xacro, run `colcon build` BEFORE testing it with `xacro` or `gz sdf`, or the check reads
  stale files and misleads you.
- A block pasted outside `</robot>` or `</world>` is silently ignored. After editing, check with
  `grep -n` that the new block's line number is smaller than the closing tag's.
- In the world file, every `<plugin>` must be a sibling. A plugin line pasted between another
  plugin's opening and closing tags is silently ignored (this disabled the IMU once). After
  editing, print the plugin block with `sed -n` and check the nesting by eye.
- The LIDAR scans a flat plane at `lidar_link` height. Anything at that height blocks it, and the
  sim LIDAR also appeared to detect the GPS box, which has a visual but no collision (guess: gpu_lidar
  renders the scene). The sim GPS now sits flat on the chassis (`gps_z` = chassis top + 0.01), 1 cm under
  the scan plane. On the real rover, keep the GPS, IMU, and any mast out of the LIDAR's plane.
- RViz started by hand needs `--ros-args -p use_sim_time:=true` when the sim is running, or it
  shows nothing.
- The EKF's pose starts at (0, 0, 0) whenever it launches, so comparisons against Gazebo or `/odom` are only
  meaningful when the sim and EKF were both started fresh before the rover moved.

## Mistakes already made (don't repeat)
- ROS 2 has NO fish setup script. Use zsh with `setup.zsh`.
- Edit `~/.config/zsh/.zshrc`, not `~/.zshrc` (ZDOTDIR points to the former).
- Hyprland here uses the Lua config format. Custom changes go in
  `~/hakuspace-control/hyprland-custom.lua`, never in `config.lua`.
- `QT_SCALE_FACTOR` caused black flashing in RViz. Don't use it.

## Repo layout
- Repo: github.com/Nico-Carlson/ros_rover (private), local folder `~/rover_ws`
- `src/rover_basics`: ament_python practice package (hello publisher/subscriber)
- `src/rover_description`: ament_cmake. `urdf/rover.urdf.xacro` (model), `urdf/rover.gazebo.xacro`
  (Gazebo plugins and sensors), `worlds/rover_world.sdf`, `launch/display.launch.py` (RViz view),
  `launch/sim.launch.py` (Gazebo + spawn + bridge), `rviz/rover.rviz`
- `src/rover_bringup`: ament_cmake. `config/slam_mapping.yaml`, `config/ekf.yaml`,
  `launch/slam.launch.py`, `launch/ekf.launch.py`
- Measurements live as `xacro:property` values at the top of the xacro. Edit those,
  not the geometry.

## Gazebo simulation (done, tag gazebo-sim-v1)
- Verified installed: Gazebo Sim 8.15.0, ros_gz (ros_gz_sim, ros_gz_bridge,
  ros_gz_image, ros_gz_interfaces), robot_localization.
- Gazebo tags live in `urdf/rover.gazebo.xacro`, included from `rover.urdf.xacro`.
- Old `robot.gazebo` uses the old model's names. Translate when reusing:
  `wheel1/2_joint` -> `rear_right/left_wheel_joint`, `body_link` -> `base_link`,
  `wheel_separation` -> `track_width`, `wheelbase_length` -> `wheelbase`,
  `steering_limit` 0.785 -> `max_steer_angle` (0.7, placeholder).
- Plugin tags: AckermannSteering, JointStatePublisher, and NavSat were checked against the Gazebo docs;
  Imu and Sensors are confirmed by working in the sim.
- `worlds/rover_world.sdf`: 1 ms physics; Physics, UserCommands, SceneBroadcaster, Sensors (ogre2), Imu and
  NavSat systems; `<spherical_coordinates>` origin lat 45.0, lon -110.0 (PLACEHOLDER, set to the real test
  site later; ENU); a sun; a 100 m x 100 m ground plane; obstacles (below). Everything must sit between
  `<world>` and `</world>`; the error "Root object can only contain one of model, light or actor" means
  something is outside.
- `launch/sim.launch.py`: starts Gazebo with the world (`-r`), runs robot_state_publisher with
  `use_sim_time`, spawns the rover with `ros_gz_sim create` (`-topic robot_description -z 0.3`), and runs
  the bridge. The "gazebo process has died, exit code -2" message on Ctrl+C is harmless.
- Bridge (`ros_gz_bridge parameter_bridge`): `/clock`, `/odom`, `/scan`, `/joint_states`, `/imu`, `/gps/fix`
  and the six `/us/*` topics go Gazebo to ROS (`[`); `/cmd_vel` goes ROS to Gazebo (`]`). The `/tf` line is
  commented out so the EKF owns `odom -> base_footprint` (first suspect if transforms break). Gazebo expands
  the plugin's relative topic names to top-level topics (`/cmd_vel`, `/odom`, `/scan`, ...); check with
  `gz topic -l`.
- Verified: publishing `/cmd_vel` (linear x 0.3) drives the rover straight and `/odom` x increases. The sim
  plugin keeps the LAST command until a new one arrives (it only stopped when a zero command was sent). The
  real firmware needs a command-timeout watchdog that stops the motors.
- Verified: steering works (linear 0.3 + angular 0.3 drives a curve, front wheels turn, circle size looks
  right). Ackermann can't turn in place, so always send forward speed with turn rate.
- LIDAR: `Sensors` system (ogre2) in the world, `gpu_lidar` on `lidar_link`, publishes `/scan`
  (gz.msgs.LaserScan). Follows the RPLIDAR S2 spec: 3200 samples (0.1125 deg), 10 Hz, range 0.05 to 10 m
  (the S2's 30 m is for bright targets only; dark forest targets give ~10 m). Verified: ~10 Hz and
  `frame_id: lidar_link`. The S2's +/-1.5 deg scan-plane flatness and +/-30 mm accuracy are not modeled.
  Gazebo merges `lidar_link` into `lift_platform` (fixed-joint lumping), so `gz_frame_id` is set to keep the
  frame name. `gz_frame_id` triggers a harmless "not defined in SDF" warning.
- Joint states: `JointStatePublisher` plugin publishes all joints, bridged to `/joint_states`. Verified: TF
  `base_link` to `lidar_link` = (0.343, 0, 0.408) with the lift down. Without joint states the lift and wheel
  frames sit in a disconnected TF tree.
- IMU: `imu` sensor on `imu_link` (noise values are PLACEHOLDERS), bridged as `/imu`. Verified: ~100 Hz,
  `frame_id: imu_link`, z accel ~9.8.
- GPS: `navsat` sensor on `gps_link` publishing `gps/fix`, bridged as `/gps/fix`. Verified: ~10 Hz,
  `frame_id: gps_link`.
- Known gaps: the bridge zeroes `position_covariance`, the sim GPS has no noise (unrealistically perfect),
  and sim odom covariance is all zeros. All three need handling in the EKF work.

## Obstacle world (done, tag obstacle-world-v1)
- `tree_1` (5, 0), `tree_2` (8, -3), `tree_3` (8, 3), `tree_4` (12, 0.5): cylinders, r 0.2 to 0.35 m, 2 m tall.
  `rock_1` (3, 3): 0.5 m tall. `boulder_1` (3, -3): 0.9 m tall. `stump_1` (2.5, 0): 0.4 m square, 0.5 m tall,
  directly ahead of the spawn point.
- Verified: the LIDAR scan plane is at ~0.57 m (chassis top 0.537 + 0.03), so it sees the boulder but misses
  `rock_1` and `stump_1` (those only appear if the rover is pushed into the ground in Gazebo). Anything lower
  than ~0.57 m is a blind spot for the 2D LIDAR; the ultrasonics are meant to cover it.

## Ultrasonics (done, tag ultrasonics-v1)
- Six frames `us_front_left/mid/right` and `us_rear_left/mid/right` on `base_link` in `rover.urdf.xacro`.
  Mounted 3 in (0.0762 m) above the top of the wheels, 0.15 m left/right of center, outer sensors angled
  0.26 rad outward, rear set faces backward (yaw pi). All positions are PLACEHOLDERS except the height.
- Simulated as 5-ray `gpu_lidar` fans (cone +/-0.131 rad, range 0.03 to 4 m, 10 Hz, all PLACEHOLDERS) from a
  macro in `rover.gazebo.xacro`, bridged as `sensor_msgs/msg/LaserScan` on `/us/front_left|mid|right` and
  `/us/rear_left|mid|right`.
- Verified: `/us/front_mid` center ray reads 3.84 m to `tree_1` and ~1.39 to 1.40 m (all five rays) to
  `stump_1`, matching hand calculations, while the LIDAR does not see the stump.
- Sim vs real: a real ultrasonic reports ONE distance for its whole cone (the sim gives five), and real ones
  struggle with angled or soft surfaces like foliage. TODO: convert to `sensor_msgs/msg/Range` (what the
  ESP32 firmware will publish) using the nearest of the five rays, when we build the hardware bridge.
- Design consequence: the ultrasonics only cover the front and rear faces at short range. Nav2 will need a
  range-sensor obstacle layer fed by them (to be designed when we reach navigation).

## SLAM (done, tag slam-mapping-v1)
- `rover_bringup/config/slam_mapping.yaml` is a copy of the stock slam_toolbox
  `mapper_params_online_async.yaml`. Only change so far: `max_laser_range` 20 -> 10 (S2 reaches ~10 m on
  dark targets). Frames already matched: odom, map, base_footprint, `/scan`.
- `rover_bringup/launch/slam.launch.py` wraps the stock online_async launch with our params and
  `use_sim_time:=true` (hard-coded; make it a launch argument for the real rover).
- Verified in sim: the three trees and the boulder map in the right places and stay fixed while driving.
  RViz: Fixed Frame `map`, Map display on `/map` with Durability `Transient Local`.
- Caveat: sim odometry is nearly perfect (no slip), so this is the easy case. Pi CPU load with SLAM is
  unmeasured (the real S2 gives 3200 points per scan).
- Open design decision: SLAM and the GPS EKF both want to own `map -> odom`. Plan: SLAM is a separate
  mapping mode, not run together with GPS navigation.

## Localization (done in the sim, tag ekf-localization-v1)
- `rover_bringup/config/ekf.yaml`: single EKF (`ekf_filter_node`, frame `odom`) fusing `/odom` forward
  speed + yaw rate and `/imu` yaw rate only. GPS not added yet. `use_sim_time` is passed from
  `launch/ekf.launch.py`, not the YAML. Output: `/odometry/filtered` (~29 Hz).
- Verified (rover stationary): `/tf` publishers are only `ekf_filter_node` and `robot_state_publisher`;
  yaw wanders ~0.03 to 0.05 deg from integrating simulated IMU noise.
- Verified (one short drive from a clean start): vs Gazebo's true pose, `/odom` was off ~12 cm / +3.5 deg
  and `/odometry/filtered` ~1 cm / +0.6 deg. The sim odometry seems to overestimate turning (IMU yaw
  rate reads ~8% below it), and the EKF follows the IMU. Single run, mild sim noise: not an accuracy claim.
- Verified: the sim IMU orientation follows the rover (ENU, yaw 0 = facing east). After a left turn the IMU
  read ~99 deg and `/odom` ~107 deg (~7% less turning on the IMU, matching the 8% yaw-rate gap). Likely the
  odometry plugin overestimates; consistent with the ground-truth test. Orientation covariance is all zeros.
- Sim vs real: the sim IMU acts as a perfect compass. The real IMU (MPU6050?) probably has no heading, so
  GPS fusion needs another heading source on hardware.
- `navsat_transform` added (in `ekf.yaml` and `ekf.launch.py`). Verified: `/odometry/gps` matched Gazebo's true
  position to under 1 mm, including after a turn and a reverse, so the ENU alignment is correct (yaw_offset 0
  works with the sim IMU).
- Verified: when the rover is pressed against an obstacle (the stump), wheel odometry overcounts: true x 0.312,
  `/odom` 0.985, `/odometry/filtered` 0.947. GPS stayed correct. An earlier unexplained 1 m error was almost
  certainly the same thing (rover against the stump). Idea for the unstick behavior: a gap between wheel
  odometry speed and GPS-derived speed means the wheels are slipping or blocked.
- Done: dual EKF (Nav2 pattern, checked against its Jazzy example; these nodes are not lifecycle nodes on
  Jazzy). `ekf_filter_node` (odom frame, remapped to `/odometry/local`) fuses `/odom` + `/imu` and owns
  `odom -> base_footprint`. `ekf_filter_node_map` (map frame, `/odometry/global`) also fuses `/odometry/gps`
  x/y and owns `map -> odom`. `navsat_transform` reads `odometry/global`. `/odometry/filtered` no longer exists.
  Verified at rest: three nodes, three topics, `map -> base_footprint` ~zero.
- GOTCHA: pasting a new YAML block with the editor can replace the file instead of appending. After editing,
  check the keys: `python3 -c "import yaml; print(list(yaml.safe_load(open('src/rover_bringup/config/ekf.yaml')).keys()))"`
  should print all three node names. A node name that isn't a YAML key silently runs with defaults.
- Verified (rover held against the stump, wheels spinning): truth x 1.398, `/odom` 2.730 and `/odometry/local`
  2.726 (+1.33 m wheel overcount), `/odometry/global` and `/odometry/gps` 1.3975. GPS fusion corrects the slip.
- CAVEAT: global equals GPS to ~1e-10, so the map filter is copying GPS, not blending. Likely cause: the
  bridge zeroes GPS covariance (and odom covariance is also zero), so GPS is treated as perfect. Fine for a
  perfect sim GPS, but a real GPS (meters of error, worse under trees) will jitter. TODO: realistic sim GPS
  noise and covariance, plus odometry covariance. Real GPS module and its accuracy are unknown.
- One `Failed to meet update rate! Took 0.152 s` error from the odom filter (sim load on integrated graphics?
  unverified). Watch whether it recurs; it matters for the Pi.


## GPS noise and covariance (done in the sim, branch gps-covariance)
- Sim GPS noise (PLACEHOLDERS): navsat `position_sensing` horizontal gaussian stddev 0.0000135, vertical
  3.0. It seems to be applied in degrees (~1.5 m north-south, ~1.06 m east-west at 45 deg latitude); that is
  inferred from sample scatter, not confirmed in the docs.
- The bridge zeroes `position_covariance`. New node `rover_bringup/scripts/gps_covariance.py` (installed
  with `install(PROGRAMS ...)` in CMakeLists.txt, needs `chmod +x`) subscribes `/gps/fix`, fills a diagonal
  covariance (parameters `horizontal_variance` 2.25, `vertical_variance` 9.0, in m^2) and publishes
  `/gps/fix_cov`. `ekf.launch.py` remaps navsat_transform's `gps/fix` to `gps/fix_cov`. SIM ONLY: a real GPS
  driver publishes its own covariance, so drop the node and the remap on hardware.
- Verified (stationary, 15 samples, std devs computed by hand): `/odometry/gps` x ~1.1 m, `/odometry/global`
  x ~0.28 m. Before this, global copied GPS to ~1e-10.
- Datum: without one, navsat_transform used the first (noisy) fix as its origin, leaving a fixed ~2.3 m
  offset. Now `wait_for_datum: true` and `datum: [45.0, -110.0, 0.0]` in `ekf.yaml` (matches the world's
  `<spherical_coordinates>`; change both together when the real test site is chosen). A one-time startup
  ERROR "Could not obtain map -> base_footprint transform" is harmless (it did not repeat).
- Yaw: the map EKF had no absolute heading, so with noisy GPS its yaw wandered (-90 to -51 deg) while the
  rover sat still (cause is a guess; the fix below resolved it). Fix: the map EKF's `imu0_config` also fuses
  absolute yaw (6th value, first row). Yaw now reads 0.000. SIM ONLY: the real IMU probably has no compass,
  so hardware needs another heading source.
- Verified (stationary): `map -> base_footprint` yaw 0.000 on every sample, translation within ~1 m of origin.


## Old files (uploaded earlier, NOT trusted, reuse selectively)
- `ekf.yaml`: dual-EKF design (`ekf_filter_node` in odom, `ekf_filter_node_map` in map fusing
  `/odometry/gps` from navsat_transform). Reuse the map-frame idea when adding GPS.
- `nav2_params.yaml` (and a duplicate `nav2_params_1.yaml`): Ackermann MPPI + Smac Hybrid,
  rolling-window costmaps, no static map. Known issues: footprint is 0.56 m wide but the
  real rover is about 0.66 m with wheels; `minimum_turning_radius` 0.8 depends on the
  real max steer angle; AMCL section is irrelevant without a map.
- `robot.xacro` / `robot.gazebo`: old model, replaced by rover_description. The Gazebo file
  is a useful reference for the Ackermann, IMU, LIDAR, and navsat plugins.
- `Rover_Documentation.docx` (reviewed): RC channels (1 steering, 2 throttle & brake, 6 lift, 7 reverse
  thumb switch), partly blank ESP32 pin tables (limit switches, brake, reverse, throttle in/out, gyro on
  I2C pins 21/22), and a feature list: tilt and collision alarms (caution/danger), Bluetooth telemetry, lift
  interlock (alarms off, brake set, no throttle), auto-brake when throttle is off, brake releases before
  forward, override switch, stop on loss of radio contact. No part numbers and NO wheel encoders mentioned.
  The firmware must map `/cmd_vel` to throttle/steering/brake/reverse. Not yet reviewed:
  `ackermann_bt.xml`, `ackermann_through_poses_bt.xml`.
  - Wiring PDFs (unverified): master ESP drives a BTS7960 H-bridge for the drive motor, a
  steering servo, an RC receiver (7 channels), and the lift controller (relay, limit
  switches). Sensor ESP handles ultrasonics, gyro (I2C), battery voltage, and an override
  switch.

## No wheel encoders on the real rover (stated by me, replaces the odometry plan)
- The real rover has NO wheel encoders, so no real `/odom` forward speed. The sim's wheel odometry is a
  sim-only source. Everything that fuses or compares `/odom` (both EKFs, the wheel-vs-GPS stuck idea) needs a
  hardware plan.
- DECIDED: GPS + IMU + LIDAR odometry (rf2o) replace wheel odometry in both EKFs; see the "Hardware-matched
  EKF" section. GPS velocity, commanded speed, and adding hall sensors were not pursued. Hall sensors or
  encoders remain an option if rf2o proves weak in a real forest.
- Still open for hardware: a real heading source (the sim IMU is a compass, the real one probably is not), and
  whether rf2o holds up on rough, cluttered terrain.

## Hardware-matched EKF and LIDAR odometry (done in the sim, branch hardware-matched-ekf)
- Neither EKF fuses the sim's `/odom` any more (the real rover has no encoders). Odom filter: IMU absolute yaw
  + yaw rate (sim only, see GPS section) and `/odom_rf2o_cov` forward speed + yaw rate. Map filter: GPS x/y as
  `odom0`, IMU yaw + yaw rate, and `/odom_rf2o_cov` speed + yaw rate as `odom1`.
- GOTCHA: a sensor numbered `odom1` with no `odom0` is ignored (the map filter read exactly 0.0 until the GPS
  entry was renamed `odom0`). Number sensors from 0.
- rf2o (LIDAR odometry, GPL v3): cloned into `src/rf2o_laser_odometry`, which is git-ignored, NOT in this
  repo. Fetch: `cd ~/rover_ws/src && git clone -b ros2 https://github.com/MAPIRlab/rf2o_laser_odometry.git`,
  then `colcon build --packages-select rf2o_laser_odometry` (~20 s on Jazzy). The port's own launch file
  defaults to `publish_tf: True` and `base_link`, which would fight the EKF; use ours,
  `rover_bringup/launch/lidar_odom.launch.py` (`publish_tf` False, `base_frame_id` base_footprint, 10 Hz to
  match the scan, `use_sim_time` hard-coded; make it a launch argument for hardware). Publishes `/odom_rf2o`.
- rf2o publishes all-zero covariances. `rover_bringup/scripts/odom_covariance.py` (installed in
  CMakeLists.txt with `gps_covariance.py`) fills the twist covariance (`speed_variance`, `yaw_rate_variance`,
  both 0.01, PLACEHOLDER guesses, not measured) and republishes `/odom_rf2o_cov`.
- Start order (fresh start, three terminals): `sim.launch.py`, then `lidar_odom.launch.py`, then
  `ekf.launch.py`.
- Verified (single runs, sim): driving in reverse 1.56 m, rf2o -1.579 vs sim wheels -1.558. Rover held
  against the stump: wheels read 3.567 (overcount), rf2o 1.364 (earlier true x there was 1.398; assumes the
  same stopping spot). With the full stack, `/odometry/local` read 1.379 and `/odometry/global` over 5
  samples averaged ~1.31 m with ~0.12 m std dev (computed by hand), vs ~1.1 m std dev for raw GPS at rest.
- Caveats: rf2o occasionally skips a scan (0.2 s gaps, likely sim load; unconfirmed). It needs the lift down
  and structure in range; the sim world is flat with a few trees, so forest performance is unknown. Pi CPU
  cost with the real S2 (3200 points per scan) is unmeasured. The "Latitude 45d more than 20d from N pole"
  ERROR from navsat_transform seems harmless (positions stay sane; unconfirmed).
- Stuck-detection idea changes: wheel speed does not exist on hardware, so compare commanded speed with
  rf2o speed instead.

## Scan gating (done in the sim, branch scan-gating)
- Goal: LIDAR consumers (rf2o now; SLAM and Nav2 later) must only see scans taken with the lift fully down.
- Design: one signal, `/lift_down` (`std_msgs/Bool`, true = fully down). Sim: derived from `lift_joint` in
  `/joint_states`. Hardware: the ESP32 publishes it from the limit switch. The gate does not care which.
- The sim lift CANNOT be moved: `lift_joint` is a plain prismatic joint with limits, with no controller or
  command topic. It always reads ~0 (down). So the gate is tested by hand-publishing `/lift_down`. Making the
  sim lift controllable is a possible later task (Gazebo joint controller + bridged command topic).
- `rover_bringup/scripts/scan_gate.py` (written, installed, tested): subscribes `/scan` (best-effort
  `qos_profile_sensor_data`, so a real LIDAR driver connects too) and `/lift_down`, republishes
  `/scan_filtered` (reliable). FAIL-SAFE: blocks scans if no `/lift_down` ever arrived, if it is false, or if
  the last one is older than `lift_timeout` (1.0 s, PLACEHOLDER).
- `rover_bringup/scripts/sim_lift_signal.py`: SIM ONLY. Publishes `/lift_down` = (`lift_joint` position <
  `down_threshold` 0.01 m, PLACEHOLDER) on every joint-state message. Verified: it publishes true in the sim
  (the lift never moves there), so `/scan_filtered` flows.
- Both scripts are listed in the `install(PROGRAMS ...)` block in `CMakeLists.txt` (every new script must be
  added there and `chmod +x`'d).
- Verified by hand (sim running, `ros2 topic pub -r 5 /lift_down std_msgs/msg/Bool "{data: ...}"`):
  no signal -> `/scan_filtered` silent; `true` -> ~9.9 Hz; `false` -> silent; publisher stopped -> scans
  stopped within about a second.
- Wired in: `launch/lidar_odom.launch.py` now starts `scan_gate`, `sim_lift_signal`, rf2o and
  `odom_covariance`; rf2o's `laser_scan_topic` is `/scan_filtered`. Start order is unchanged (sim, then
  `lidar_odom.launch.py`, then `ekf.launch.py`).
- Verified (fresh start, full stack): `/scan_filtered` ~8.9 Hz vs raw `/scan` ~8.6 Hz, so the gate loses no scans;
  both are below 10 Hz probably because the sim runs under real time (inferred, real-time factor not checked).
  A short reverse drive gave `/odom_rf2o` (-0.982, 0.467) vs sim wheels (-0.949, 0.508), about 5 cm apart.
  Agreement only, not a ground-truth check.
- Harmless oddities: `ros2 node list` shows `/rf2o_laser_odometry` twice although `pgrep` finds one process
  (probably an internal helper node in the port; a guess). One `Failed to meet update rate! Took 0.165 s`
  from the map EKF at startup, not repeated during the drive (second sighting of this error, see Localization).
- Still to do: SLAM should read `/scan_filtered` (change `scan_topic` in `slam_mapping.yaml`) when next used.
  On hardware, leave `sim_lift_signal.py` out of the launch; the ESP32 publishes `/lift_down`.

## Nav2 (in progress, branch nav2-params, NOT merged, NOT tagged)
- `config/nav2_params.yaml`: copied from the old project, then: footprint [0.902, +-0.33] to [-0.216, +-0.33]
  (chassis centered 0.343 m ahead of base_footprint; 0.33 half-width includes wheels, track width unverified),
  scan topics -> `/scan_filtered`, odom topics -> `/odometry/local`, `consider_footprint: true` in CostCritic,
  dropped amcl, route_server, docking_server and loopback_simulator blocks.
- `launch/nav2.launch.py`: OWN launch, not the stock `navigation_launch.py`, because the stock one hard-codes
  `route_server` and `docking_server` (docking fails without a dock plugin and aborts the whole bringup). Starts
  eight nodes plus a lifecycle manager; `use_sim_time` is an argument (default true). Start order: sim,
  `lidar_odom`, `ekf`, then `nav2` LAST (the costmaps need `odom -> base_footprint` to exist first, or the
  first attempt times out and you relaunch).
- Verified in the sim: all nodes active; a goal in RViz plans and the rover drives to it.
- Observed and TODO: wavy turns near the goal and a small "v" at the end; `yaw_goal_tolerance` (0.75) is
  probably too strict, try ~1.5. Smac planner takes ~29 s to configure (check on the Pi).
  MPPI speed limits are untested guesses.
- Not done: ultrasonic range layer (LIDAR blind below ~0.57 m), real steer angle for
  `minimum_turning_radius`, scan-silent handling (a gated-off scan looks like "no obstacles"),
  Pi CPU check, boundary roaming and stuck recovery.

## Open questions
- Exact ultrasonic part (model number) is unknown, so their range/cone are still placeholders.
- Which RPLIDAR S2 variant (30 m or 18 m) is on the rover; check the label.
- Where the real GPS antenna sits (a forest rover probably needs it up on a mast, not in the LIDAR plane).
- Real mass, ground clearance, max steer angle, lift speed, and sensor positions.
- Do the real wheels sit outside the chassis? Track width (0.559) is almost equal to chassis width (0.562).
- The real IMU (MPU6050 per old comments) probably has no compass heading, but navsat_transform needs a
  heading reference. The sim IMU may hide this. Revisit before hardware.

## Next steps (in order)
1. Add realistic IMU noise in the sim. On hardware: a real heading source for the map filter, drop
   `gps_covariance.py` and the `gps/fix_cov` remap (a real GPS driver reports its own covariance), and
   measure rf2o's real error to replace the placeholder variances in `odom_covariance.py`.
   Sim signal: lift joint position. Real signal: limit switch via ESP32. Same topic for both.
3. Nav2 params in `rover_bringup/config/`: update the footprint (~0.66 m wide with wheels), set
   `minimum_turning_radius` from the real steering limit, feed the ultrasonics to a range obstacle layer
   (the LIDAR is blind below ~0.57 m), and check the MPPI controller's CPU cost on the Pi.
4. Boundary roaming + stuck recovery, then the ESP32 firmware (with a command-timeout watchdog) and the
   Pi hardware bridge.
- New chat: paste this file first, plus the file being edited.

## Ideas for later (not committed to)
- SLAM works in the sim. Open question: how it coexists with the GPS EKF (both want `map -> odom`), and
  whether a 2D LIDAR maps well on rough forest terrain. Keep the lift down.
- Portfolio: Gazebo can't run in a browser. Use screen-recorded video, a README with an architecture
  diagram and test results, and the repo link. Decide later whether to make the repo public.
