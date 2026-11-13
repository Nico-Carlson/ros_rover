# Rover Project Devlog

Full history and step-by-step instructions. Paste THIS file into a new chat so a model has all the
context. `NOTES.md` is the short current-state sheet for people; keep both up to date.

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

1. `ros2 launch rover_bringup sim_nav.launch.py` starts everything in order, on timers: sim (0 s),
   `us_to_range` + rf2o + scan gate (10 s), both EKFs (15 s), Nav2 (22 s), RViz (25 s), `boundary_canvas` (26 s),
   `roam` (30 s) and the control panel (30 s). All delays are guesses: if Nav2 times out waiting for `odom`,
   raise its delay in the file. Nav2 takes ~30 s more to become active ("Managed nodes are active"). Use the
   panel for patrol and boundary control; no extra terminals are needed.
   Gazebo runs server-only (`-s` in `sim.launch.py`), so there is no Gazebo window; attach one with
   `gz sim -g` in another terminal.
2. In RViz, send goals with "2D Goal Pose". Drive by hand only when Nav2 has no goal:
   `ros2 run teleop_twist_keyboard teleop_twist_keyboard` (avoid `j`/`l`; press `k` to stop).

- Manual start order, if you need the terminals separately: `sim.launch.py`, `lidar_odom.launch.py`,
  `ekf.launch.py`, `nav2.launch.py` LAST.
- Stop everything with Ctrl+C in each launch terminal. Closing the Gazebo window does NOT stop the sim.
- Start fresh (stop everything first) before any test where positions are compared: the EKFs start at (0, 0).
- Scan gating is wired into `lidar_odom.launch.py`: rf2o reads `/scan_filtered`, and `sim_lift_signal.py`
  stands in for the lift switch in the sim.
- `ekf.launch.py`, `lidar_odom.launch.py` and `nav2.launch.py` take `use_sim_time:=false` for hardware
  (default true). `slam.launch.py` still hard-codes true.
- `slam.launch.py` is NOT part of `sim_nav` and conflicts with the EKF (both publish `map -> odom`). Run it as
  a separate mapping session.

## Git conventions

- `main` always works; one descriptive branch per piece of work (no phase numbers)
- Commit after each working step; merge to main (`--no-ff`) when a branch's goal is done
- Tag each finished milestone before deleting any branch. Tags so far: `robot-model-v1`,
  `gazebo-sim-v1`, `obstacle-world-v1`, `ultrasonics-v1`, `slam-mapping-v1`, `ekf-localization-v1`,
  `gps-covariance-v1`, `hardware-matched-ekf-v1`, `scan-gating-v1`, `nav2-sim-v1`
- Finished branches still exist (all merged). Deleting them is optional; the tags preserve the work.

## Frame conventions

- `base_footprint`: on the ground under the rear axle center
- `base_link`: one wheel radius above it. X forward, Y left, Z up
- `lidar_link` rides on `lift_platform` (prismatic `lift_joint`, 0 to 1.2192 m)
- Rule: drive/navigate only with the lift fully down. The lift has limit
  switches only (no position feedback)

## Measurements (meters)

| Item                                                           | Value                                                                           | Status                                                                                            |
| -------------------------------------------------------------- | ------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------- |
| Wheelbase                                                      | 0.686                                                                           | measured                                                                                          |
| Track width                                                    | 0.559                                                                           | measured (overlaps chassis width, recheck)                                                        |
| Wheel radius / width                                           | 0.15875 / 0.1016                                                                | measured                                                                                          |
| Chassis L x W x H                                              | 1.118 x 0.562 x 0.457                                                           | measured                                                                                          |
| Ultrasonic mounting height                                     | 3 in (0.0762 m) above the top of the wheels                                     | measured                                                                                          |
| Total mass                                                     | ~30 kg                                                                          | PLACEHOLDER (xacro `chassis_mass` is 24.0 kg; the rest of the model's mass was not summed, check) |
| Ground clearance                                               | 0.08                                                                            | PLACEHOLDER                                                                                       |
| Max steer angle                                                | 0.7 rad                                                                         | PLACEHOLDER                                                                                       |
| Lift platform mass / size / speed                              | 5 kg / 0.4 x 0.4 m / 0.05 m/s                                                   | PLACEHOLDER                                                                                       |
| Ultrasonic positions, angles, cone                             | front, rear, 4 corners at 45 deg (`us_side_y` 0.28, `us_angle` pi/4); see xacro | PLACEHOLDER                                                                                       |
| LIDAR / GPS / IMU positions                                    | see xacro (GPS flat on the chassis)                                             | PLACEHOLDER                                                                                       |
| Nav2 MPPI speeds (`vx_max` 0.65, `vx_min` -0.45, `wz_max` 1.0) | `nav2_params.yaml`                                                              | PLACEHOLDER (untested guesses)                                                                    |
| `minimum_turning_radius`                                       | 0.8 (both MPPI and Smac)                                                        | PLACEHOLDER (derived from the placeholder steer angle: 0.686 / tan(0.7) = ~0.81)                  |
| Goal tolerances                                                | `xy_goal_tolerance` 0.5, `yaw_goal_tolerance` 3.14 (was 1.5)                    | tuned in the sim only                                                                             |
| Costmap `inflation_radius` / `cost_scaling_factor`             | 1.5 / 1.0                                                                       | PLACEHOLDER (from the old project)                                                                |
| Scan gate `lift_timeout`                                       | 1.0 s                                                                           | PLACEHOLDER                                                                                       |
| `sim_nav` start delays                                         | 10, 15, 22, 25, 26, 30, 30 s                                                    | PLACEHOLDER (guesses)                                                                             |
| Nav2 footprint                                                 | [0.902, +-0.33] to [-0.216, +-0.33]                                             | derived from the xacro; depends on the unverified track width                                     |
| rf2o gate limits (`odom_covariance.py`)                        | 1.5 m/s, 2.0 rad/s                                                              | PLACEHOLDER (generous guesses)                                                                    |
| Extra sim trees                                                | `tree_5` to `tree_26`                                                           | PLACEHOLDER positions                                                                             |
| Range layer (`phi`, thresholds, `clear_on_max_reading`)        | 1.2, 0.2 / 0.8, true                                                            | from memory, not tuned                                                                            |

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
- [x] Nav2 in the sim (own launch, footprint, filtered scan; real-hardware tuning still open)
- [x] Boundary roaming, control panel, goal watchdog (stuck detector and recovery SKIPPED for now by decision).
- [x] Control panel with parameter sliders (see "Control panel")
- [x] Ultrasonic range layer in both Nav2 costmaps (sim converter `us_to_range.py`, see "Ultrasonics in Nav2")
- [x] rf2o speed gate in `odom_covariance.py` and 22 extra sim trees (see "Localization failures and fixes")
- [x] Canvas slab only while drawing; `/patrol_status` topic from roam (see "Canvas, status topic and RViz config")
- [x] Show `/patrol_status` on the control panel (next, small)
- [ ] Replace placeholder measurements and sensor specs with real ones
- [x] exec_depend entries in both package.xml files (rover_bringup, rover_description), `rosdep check` clean except rf2o's own harmless `cmake_modules` error (branch `exec-depends`, 2026-11-08). Not yet tried on a real Pi.
- [x] Pre-ROS and old micro-ROS firmware both reviewed from source, 2026-11-08 (see "Pre-ROS firmware review" and "Old micro-ROS firmware review")
- [ ] ESP32 firmware (with watchdog) + Pi hardware bridge
- [ ] Serial protocol (Python + C++ + Pi bridge done on a virtual cable; ESP sketch and ROS node not started).
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
- Don't over-verify: I asked not to double check everything. Check only things that can actually break something.
- keep DEVLOG.md current: as progress is made, record relevant context and
  instructions here. It is the project's "second brain" so a new model or a new
  maintainer never has to rebuild context from scratch. Also update `NOTES.md` (the short current-state sheet).

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
  shows nothing. (`sim_nav.launch.py` handles this.)
- The EKF's pose starts at (0, 0, 0) whenever it launches, so comparisons against Gazebo or `/odom` are only
  meaningful when the sim and EKF were both started fresh before the rover moved.
- Known harmless warnings (seen repeatedly, no action needed unless they start repeating mid-run):
  - `Failed to meet update rate!` from the EKFs at startup (seen 3 times, never again mid-run; sim load is a
    guess). It may matter on the Pi.
  - `/rf2o_laser_odometry` listed twice in `ros2 node list` (one process; probably an internal helper node
    in the port, a guess).
  - `Parameter controller_server.verbose not found` (MPPI optional parameter).
  - A one-time `Invalid frame ID "odom"` / "Timed out waiting for transform from base_footprint to odom" from
    the Nav2 costmaps if Nav2 starts before the EKF is publishing. Relaunch Nav2 or raise its delay.
  - `/cmd_vel_teleop` appears in the topic list (origin not investigated, nothing publishes on it).
- Sim runs a bit under real time (~8.6 to 8.9 Hz instead of 10 Hz for `/scan`). Inferred, real-time factor
  not checked.

## Mistakes already made (don't repeat)

- ROS 2 has NO fish setup script. Use zsh with `setup.zsh`.
- Edit `~/.config/zsh/.zshrc`, not `~/.zshrc` (ZDOTDIR points to the former).
- Hyprland here uses the Lua config format. Custom changes go in
  `~/hakuspace-control/hyprland-custom.lua`, never in `config.lua`.
- `QT_SCALE_FACTOR` caused black flashing in RViz. Don't use it.
- I deleted the `docking_server` block from `nav2_params.yaml` assuming it was unused, but the stock
  `navigation_launch.py` still starts the docking server and aborts the whole bringup when it fails to
  configure. We now use our own `nav2.launch.py` instead (see "Nav2").
- A commit message can miss a change: the `yaw_goal_tolerance` edit went in with "Nav2 params tested in the
  sim". Check `git log` or `git diff` before assuming a change is not committed.

## Repo layout

- Repo: github.com/Nico-Carlson/ros_rover (private), local folder `~/rover_ws`
- `src/rover_basics`: ament_python practice package (hello publisher/subscriber)
- `src/rover_description`: ament_cmake. `urdf/rover.urdf.xacro` (model), `urdf/rover.gazebo.xacro`
  (Gazebo plugins and sensors), `worlds/rover_world.sdf`, `launch/display.launch.py` (RViz view),
  `launch/sim.launch.py` (Gazebo + spawn + bridge), `rviz/rover.rviz` (saved RViz config, used by `sim_nav`)
- `src/rover_bringup`: ament_cmake.
  - `config/`: `slam_mapping.yaml`, `ekf.yaml`, `nav2_params.yaml`, `boundary.yaml` (git-ignored)
  - `launch/`: `slam`, `ekf`, `lidar_odom`, `nav2`, `sim_nav` (each `*.launch.py`)
  - `scripts/`: `gps_covariance.py`, `odom_covariance.py`, `scan_gate.py`, `sim_lift_signal.py`, `boundary_canvas.py`, `roam.py`, `control_panel.py`, `us_to_range.py`
    (all listed in `install(PROGRAMS ...)` in CMakeLists.txt, each needs `chmod +x`)
- `src/rf2o_laser_odometry`: cloned separately, git-ignored, NOT in this repo (see "Hardware-matched EKF").
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
  0.26 rad outward, rear set faces backward (yaw pi). All positions are PLACEHOLDERS except the height. (Layout changed later:
  front, rear and four 45 deg corner sensors, see "Ultrasonics in Nav2".)
- Simulated as 5-ray `gpu_lidar` fans (cone +/-0.131 rad, range 0.03 to 4 m, 10 Hz, all PLACEHOLDERS) from a
  macro in `rover.gazebo.xacro`, bridged as `sensor_msgs/msg/LaserScan` on `/us/front_left|mid|right` and
  `/us/rear_left|mid|right`.
- Verified: `/us/front_mid` center ray reads 3.84 m to `tree_1` and ~1.39 to 1.40 m (all five rays) to
  `stump_1`, matching hand calculations, while the LIDAR does not see the stump.
- Sim vs real: a real ultrasonic reports ONE distance for its whole cone (the sim gives five), and real ones
  struggle with angled or soft surfaces like foliage. TODO: convert to `sensor_msgs/msg/Range` (what the
  ESP32 firmware will publish) using the nearest of the five rays. (Done in the sim by `us_to_range.py`, see "Ultrasonics in Nav2"; on
  hardware the firmware publishes Range directly.)
- Design consequence: the ultrasonics only cover the front and rear faces at short range. Nav2 needs a
  range-sensor obstacle layer fed by them (built since, see "Ultrasonics in Nav2").

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
  mapping mode, not run together with GPS navigation or Nav2. Running SLAM alone also needs an
  `odom -> base_footprint` source (the EKF normally provides it, rf2o publishes no TF), which is unsolved.
- Not retested since scan gating: `scan_topic` in `slam_mapping.yaml` still reads `/scan`; change it to
  `/scan_filtered` when SLAM is next used.

## Localization (done in the sim, tag ekf-localization-v1)

- HISTORY: this section is chronological. The final design is the dual EKF described in its later bullets
  and in "Hardware-matched EKF"; `/odometry/filtered` no longer exists, and the bullets that mention fusing
  `/odom` are superseded (wheel odometry is no longer fused).
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
  perfect sim GPS, but a real GPS (meters of error, worse under trees) will jitter. (Addressed in the next
  section.) Real GPS module and its accuracy are unknown.
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
  `/odometry/gps` from navsat_transform). Already reused (see "Localization").
- `nav2_params.yaml` (and a duplicate `nav2_params_1.yaml`): was copied into `rover_bringup/config/` and
  edited (see "Nav2"); the original is in `~/SmartFiresFa26/smart_fires_rover/config/`. The footprint and
  AMCL issues are fixed there. `minimum_turning_radius` 0.8 still depends on the real max steer angle.
- `ackermann_bt.xml` and `ackermann_through_poses_bt.xml` (old custom behavior trees): NOT used, not yet
  reviewed. Our `nav2.launch.py` relies on Nav2's default behavior trees.
- `robot.xacro` / `robot.gazebo`: old model, replaced by rover_description. The Gazebo file
  is a useful reference for the Ackermann, IMU, LIDAR, and navsat plugins.
- `Rover_Documentation.docx` (reviewed): RC channels (1 steering, 2 throttle & brake, 6 lift, 7 reverse
  thumb switch), partly blank ESP32 pin tables (limit switches, brake, reverse, throttle in/out, gyro on
  I2C pins 21/22), and a feature list: tilt and collision alarms (caution/danger), Bluetooth telemetry, lift
  interlock (alarms off, brake set, no throttle), auto-brake when throttle is off, brake releases before
  forward, override switch, stop on loss of radio contact. No part numbers and NO wheel encoders mentioned.
  The firmware must map `/cmd_vel` to throttle/steering/brake/reverse.
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
  - yaw rate (sim only, see GPS section) and `/odom_rf2o_cov` forward speed + yaw rate. Map filter: GPS x/y as
    `odom0`, IMU yaw + yaw rate, and `/odom_rf2o_cov` speed + yaw rate as `odom1`.
- GOTCHA: a sensor numbered `odom1` with no `odom0` is ignored (the map filter read exactly 0.0 until the GPS
  entry was renamed `odom0`). Number sensors from 0.
- rf2o (LIDAR odometry, GPL v3): cloned into `src/rf2o_laser_odometry`, which is git-ignored, NOT in this
  repo. Fetch: `cd ~/rover_ws/src && git clone -b ros2 https://github.com/MAPIRlab/rf2o_laser_odometry.git`,
  then `colcon build --packages-select rf2o_laser_odometry` (~20 s on Jazzy). The port's own launch file
  defaults to `publish_tf: True` and `base_link`, which would fight the EKF; use ours,
  `rover_bringup/launch/lidar_odom.launch.py` (`publish_tf` False, `base_frame_id` base_footprint, 10 Hz to
  match the scan, `use_sim_time` is now a launch argument, default true). Publishes `/odom_rf2o`.
- rf2o publishes all-zero covariances. `rover_bringup/scripts/odom_covariance.py` (installed in
  CMakeLists.txt with `gps_covariance.py`) fills the twist covariance (`speed_variance`, `yaw_rate_variance`,
  both 0.01, PLACEHOLDER guesses, not measured) and republishes `/odom_rf2o_cov`.
- Start order (fresh start, three terminals): `sim.launch.py`, then `lidar_odom.launch.py`, then
  `ekf.launch.py` (now automated by `sim_nav.launch.py`).
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

## Scan gating (done in the sim, tag scan-gating-v1)

- Goal: LIDAR consumers (rf2o and Nav2 now; SLAM later) must only see scans taken with the lift fully down.
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
  `odom_covariance`; rf2o's `laser_scan_topic` is `/scan_filtered`.
- Verified (fresh start, full stack): `/scan_filtered` ~8.9 Hz vs raw `/scan` ~8.6 Hz, so the gate loses no scans;
  both are below 10 Hz probably because the sim runs under real time (inferred, real-time factor not checked).
  A short reverse drive gave `/odom_rf2o` (-0.982, 0.467) vs sim wheels (-0.949, 0.508), about 5 cm apart.
  Agreement only, not a ground-truth check.
- On hardware, leave `sim_lift_signal.py` out of the launch (it is not behind an argument yet; remove it from
  `lidar_odom.launch.py` or add a launch argument); the ESP32 publishes `/lift_down`, and the Pi bridge must
  treat a missing signal as "lift not down".
- Open: when `/scan_filtered` goes silent (lift up or signal lost) Nav2's costmaps just stop updating, which
  can look like "no obstacles". The rover should stop when the scan goes silent. Not designed yet.

## Nav2 (done in the sim, tag nav2-sim-v1)

- `config/nav2_params.yaml`: copied from the old project, then: footprint [0.902, +-0.33] to [-0.216, +-0.33]
  (chassis centered 0.343 m ahead of base_footprint; 0.33 half-width includes wheels, track width unverified),
  scan topics -> `/scan_filtered` (both costmap obstacle layers and `collision_monitor`), odom topics ->
  `/odometry/local` (`bt_navigator`, `velocity_smoother`), `consider_footprint: true` in CostCritic,
  `yaw_goal_tolerance` 0.75 -> 1.5, dropped amcl, route_server, docking_server and loopback_simulator blocks.
  No static map: rolling-window costmaps, Smac Hybrid planner (Reeds-Shepp, reverse allowed), MPPI controller
  (Ackermann motion model, `vx_min` -0.45 allows reversing).
- `launch/nav2.launch.py`: OWN launch, not the stock `navigation_launch.py`, because the stock one hard-codes
  `route_server` and `docking_server` (docking fails without a dock plugin and aborts the whole bringup).
  Starts eight nodes (controller, smoother, planner, behavior, bt_navigator, waypoint follower, velocity
  smoother, collision monitor) plus a lifecycle manager; `use_sim_time` is an argument (default true). It
  passes the params file straight in (the stock file's namespace and `RewrittenYaml` handling is skipped, which
  worked). Start Nav2 LAST (see Quick start; `sim_nav.launch.py` does it on timers).
- Velocity chain: `cmd_vel_nav` (controller, behavior server) -> velocity smoother -> `cmd_vel_smoothed` ->
  collision monitor -> `cmd_vel` (to the sim; to the ESP32 bridge on hardware). Goals come from RViz "2D Goal
  Pose". Nav2 reads `/odometry/local`; the global costmap uses the `map` frame.
- `launch/sim_nav.launch.py`: one command for the whole sim (sim, lidar_odom, ekf, nav2, RViz with the saved
  `rover.rviz`) on timers (10, 15, 22, 25 s; guesses).
- Verified in the sim: all nodes active ("Managed nodes are active"); a goal in RViz plans and the rover
  drives to it. Wavy turns and a "v" at the end of the path were fixed by loosening `yaw_goal_tolerance` to
  1.5 rad (verified by eye, not measured).
- Smac planner takes ~29 s to configure (check on the Pi). MPPI speed limits (`vx_max` 0.65, `vx_min` -0.45,
  `wz_max` 1.0) are untested guesses.
- Not done (the ultrasonic range layer is done since, see "Ultrasonics in Nav2"): real steer angle for
  `minimum_turning_radius`, scan-silent handling (a gated-off scan looks like "no obstacles"),
  Pi CPU check, boundary roaming and stuck recovery. Goals around trees at longer range are untested.

## Roaming and stuck recovery (merged without stuck recovery, tag roaming-and-panel-v1)

- Plan: (1) boundary polygon editor, (2) roam goal generator, (3) scan stops (lift up, wait, lift down),
  (4) stuck detector (BOTH signals: commanded vs rf2o speed, and no progress toward the goal) + recovery,
  (5) a supervisor tying them together.
- DONE (1) `scripts/boundary_canvas.py`: RViz "Publish Point" clicks become a closed polygon. RViz Fixed Frame
  must be `map` (clicks in `odom` are ignored). Publish Point only works on a solid surface, so the node
  publishes a 200 x 200 m flat slab as a Marker (`/boundary_canvas`); the line is `/boundary_line`. Click
  within 3 m of the first point (needs 3+ points) to close; closing saves `config/boundary.yaml` as lat/lon
  around the datum. `ros2 service call /clear_boundary std_srvs/srv/Empty` cancels at any time. The datum
  (45.0, -110.0) now lives in THREE places that must match: world file, `ekf.yaml`, this node's parameters.
  Assumes map origin = datum, x east, y north (inferred from the GPS tests, not separately checked).
  The saved boundary is not displayed in RViz on a new run (the line only draws that run's clicks).
- DONE (2) `scripts/roam.py`: `load_boundary`, `point_in_polygon`, `VisitGrid` (5 m cells, counts entries),
  `SmoothNoise` + `Walker` (hops 5 to 8 m, up to +/-60 deg turn per hop; when every cell within 10 m has
  > = 1 visit it targets the least-visited cell, in short hops), and `RoamNode` (reads `map -> base_footprint`
  > from TF, sends `NavigateToPose` goals, a 1 s timer marks cells and sends the next goal when `busy` is
  > False). Offline test: 26 cells, 23 visited after 60 hops. Run with
  > `python3 ~/rover_ws/src/rover_bringup/scripts/roam.py --ros-args -p use_sim_time:=true` (needs
  > `use_sim_time` or timestamps do not match the sim).
- (Since done, see the last bullet of this section.) Was: add `boundary_canvas.py` and `roam.py` to `install(PROGRAMS ...)` and `chmod +x`
  them, add a launch file, decide whether `boundary.yaml` is committed (it is test data from the sim).
- OPEN PROBLEM: roaming stalls. One goal succeeded (status 4, ~38 s). Others were sent and never reported a
  result, so the loop stops waiting. Seen once: `controller_server: Failed to make progress` then follow_path
  aborted. Seen once: planner `Start occupied` at (4.27, -1.10) then `Goal failed`. CAUSE UNKNOWN.
  Hypotheses (all unconfirmed): (a) the Ackermann rover cannot turn in place, so it sits on the goal wiggling
  because the final HEADING is out of tolerance; `yaw_goal_tolerance` was changed 1.5 -> 3.14 in
  `nav2_params.yaml` to test this, but the two test runs after the change were stopped before an outcome, so
  it is UNTESTED; (b) Nav2's recovery behaviors (spin, backup) run for a long time on an Ackermann; (c) the
  1.5 m `inflation_radius` placeholder leaves no free start/goal cell near obstacles.
- RULED OUT (measured): the velocity chain (`/cmd_vel_nav` ~19 Hz, `/cmd_vel_smoothed` ~20 Hz, `/cmd_vel`
  13 to 15 Hz, rover moves) and the scan gate (`/scan` ~6.7 Hz, `/scan_filtered` ~6.2 Hz, `/lift_down`
  published).
- Sim performance: Gazebo real-time factor ~52 to 55%, RViz ~31 fps, `/scan` ~6.7 Hz (was 8.6 to 8.9 in the
  notes). `/lift_down` runs at ~400 Hz because `sim_lift_signal.py` publishes on every joint-state message;
  probably wasteful and may contribute (guess, untested). Fix later: publish it from a 10 Hz timer. The sim is
  too slow to say anything about the Pi.
- Probably harmless: rf2o `Waiting for laser_scans....` repeating (rf2o runs at 10 Hz but scans arrive at
  ~6 Hz; inferred). `Failed to meet update rate!` from the EKFs again.
- RViz tip: with Fixed Frame `map` everything attached to `odom` shakes, probably because `map -> odom` follows
  the noisy sim GPS (guess). Use `odom` to watch driving, `map` only to click boundary points.
- Debug tip: launch with `ros2 launch rover_bringup sim_nav.launch.py 2>&1 | tee ~/sim_nav.log`, then
  `grep -i "abort\|fail\|occupied\|progress" ~/sim_nav.log | tail -20`. The terminal scrolls too fast to read.
- Lift: the DIHOOL scissor lift (confirmed the real one) is listed at ~0.83 m of travel (10.43 in to 43.31 in),
  but the xacro `lift_travel` is 1.2192 m. Reconcile before relying on LIDAR height.
- Not possible easily: viewing the SLAM map alongside the EKF stack (both publish `map -> odom`).
- (Old plan; the watchdog and markers are done, see "Next steps" for what remains.) NEXT: (1) rerun roaming with `yaw_goal_tolerance` 3.14 and give each goal up to 3 min (sim runs at half
  speed); note whether `Goal finished` prints and the next goal is sent. (2) Add a goal watchdog in `roam.py`
  (cancel a goal that runs too long and pick a new one), then the stuck detector and recovery. (3) Publish the
  boundary and goals as RViz markers from the roam node. (4) Scan stops (sim lift cannot move, so stub it).
  (5) Fix `sim_lift_signal.py` rate.
- FIXED: roaming stalls were partly a lost-result bug. rclpy's action client holds only a weak reference to the
  goal handle, so `roam.py` now keeps `self.goal_future`, `self.goal_handle` and `self.result_future`. After
  this, 4 goals in a row finished (status 4), each followed by the next, and unvisited cells fell 22 -> 16.
  (Cause inferred from the fix working, not proven.)
- The real Nav2 failure is "Start occupied": the rover parks next to a tree, the planner says its own
  footprint is too close to an obstacle, and the goal aborts. The costmap has no map, so it only sees the tree
  as the rover approaches, and a goal chosen blind can land next to it. The heading hypothesis
  (`yaw_goal_tolerance` 3.14) is UNSUPPORTED; the change is now committed (kept).
- `controller_server: Failed to make progress` also fires mid-route sometimes; the behavior tree recovered
  once (goal still finished, status 4). Cause unknown.
- WORKS (checked by eye in RViz, 2026-11-03): `roam.py` publishes the boundary (yellow line) and current goal
  (green ball) on `/roam_markers`. In RViz: Add > By topic > Marker. To keep it, save the RViz config over
  `src/rover_description/rviz/rover.rviz` (File > Save Config As) and run
  `colcon build --packages-select rover_description` (launch uses the installed copy). Saved and committed later (see
  "Canvas, status topic and RViz config"). Still unchecked: whether Nav2 paths stay inside the polygon.
- LOG FINDINGS (2026-11-03, one `~/sim_nav.log` run, 8 goals): 5 goals ended "Goal succeeded"; no
  `Goal failed` line and no `Start occupied` in this log window. `Failed to make progress` fired 3 times
  (about 30 s and 16 s apart in wall-clock time). Each time the controller reset its optimizer, the behavior tree
  cleared the local costmap, and a new `Received a goal` followed within ~20 ms (Nav2's default
  retry-after-clearing-costmap). The first retry succeeded; the second retry failed again, and a third goal
  then succeeded (the lines between were filtered out by my grep, so what happened there is a guess).
  - No `spin` or `backup` recovery ran in the log (only their startup lines), which weakens hypothesis (b)
    above for this run. The log window was filtered, so this is "not seen", not "did not happen".
  - Before the first failure (log lines 4045 to 4072), rf2o's pose was frozen at exactly
    [-9.671636, 1.193520, -2.941370] for the ~1 s visible, so the rover was not moving. Not known whether it
    was blocked, being sent zero velocity, or rf2o had stopped updating (the `Waiting for laser_scans....`
    warnings appear there too, expected at ~6 Hz scans). CAUSE STILL UNKNOWN.
  - Guess (unconfirmed): a stale obstacle in the local costmap, since clearing it lets the goal go on. The
    spot (-9.7, 1.2) should be open ground if the origin is the spawn point (trees are at x 5 to 12).
  - To narrow it down: next stall, note what is near the rover in RViz, and check `/cmd_vel` while it sits
    still (`ros2 topic echo /cmd_vel --once`; this shows only the current command, not the past).
- DONE: `boundary_canvas.py`, `roam.py` and `control_panel.py` are installed, `chmod +x`'d and launched from
  `sim_nav.launch.py`. `boundary.yaml` is git-ignored (sim test data); `roam.py` starts without it and warns.
  `yaw_goal_tolerance` 3.14 is committed but the heading hypothesis is still UNSUPPORTED by testing.

## Control panel (done in the sim, branch roaming-and-recovery)

- `scripts/control_panel.py` (PyQt5, confirmed installed; PySide6 is not). ROS runs inside Qt through a 50 ms
  QTimer calling `rclpy.spin_once`.
- Services (all `Trigger` unless noted): `roam.py` offers `/start_patrol` and `/stop_patrol` (stop also cancels
  the running goal; start answers `success=False` if there is no boundary). `boundary_canvas.py` offers
  `/clear_boundary` (Empty) and publishes `/boundary_closed` after saving; `roam.py` reloads the boundary on it
  (grid and walker rebuilt, old goal marker cleared). New Boundary in the panel = stop patrol, then clear.
- Mode toggle (Point to point / Boundary): only greys out the three buttons. It does NOT stop a running patrol.
  Point to point uses RViz "2D Goal Pose".
- Goal watchdog: `roam.py` parameter `goal_timeout` (90 s sim time, PLACEHOLDER) cancels an accepted goal that
  runs too long; verified with a 10 s test. A goal that is never accepted is not covered.
- Sliders (`ParamSlider`): set a DOUBLE parameter on any node through its `set_parameters` service when the
  slider is released. Verified working: roam `goal_timeout`; controller_server `FollowPath.vx_max`, `wz_max`,
  `general_goal_checker.yaw_goal_tolerance`, `xy_goal_tolerance`; local and global costmap
  `inflation_layer.inflation_radius` and `cost_scaling_factor` (costmap halo responds). Names confirmed with
  `ros2 param get`; the global costmap's names were not all individually checked.
- `FollowPath.CostCritic.cost_weight` (3.81) exists but has no slider.
- Sliders START at values hard-coded in the file, not read from the nodes, so they can disagree with the real
  values. A parameter declared as an integer would be rejected (label shows "(rejected)").
- Slider changes last only until relaunch: copy keepers into `nav2_params.yaml`.
- Local vs global (my reading of Nav2, not tested here): global = planner routes and "Start occupied" failures;
  local = how the controller passes obstacles and "Failed to make progress". Keep local at or below global.
- DONE: a label under the sliders subscribes to /patrol_status and goes stale after 5 s. Not done: sliders reading real values at startup, tabs if the panel gets too tall.

## Ultrasonics in Nav2 (done in the sim, branch roaming-and-recovery)

- The ultrasonics were publishing but unused. `scripts/us_to_range.py` (SIM ONLY, launched by `sim_nav` at 10 s)
  converts each 5-ray `/us/<name>` LaserScan into one `sensor_msgs/Range` on `/us_range/<name>` (nearest valid
  ray; `inf` when nothing is in range; field of view 0.262). The real ESP32 firmware will publish Range
  directly, so drop this node on hardware and keep the costmap config.
- Verified: `/us_range/front_mid` read 1.388 m toward `stump_1`, matching the earlier hand-calculated 1.39 to 1.40 m.
- Layout changed: front and rear center sensors plus four corner sensors at 45 deg outward
  (`us_side_y` 0.28, `us_angle` pi/4 in `rover.urdf.xacro`; positions still PLACEHOLDERS). Checked with
  `tf2_echo base_link us_front_left`: (0.912, 0.280, 0.235), yaw 45 deg. Gaps remain on the sides between corners.
- `nav2_params.yaml`: `range_layer` (`nav2_costmap_2d::RangeSensorLayer`, the six `/us_range/*` topics) added to
  BOTH costmaps, between `obstacle_layer` and `inflation_layer` (inflation must stay last). Parameters
  (`phi` 1.2, thresholds 0.2/0.8, `clear_on_max_reading` true) are from memory, not tuned. Verified by eye:
  the stump (invisible to the LIDAR) now shows in the local and global costmaps.
- Limit: the sensors see ~4 m, so the global costmap learns about short obstacles only near the rover.
- Gotcha: if `us_to_range.py` is not running, short obstacles silently disappear. Rebuilding
  `rover_description` also reinstalls `rover.rviz` from `src`, so an RViz display added live and never saved is lost.

## Localization failures and fixes (branch roaming-and-recovery)

- Symptom: roaming aborts, RViz "drifting" off the boundary, ~34 `Failed to make progress` per 10 min. The
  inflation-slider experiments were confounded by this (don't trust those runs).
- Method: a loop that prints `/odom`, `/odom_rf2o`, `/odometry/local`, `/odometry/global` every 30 s
  (`ros2 topic echo <t> --once --field pose.pose.position`). The four echoes run in sequence, so rows are skewed
  by up to a few meters while the rover moves. `/odom` (sim wheels) is a sanity reference, not ground truth.
- Finding 1: the sim world was too sparse. With the rover far from the trees, all 3200 `/scan_filtered` rays were
  `inf` and rf2o's pose froze. Fix: 22 extra trees (`tree_5` to `tree_26`, radii 0.2 to 0.35, positions are
  PLACEHOLDERS) generated by a Python script and inserted before `</world>`. They cover the usual boundary
  (about x -31 to 17.5, y -36 to 19.5).
- Finding 2 (the main one): rf2o publishes impossible speeds. Seen: `/odom_rf2o_cov` linear.x 18.3 m/s and
  angular.z -3.83 rad/s (rover max 0.65 m/s and 1.0 rad/s). The odom EKF believed it (`/odometry/local` read
  17.6 m/s, positions of -200 to -458 m, once 7.6 km); the map EKF was pulled ~40 m off once. Cause of the
  spikes is UNKNOWN (guesses: irregular scan timing, bad scan matches; `/scan` intervals were 0.077 to 0.3 s).
- Fix: `odom_covariance.py` drops any reading that is non-finite or above `max_speed` 1.5 m/s or
  `max_yaw_rate` 2.0 rad/s (parameters; limits are generous guesses). It logs `Dropped rf2o reading` (throttled).
  After the fix, one 8 min run: `/odometry/local` stayed within a few meters of `/odom`, 21 readings dropped.
- rf2o's own pose (`/odom_rf2o`) still wanders 10 to 25 m from everything else. The EKFs use only its speed and
  yaw rate (heading comes from the IMU), so this does not matter.
- Results (runs differ in length, boundary and random path, so only rough comparisons):
  run 1 (before trees and gate): 34 ok, 1 aborted, 34 progress failures, 0 `Start occupied`;
  run 2 (server-only Gazebo): 15 ok, 5 aborted, 3 `Start occupied`;
  run 3 (inflation lowered, exact values not recorded): 18 ok, 8 aborted, 4 `Start occupied`;
  run 5 (trees + gate): 34 ok, 5 aborted, 4 progress failures, 1 `Start occupied`.
- Abort causes in run 5 (not fully sorted): many `Goal failed` follow a cancel request (probably the watchdog),
  one `Start occupied` at (-16.5, 5.0) with a tree at (-17, 5), one `Timed out ... compute_path_to_pose`.
- Gazebo now runs server-only (`-s` in `sim.launch.py`). It gained only ~2% in `/scan` rate (7.28 to 7.46 Hz),
  kept by choice. `gz sim -g` attaches a GUI to the running server (works).
- Tool gotchas: `ros2 topic echo` truncates arrays to 128 entries (use `--truncate-length 0`; count with
  `grep -o inf | wc -l`). "A message was lost!!!" in echo output is the echo client, not the node. My awk
  spike-detector one-liner did not work; read `--once --field twist.twist` instead. `grep -a` for logs that
  contain binary characters.
- Canvas slab hiding: DONE, see "Canvas, status topic and RViz config".
- Real-map note: the Bob Marshall is designated wilderness (motorized equipment is generally barred; check
  with the Forest Service) and Lubrecht is a UM research forest (ask them). Terrain could come from a Gazebo
  heightmap (USGS elevation) and a SLAM map of the site.

## Canvas, status topic and RViz config (done in the sim, branch roaming-and-recovery)

- `boundary_canvas.py`: the green slab is now hidden at start. New Boundary (`/clear_boundary`) shows it, and it
  is removed with a `DELETE` marker (`hide_canvas`) when the polygon closes. So on the FIRST run you must press
  New Boundary before you can click corners (Publish Point needs a surface to land on).
- `roam.py` bug fixed: the "No boundary yet" block (uses `response`) had landed in `try_start`, which raised a
  NameError whenever no boundary was loaded. It now lives in `on_start_patrol`, and `try_start` just returns if
  there is no polygon or patrol is stopped. Start Patrol answers `success=False` with no boundary.
- `/patrol_status` (`std_msgs/String`, once a second from `roam`): `<state> | goals ok N, failed N, canceled N |
unvisited cells N`. States: `No boundary`, `Stopped`, `Patrolling`, `Patrolling (waiting for a goal)`. Goal
  status 4 counts as ok, 5 as canceled (Stop Patrol and the watchdog), anything else as failed. Counters reset
  when a new boundary loads. Verified with `ros2 topic echo /patrol_status`. NOT yet shown on the panel (TODO).
- RViz config gotcha: `sim_nav` starts RViz with the INSTALLED `rover.rviz`, so File > Save Config writes there
  and the next `colcon build --packages-select rover_description` overwrites it from `src`. Use File > Save
  Config As and pick `~/rover_ws/src/rover_description/rviz/rover.rviz`, or copy the installed file over the
  `src` one (`diff` the two to check). Committed with a new Marker display.
- `gz sim -g` attaches a Gazebo GUI to the server-only sim.

## Open questions

- Exact ultrasonic part (model number) is unknown, so their range/cone are still placeholders.
- Which RPLIDAR S2 variant (30 m or 18 m) is on the rover; check the label.
- Where the real GPS antenna sits (a forest rover probably needs it up on a mast, not in the LIDAR plane).
- Real mass, ground clearance, max steer angle, lift speed, and sensor positions.
- Do the real wheels sit outside the chassis? Track width (0.559) is almost equal to chassis width (0.562).
- The real IMU (MPU6050 per old comments) probably has no compass heading, but navsat_transform needs a
  heading reference. The sim IMU may hide this. Revisit before hardware.
- Why rf2o produces impossible speeds (irregular scan timing? bad matches?). The gate in `odom_covariance.py` only
  hides it.
- The stuck detector needs a second signal besides rf2o speed (rf2o fails in empty scenes and spikes).
- Remaining roaming aborts: `Start occupied` next to trees, planner timeouts; retest the inflation sliders now that
  localization is stable.

## Parts (from Amazon listing titles; specs below are from memory, NOT verified)

- Ultrasonic: HC-SR04 (about 15 deg cone, ~4 m nominal, 5 V echo needs level shifting for the 3.3 V ESP32,
  not weatherproof). Sim cone (+/-0.131 rad) and 4 m range are close.
- GPS: NEO-6M (GY-NEO6MV2). Typically ~2.5 m accuracy, 1 to 5 Hz, heading only while moving.
- IMU: MPU-6050 (GY-521), 6-axis, NO magnetometer, so no absolute heading.
- Lift: DIHOOL electric scissor lift table, listed 10.43 in to 43.31 in (~0.83 m travel). The xacro uses
  1.2192 m. CONFIRM this is the real lift before changing `lift_travel`.

## Concerns about the Pi (nothing measured yet)

1. CPU load: S2 gives 3200 points per scan at 10 Hz feeding rf2o, Nav2 costmaps, and SLAM if used; MPPI cost
   and Smac's ~29 s configure are also unmeasured.
2. Timing: the EKFs logged `Failed to meet update rate!` in the sim; a loaded Pi could repeat it and Nav2 would
   act on stale pose.
3. Clean build: exec_depend entries are declared now but untried on a real Pi. On the Pi use `rosdep install --from-paths src --ignore-src -r -y --skip-keys "cmake_modules ros_gz_sim ros_gz_bridge joint_state_publisher_gui"` (cmake_modules is rf2o's own stale dependency; the other three are sim/PC-only).
4. Sim-only nodes (`sim_lift_signal.py`, `gps_covariance.py`, `us_to_range.py`) must be left out of the Pi launch, and
   `lidar_odom.launch.py` has no argument for that yet.

## Live tuning sliders (done for Roam and Nav2, see "Control panel"; danger zones and stop distances still need nodes that do not exist yet)

- DONE as a custom PyQt5 panel, not `rqt_reconfigure`. The bullets below are the original idea.
- Goal: a small tool with sliders to change values while the sim or rover runs (goal tolerances, speed limits,
  danger zones) with no rebuild or relaunch.
- Likely approach: `rqt_reconfigure` for ROS 2 parameters (sliders for numbers). Open question: which of our
  Nav2 parameters can change at runtime. Many controller and costmap settings can; footprint and plugin lists
  need a restart (unverified, check per parameter with `ros2 param set`).
- Danger zones and stop distances need a node that reads them as parameters (the ultrasonic range layer and
  stuck recovery do not exist yet), so this depends on those.
- Values tuned live must be copied back into `nav2_params.yaml` afterwards, or they are lost on relaunch.
- A custom GUI (Python, Qt or a web page) is possible if `rqt_reconfigure` is too limited; decide after
  trying it.

## Firmware approach (proposed, NOT final)

### Decided so far (2026-11-08)

- Order: (1) the new ESP32 codebase working with ROS in MANUAL mode only, with an RC switch that overrides any Nav2 command so the rover drives fully by hand; (2) only then add the Nav2 stack that works in the sim.
- The manual/auto override lives in the master ESP (works with the Pi off or crashed). The RC receiver plugs straight into the master ESP.
- Plain serial (UART RX/TX) instead of micro-ROS, hoping to fix the lag. micro-ROS also ran over a UART, so what changes is the protocol on top (text lines + checksum + a pyserial bridge on the Pi), not the wire.
- The rover, ESPs and Pi were not available on 2026-11-08, so the first work uses PLACEHOLDER pins.
- First test state (proposed, not confirmed): wheels off the ground, master drives from the RC like the pre-ROS firmware, the switch picks manual or auto, auto commands do nothing yet.
- Pi link: ONE dedicated UART (RX/TX) per ESP, no shared link and no USB adapters. PLACEHOLDER (no hardware access): both ESPs use UART0 (RX0/TX0) as in the old ROS firmware (master -> Pi UART0, sensor -> Pi UART2 per its comments). Consequences: USB flashing and debug prints share these pins, so unplug the Pi wires to flash and use a debug channel that does not depend on the Pi link (Bluetooth on the sensor board, something equivalent still to choose on the master). The Pi needs two UARTs; the overlay names in `config.txt` are still unchecked. The master<->sensor safety link stays on 16/17 (GPS moves to 32/33, see the open questions).
- No hardware yet: work proceeds with PLACEHOLDER pins and no board. Pure logic (packet CRC and parser, mode switch, timeouts, throttle/brake/steer maps, reverse state machine, tilt math) is kept free of Arduino calls so it compiles and is tested on the PC. The Pi bridge is tested against a fake ESP script on a virtual serial port (`socat`). Everything is marked UNTESTED ON HARDWARE until the bench checklist in "Pre-ROS firmware review" has been run on the real boards.

### Serial protocol plan (2026-11-08, PLAN ONLY: nothing written or tested yet)

- Why: a UART is a raw byte stream with no message boundaries. The pre-ROS sensor-to-master link (10 raw bytes, no
  markers) misaligned permanently after one lost byte. The micro-ROS version fixed that with a magic byte + CRC; the
  idea is kept.
- Format: one message per line, ending in a newline: `TYPE,field,field,...*CS`. `CS` is a checksum over every
  character before the `*`, written as two uppercase hex digits. Proposed: XOR of all bytes (NMEA style, the same way
  GPS modules do it; GPS NMEA lines can also be parsed this way). A line with a bad or missing checksum is dropped;
  after any glitch the receiver waits for the next newline and recovers on its own. Example (illustrative, checksum
  not computed): `CMD,0.30,12.5,0.0*5A`.
- Rules: the receiver never blocks waiting for a line (read what is available, assemble into a buffer, handle a line
  only when a full one has arrived); a maximum line length (PLACEHOLDER ~100 bytes) with the buffer reset on overflow;
  debug text must never share this link (see the Pi link decision).
- Messages (fields and rates are PLACEHOLDERS):

  | Direction | Message  | Carries                                                                           |
  | --------- | -------- | --------------------------------------------------------------------------------- |
  | Pi -> ESP | `CMD`    | speed (m/s), steering ANGLE (deg, computed on the Pi, not a yaw rate), lift speed |
  | Pi -> ESP | `DANGER` | stop flag                                                                         |
  | ESP -> Pi | `IMU`    | tilt, yaw rate, accelerations (Pi adds timestamp, frame_id, covariances)          |
  | ESP -> Pi | `GPS`    | lat, lon, fix quality / HDOP, fix age (send only on a NEW fix, not repeats)       |
  | ESP -> Pi | `BAT`    | battery voltage                                                                   |
  | ESP -> Pi | `LIFT`   | up and down limit switch LEVELS (pressed or not), sent continuously               |
  | ESP -> Pi | `MODE`   | manual or autonomous, danger and warning flags, brake state                       |

- Watchdogs (carried over from the old designs): master with no fresh `CMD` for ~250 to 500 ms (PLACEHOLDER) =
  zero throttle and brake applied; Pi with no ESP lines treats the lift as not down; master with no sensor
  heartbeat (350 ms in the old firmware) = danger. The master<->sensor safety link keeps its own framed packet on 16/17.
- Build order for this step: write the spec, then the encoder and parser once in Python and once in C++ (logic kept
  free of Arduino calls so it runs on the PC), and make each test the other, including corrupted and dropped bytes.
- NEXT STEP (user): write the checksum function in Python (about 6 lines: XOR the characters before `*`, format as two
  uppercase hex digits), then a matching check function. All UNTESTED ON HARDWARE.

### Serial protocol implementation (2026-11-08, branch serial-protocol)

- Lives in `~/rover_ws/protocol/`, NOT a ROS package, so it tests on the PC with no ROS. `SPEC.md` is the
  source of truth (DRAFT: `CMD`, `MODE`, `LIFT`, `BAT` defined; all fields and rates PLACEHOLDER).
- Python (`protocol.py`): `checksum`, `parse_line`, `make_line`, `make_cmd`, `make_lift`, `LineReader`
  (string-based, `max_len` 100), `split_body`.
- C++ (`cpp/protocol.h`, `cpp/protocol.cpp`): `protocol_checksum`, `protocol_check_line`, `LineReader` struct
  with `line_reader_feed` (one byte at a time, fixed 101-byte buffer, no Arduino calls, no heap).
- `vectors.txt`: shared list (`ok|line|body`, `bad|line`) read by BOTH test suites so the languages cannot
  drift. Add every new message's examples there; get checksums from `checksum()`, not by hand.
- Pi side: `bridge.py` has `Bridge` (pyserial, non-blocking, `poll()`, `send_cmd()`, `poll_with_lift()`) and
  `LiftState` (`lift_down` true only if the latest LIFT line is under `timeout` 1.0 s old, down=1, up=0;
  malformed lines ignored). `fake_esp.py` and `try_bridge.py` are manual tools for a `socat` virtual cable.
- Tests (run from INSIDE `protocol/`, since running pytest from `~/rover_ws` also collects the failing
  `rover_basics` flake8 test): `cd ~/rover_ws/protocol && python3 -m pytest -q` (20 tests, includes
  `test_bridge.py`, which starts its own `socat`). C++: `cd cpp && g++ -Wall -o test_protocol
test_protocol.cpp protocol.cpp && ./test_protocol`.
- Needs `sudo apt install python3-serial socat` (pip is not installed in the distrobox).
- Verified on the PC only (virtual serial cable): good lines arrive, junk and corrupted lines are dropped and
  the link recovers, CMD reaches the ESP, a silent ESP makes `lift_down` false. UNTESTED ON HARDWARE.
- Known gaps: Python reader takes `str` (bridge decodes bytes); an over-long line leaves its tail read as a new
  line (one extra drop, harmless); no ROS node yet; `BAT`, `IMU`, `GPS`, `DANGER` not in the Python or C++
  helpers; the Pi still has to treat "both limits pressed" as a fault (done in `LiftState`).
- NEXT: finish `IMU`, `GPS`, `DANGER` in the spec; the ESP32 master sketch using the C++ code (CMD in, MODE out,
  command-timeout watchdog); a ROS node wrapping `Bridge`.
- Branch state (2026-11-08): `serial-protocol` has NOT been merged to main or tagged. When you are happy with it:
  `git checkout main && git merge --no-ff serial-protocol && git tag serial-protocol-v1`. Check `git status` first:
  the last few commits (the silent-ESP test in `test_bridge.py`, `.gitignore`) were never confirmed in the log.
- Decisions made this session: booleans on the wire are `1`/`0`; field order is part of the spec; speed/lift
  formatted with 2 decimals and steering with 1 (`make_cmd`); both languages must pass `vectors.txt`.
- Spec gaps still open: `DANGER` (Pi -> ESP), `IMU` and `GPS` messages are in the old plan table but have no
  exact fields in `SPEC.md`. Raw IMU axes/units and GPS fix-age fields need deciding (see the IMU and GPS
  findings in "Old micro-ROS firmware review": send only NEW GPS fixes, send HDOP/fix quality, stamp on the Pi).
- C++ side (updated 2026-11-08): checksum, line checker, `LineReader`, message builders (`protocol_make_line`,
  `protocol_make_lift`, BAT and MODE builders) and the `CMD` parser are all done and tested on the PC.
  Dead zone (1401 to 1599 us) now drops to manual immediately instead of holding the current mode.
  `firmware/master_serial/` holds COPIES of the `protocol/cpp/` files (the Arduino IDE only compiles files inside
  the sketch folder), so re-copy after any change there; `mode_switch` is not in the sketch yet.
  Branch `serial-protocol` is still unmerged and untagged (when ready: `git checkout main && git merge --no-ff
serial-protocol && git tag serial-protocol-v1`).
- Pure-logic modules for the master sketch, all in protocol/cpp/, tested on the PC in test_protocol.cpp (no Arduino calls, UNTESTED ON HARDWARE): watchdog (command freshness, never-seen = stale, millis wrap safe), drive (drive_from_speed: speed to throttle DAC, reverse relay and brake from ONE threshold, so the old “small reverse drives forward” bug cannot happen; 1.5 m/s full scale and 0.05 stop band are PLACEHOLDERS), reverse_guard (stop, settle 500 ms PLACEHOLDER, switch relay, go; no encoders, so it waits a fixed time and cannot confirm the rover stopped), mode_switch (manual at or below 1400 us, auto at or above 1600, between means manual, 3 frames to change, stale = manual, numbers from the old ROS firmware; transmitter channel still unknown). Compile: g++ -Wall -o test_protocol test_protocol.cpp protocol.cpp watchdog.cpp drive.cpp reverse_guard.cpp mode_switch.cpp && ./test_protocol. For the real boards: the master sketch uses all of these except test_protocol.cpp; the sensor sketch only needs protocol.
- Pi UART options and pin choices are still PLACEHOLDER (see the Firmware approach decisions). Nothing here has
  touched real hardware.
- The ROS node wrapping `Bridge` should: publish `/lift_down` from `LiftState` on a 10 Hz timer (not per
  message), publish `/imu` and `/gps/fix` with stamps and covariances, subscribe `/cmd_vel` and convert
  Twist to a steering ANGLE (delta = atan(wheelbase * wz / v), not `wz * 180/pi`), and send `CMD` at a steady
  rate so the ESP command-timeout watchdog stays satisfied.
- Test habit: run `python3 -m pytest -q` only from inside `protocol/`; running it from `~/rover_ws` pulls in
  the failing `rover_basics` flake8 lint (976 style errors, unrelated, ignorable).
- ROS node rover_bringup/scripts/esp_bridge.py (tested on the PC with a socat cable and printf, UNTESTED ON HARDWARE): reads the serial link at 50 Hz, publishes /lift_down from LiftState on a 10 Hz timer (false when the ESP is silent) and /battery_voltage (Float32, placeholder name). Subscribes /cmd_vel and sends CMD at 20 Hz; steering comes from protocol/kinematics.py (atan(wheelbase * wz / v), positive = left, clamped to 30 deg PLACEHOLDER, 0 at or below 0.05 m/s); a /cmd_vel older than 0.5 s (PLACEHOLDER) sends speed 0. CMakeLists.txt installs bridge.py, protocol.py and kinematics.py from ../../protocol/ next to the node (an install(FILES ...) block above ament_package()); package.xml has python3-serial. Not yet done: MODE is not published, /imu and /gps/fix wait for IMU/GPS in the spec, esp_bridge is not in sim_nav (sim-only: it must not run in the sim) and not in a hardware launch file. Also still to add to SPEC.md: steer positive = left.

### Output tests (PLAN, written 2026-11-12; nothing below has been run)

Stage 1 (failsafe retest, no outputs enabled) is DONE, see the bench session section. Stages 2 to 7 need `ENABLE_OUTPUTS` set to 1 in the master's `FILL IN` block, one stage at a time, in this order (least risky first). Reflash after each change.

Before any stage: rover on blocks with the wheels clear of the ground; a known way to cut the battery quickly, with a hand near it whenever motor power is on; master on USB with the Pi unplugged; every actuator and motor-controller supply OFF until the stage says otherwise.

Pin states set in `setup()` when outputs are enabled (read from the sketch): brake driver L_EN (pin 14) and R_EN (pin 19) HIGH, brake LPWM (18) and RPWM (2) LOW, reverse relay (23) LOW, throttle DAC (25) 0, steering servo (33) centred at 1500 us, lift A and B (26, 27) both HIGH (stop). NOTE: GPIO 14 pulses briefly at boot (ESP32), and it feeds the brake driver's left enable, so the brake driver's supply must stay OFF for the first power-ups with outputs enabled.

Stages: 2. Outputs on, ALL motor, actuator and controller power off. Multimeter on each output pin: confirm the rest states above, and that the DAC pin follows the throttle stick (expected 0 to about 3.3 V; PLAN, not measured). 3. Reverse relay: it should click when the reverse button is pressed, after the 500 ms settle (PLACEHOLDER) in `reverse_guard`. 4. Steering servo: moves with the steering stick and centres when the transmitter is switched off. Check the direction: `STEER_AUTO_SIGN` (+1 = higher pulse = left) is UNKNOWN for the auto path. 5. Lift: up and down from the ch6 switch, stopped when the throttle is not 0 and when the transmitter is off. 6. Brake actuator, disconnected from the brake lever at first. Find which BTS7960 pin extends it (`BRAKE_EXTEND_PIN` is a GUESS), check the limit switch (`PIN_BRAKE_LIMIT`, LOW = pressed) stops the extension, and that the fixed 2000 ms retract is enough. 7. Throttle, wheels off the ground: multimeter on the throttle wire at full stick (does the DAC reach full throttle? 3.3 V vs a controller that may want about 4 V, unverified), then watch the wheels. Also check the old "controller current limit" question from the pre-ROS review.

Things to know before stage 2 (not yet known): the throttle controller and brake driver models (the sketch calls the brake driver a BTS7960), whether the brake and lift have separate supplies, where multimeter probes can reach, and whether there is an emergency cut-off.

### Bench session 2026-11-12 (first hardware contact; `ENABLE_OUTPUTS` still 0, nothing driven)

Master and sensor boards flashed over USB. Pulse widths below were read from the master's serial monitor (`# pulses us:` line).

Measured RC pulses (us):

- steer (ch1): full left 983, rest 1511, full right 2051.
- throttle/brake (ch2): full pull 1001 (more throttle = LOWER pulse), rest 1554, pushed away (brake) 2061.
- lift (ch6): down 983, rest 1515, up 2048.
- mode (ch5, master pin 22): three-way switch, down 982 / middle 1514 / up 2048. Matches the 1400/1600 thresholds unchanged (up = auto; middle and down = manual). Answers the open question: the pin 22 switch is ch5.
- reverse (ch7, master pin 34): button, about 979 off, 2045 held. The old `REVERSE_PULSE_MIN` 2000 left only 45 us of margin.

Receiver failsafe (receiver model given as RC6GS V3):

- With the transmitter OFF the receiver keeps sending centred pulses (steer 1515, thr 1514, lift 1514, rev 1512). The master therefore never sees "no signal", so the "stale = brake" path does not fire. At thr 1514 the DAC is about 2 and there is NO brake (the rover coasts). A receiver failsafe is required, not optional.
- Throttle channel failsafe set on the transmitter to the brake end (about 2060). RETESTED 2026-11-12 and it works: with the transmitter off, `thr` goes above 2000 and `mbrake` goes to 1 in under 1 s (receiver delay, not measured more precisely); switching back on restores `thr` to about 1554 and `mbrake` to 0; with the stick pulled at switch-off it still goes to the brake end; `auto` drops to 0 with the transmitter off (ch5 up -> dead zone). Note the delay: roughly up to a second of coasting before the brake request after a transmitter loss.
- Reverse (ch7) failsafe changed on the transmitter from the centred default (about 1512) to about 980 (the "not pressed" value), so a lost transmitter can never read as a press. Verified: `rev` goes to about 980 with the transmitter off and `rev` direction does not change.
- Mode (ch5) goes 2000 -> 1500 when the transmitter is off, which the dead zone reads as manual. A lost transmitter can never leave the rover in autonomous. Consequence: autonomous only runs while the transmitter is on and the switch is up.
- The other channels' centred failsafe is acceptable: lift stops, steering centres, reverse reads 1512 (below the threshold).
- PROPOSED constants from these numbers (confirm they are in the `FILL IN` block): `THROTTLE_PULSE_MIN` 1010, `THROTTLE_PULSE_MAX` 1500, `BRAKE_PULSE_MIN` 1900, `REVERSE_PULSE_MIN` 1700. 1500 would be wrong for reverse because the failsafe value 1512 would read as pressed.

Safety link (sensor -> master, 4 bytes at 10 Hz, one-way on pins 16/17, shared ground):

- Packet: magic 0xA5, sequence, flags (bit 0 danger, bit 1 warning), CRC-8 (poly 0x07, init 0) over sequence + flags. Code is in `protocol/cpp/safety_link.cpp/.h` (CRC, builder, byte-by-byte reader that resyncs on the magic byte, freshness wrapper reusing `watchdog`); copies live in both sketch folders. Tested on the PC (43 pytest tests plus the C++ suite pass).
- DECIDED: danger and warning HOLD until the next good packet changes them. "Stale" (no good packet in 350 ms, PLACEHOLDER from the old firmware, or none ever) shows only in the master's debug line (`stale=`) and does NOT raise danger. The sequence number is not checked yet (left out on purpose).
- DECIDED: danger stops and brakes AUTONOMOUS driving only. Manual driving ignores danger (so the rover cannot strand itself on a slope). Danger does not touch the reverse relay (the old firmware forced it off) and does not stop the lift.
- On hardware: after flashing the sensor board the master showed `stale=0`, so the link works. Baud 115200 is a PLACEHOLDER and must match in both sketches.
- Sensor sketch now sends `LIFT` at 10 Hz, `BAT` at 1 Hz, tilt from the MPU6050 at 50 Hz (a dead sensor for 350 ms = danger) and the safety packet. Master sketch now has steering (RC or Pi angle) and manual lift with a throttle interlock.

MPU6050 (sensor board):

- At first `mpu_ok=0` (so danger=1 after 350 ms, by design). It worked after its power and ground were reconnected, so that connection is marginal; check it.
- At rest it read roll -166.4, pitch -1.2: the board is mounted upside down about the X axis (GUESS from the numbers). Fixed with `MPU_SIGN_Y` = _**, `MPU_SIGN_Z` =**_ and level offsets roll _**, pitch**_ in the sketch's `FILL IN` block. Calibrated against the rover's resting position; confirm `danger=0 warn=0` at rest on the master's debug line, then check nose up/down and left/right symmetry.

Python side: `protocol.parse_mode` plus `protocol/test_mode.py` are done. `esp_bridge` does not publish `MODE` yet. PLAN: `/rover/autonomous`, `/rover/danger`, `/rover/warning`, `/rover/brake` (Bool) and `/rover/esp_link_ok` (false when no `MODE` for 0.5 s, PLACEHOLDER; a silent ESP also forces `autonomous` false).

Still UNTESTED ON HARDWARE: every output (throttle DAC, brake actuator and which BTS7960 pin extends it, steering servo, lift, reverse relay), the throttle multimeter test, and the Pi link. `DEBUG_PRINT` and the pulse line share the Pi's UART0: set `DEBUG_PRINT` to 0 before wiring the Pi.

### Master sketch progress (2026-11-08, branch serial-protocol, pushed; compile-only, UNTESTED ON HARDWARE)

- `firmware/master_serial/` holds COPIES of the `protocol/cpp/` files (the Arduino IDE only compiles files inside
  the sketch folder). Re-copy after any change in `protocol/cpp/`. Pure logic is tested on the PC; the .ino is not.
- Every uncertain value (pins, pulse ranges, thresholds) sits in one `FILL IN` block at the top of `master_serial.ino`.
  Pins are the pre-ROS ones from `globals.h`. `ENABLE_OUTPUTS` is 0: no motor pin is touched until it is set to 1.
- RC inputs: edge interrupts through `RcChannel` (`rc_channel.h`, kept in its own header because the Arduino IDE's
  auto-generated prototypes broke when the struct was in the .ino). One ISR, five channels (steer, throttle, lift,
  reverse, mode). Freshness check (100 ms) after the snapshot. `rc_clamp_map` saturates instead of returning 0, so
  the old range cliff is gone. Pulse ranges are the old placeholders; measure on the bench.
- DECIDED: manual throttle is at 100% (`MANUAL_THROTTLE_SCALE` 1.0). The old firmware capped it at 0.75. The DAC may
  still not reach full throttle (3.3 V vs a controller that may want ~4 V; unverified, multimeter test).
- Mode switch: `mode_switch` (dead zone 1401 to 1599 us now drops to manual immediately), stale = manual.
- Brake: new `brake` module (extend until the limit switch, retract for a fixed 2000 ms, brake always wins), tested on
  the PC. Manual brake request is ch2 pulse > 2000 us. PROPOSED, not confirmed: loss of RC signal = brake ON (the
  old firmware released it and coasted). The receiver's own failsafe must also be set; a receiver that keeps
  repeating the last pulse would never trigger "stale".
- Manual reverse: ch7 rising edge toggles direction (no `delay`, a held switch does not re-toggle). Manual and auto
  both go through the ONE `reverse_guard` (stop, settle 500 ms, flip relay). Direction survives a brake press.
- `MODE` is sent to the Pi at 10 Hz. `danger` and `warning` are hard-coded 0 until the sensor board is wired in:
  MUST be fixed before any real driving.
- Source choice: auto only if the switch says auto AND the Pi `CMD` is fresh (500 ms); otherwise manual.
- Not done in the sketch: steering servo output, lift, sensor-board heartbeat and danger, `DEBUG_PRINT` shares the
  Pi link (set to 0 before wiring the Pi to UART0), which BTS7960 pin extends the brake (`BRAKE_EXTEND_PIN` is a
  guess), pin 22's transmitter channel number, receiver voltage (pin 22 is 3.3 V only).
- Branch `serial-protocol` is still unmerged and untagged.

### Session log 2026-11-08 evening (firmware rebuild, pure logic + Pi node)

All of this is UNTESTED ON HARDWARE (no Pi or ESPs available). C++ is tested on the PC only.

DONE and tested on the PC (`cd ~/rover_ws/protocol/cpp && g++ -Wall -o test_protocol test_protocol.cpp protocol.cpp watchdog.cpp drive.cpp reverse_guard.cpp mode_switch.cpp && ./test_protocol | grep FAIL`; no output = no failures):

- `protocol.cpp/.h`: `protocol_make_line`, `protocol_make_lift`, `protocol_make_bat`, `protocol_make_mode`, `protocol_parse_cmd` (strict: CMD plus exactly 3 finite numbers, outputs untouched on failure).
- `watchdog` (command freshness, never-seen = stale, millis-wrap safe, 500 ms timeout PLACEHOLDER).
- `drive` (`drive_from_speed`: ONE threshold decides throttle DAC, reverse and brake together; 1.5 m/s full scale and 0.05 m/s stop band PLACEHOLDERS; fixes the old "small reverse drives forward" bug).
- `reverse_guard` (direction change = throttle 0 + brake, wait 500 ms PLACEHOLDER, flip relay, go; no encoders so it only waits a fixed time).
- `mode_switch`: manual <= 1400 us, auto >= 1600, DEAD ZONE 1401-1599 = MANUAL immediately (matches the old ROS firmware; I first wrote "hold the current mode", which was wrong and was corrected), 3 frames in a row to change, stale = manual. CHECK: the dead-zone change in `mode_switch.cpp` and the replaced mode test block in `test_protocol.cpp` may not have been applied or run yet. Run `git status` and the test.
- Python: `protocol/kinematics.py` `twist_to_steer_deg(v, wz, wheelbase=0.686, max_deg=30, stop_speed=0.05)` = atan(L*wz/v) in degrees, positive = LEFT, 0 at or below stop speed, clamps (30 deg PLACEHOLDER). `protocol/test_kinematics.py`. 27 pytest tests pass.
- ROS node `rover_bringup/scripts/esp_bridge.py` (tested with `socat` + `printf`): reads serial at 50 Hz, publishes `/lift_down` (10 Hz timer, false when ESP silent) and `/battery_voltage` (Float32, placeholder name); subscribes `/cmd_vel`, sends `CMD` at 20 Hz (steering from kinematics.py); `/cmd_vel` older than 0.5 s (PLACEHOLDER) sends speed 0. `CMakeLists.txt` has an `install(FILES ../../protocol/bridge.py ../../protocol/protocol.py ../../protocol/kinematics.py ...)` block ABOVE `ament_package()`; `package.xml` has `python3-serial`. Not published yet: MODE, `/imu`, `/gps/fix`. Not in any launch file (sim-only nodes must stay out of a hardware launch).
- First master sketch `firmware/master_serial/master_serial.ino` (+ copies of protocol, watchdog,

### Firmware files

- `SenserBoardRedone.ino` + `MasterBoardRedone.ino` (each with a `globals.h`): pre-ROS firmware, drove well manually.
- `ROSSensorBoard.ino` + `ROSMasterBoard.ino` (each with a `globals.h`): micro-ROS version. Does not work at all now; earlier it had inoperable lag, cause never found.
- Pre-ROS module files REVIEWED 2026-11-08 (see "Pre-ROS firmware review"): `gyro`, `warning_switch`, `bluetooth`, `battery_monitor`, `lift_lower_limit`, `lift_upper_limit` (sensor side; no `gps` file was among the uploads and GPS is never started) and `throttle_and_brakes`, `reverse`, `lift`, `turning` (master side).
- ROS versions reviewed too: see "Old micro-ROS firmware review" (module files `*_1`, plus `gps_sensor`).

### Findings from reading the sketches (GUESSES about the lag, nothing tested)

- micro-ROS entities use `rclc_..._init_default`, which is reliable QoS. Over a slow serial link that can stall waiting for acknowledgements; best-effort is the usual choice for sensor streams.
- The sensor board sent a full IMU message (three covariance arrays) plus GPS at 10 Hz over 115200 baud: about 4.5 KB/s of payload by my arithmetic, against about 11.5 KB/s link capacity, before micro-ROS framing. The baud rate can also be raised.
- Old pre-ROS master: `Serial.readStringUntil('\n')` in the main loop has a default 1 s timeout, so a half-received Pi line could freeze throttle and steering for up to a second. The new design must never block in the control loop.
- Old sensor-to-master packet: 10 raw bytes, no sync or checksum (one lost byte misaligns it for good). The ROS version added magic byte + sequence + CRC; keep that idea.
- ROS master `globals.h` already has a mode-switch design: `autonomousMode`, manual below 1400 us, autonomous above 1600 us, 3 confirmation frames, `autonomousCommandTimeout` 500 ms, `sensorHeartbeatTimeout` 350 ms. By the names, the input is probably pin 22 (`turnswitch`); GUESS, confirm which transmitter switch it is. Throttle `pwmMin`/`pwmMax` changed from 980/1517 (pre-ROS) to 900/2100 (ROS); not investigated.
- GPS pins are unresolved: the pre-ROS sketch comment says RX 3 / TX 1 (UART0); the pre-ROS `globals.h` says 16/17 (same pins as the sensor-to-master link); the ROS sensor `globals.h` says 16/17 "not sure these are right". The pre-ROS sketch never starts the GPS, so it settles nothing. Need the physical wiring.
- The ESP32 has 3 hardware UARTs, each mappable to almost any free pins. The sensor board needs 3 serial links (Pi, master, GPS), and USB flashing/debug shares UART0. Options: (a) Pi on UART0 (cost: unplug to flash, no debug prints), master link and GPS on UART1/UART2; (b) GPS and the master link share one UART at 9600 baud (the master link only uses the sensor's TX, the GPS only its RX, and the link is about 40 bytes/s), Pi on UART1, UART0 free for USB. Not decided; the first test does not need GPS.

### Open questions (firmware)

- Which transmitter channel is the manual/auto override. It lands on master pin 22 (`turnswitch`, > 1400 us = autonomous steering, per pre-ROS `turning.cpp`); the channel number is still unknown (not 1, 2, 6 or 7). SOLVED - ch 5
- Where the GPS wires physically land on the sensor ESP (unknown; no hardware access 2026-11-08). PLACEHOLDER for the rebuild: GPS RX = GPIO 32, TX = GPIO 33 on the sensor board (free there, not strapping pins, no clash with the 16/17 master link). When the rover is available, check the real wiring, then either move the wire or change the two pin defines to match. Both old firmwares put the GPS on 16/17, which clashes with the safety link.
- How many UARTs the Pi exposes (overlay names in `config.txt` NOT checked).
- Real pins: the pre-ROS pins are now known (see "Pre-ROS firmware review"); the ROS boards' pins are still unchecked.

### Original Proposal

- micro-ROS was tried before and had severe latency problems (cause unknown). Proposal: custom serial
  protocol, ESP32s run no ROS, and a Python bridge on the Pi (`pyserial`) translates to ROS topics.
- Link: RX/TX between each ESP and the Pi (ESPs are powered by the rover), shared ground, 3.3 V on both
  sides (no level shifting here; the HC-SR04 echo pins still need it). Use a UART other than UART0 on the
  ESP32 (UART0 is shared with USB flashing). The Pi needs two UARTs (overlay names in `config.txt` NOT
  checked), or two USB-serial adapters, or one ESP relaying to the other.
- Newline-terminated text lines with a checksum (`CMD,speed,steer`, `US,front_mid,1.82`, `LIFT,1`).
  Kinematics (Twist to steering angle) on the Pi; the ESP maps speed/steer to throttle, servo, brake and
  reverse following the rover's rules. Speed is open loop (no encoders).
- Watchdog on both sides: no `CMD` for ~250 ms (placeholder) means zero throttle and brake set; no sensor
  lines means the Pi treats the lift as not down.
- Build order: sensor ESP first (cannot move anything), commands and motors last with wheels off the ground.
- Latency estimate: 115200 baud is ~11.5 KB/s; the full message set is ~3.3 KB/s (my arithmetic).

## Pre-ROS firmware review (2026-11-08)

Read from source only: nothing compiled or run, so everything below is "read from code" unless it says GUESS or
"from memory" (library behavior I recalled, not checked). The upload renamed the sensor board's `globals.h/.cpp`
to `globals_1.*`; the master's are `globals.*`. This is the firmware that DRIVES today (manually), so it is the
reference for the rebuild, but treat it as fallible: several bugs below.

### Architecture

- Master ESP (`MasterBoardRedone.ino`): RC receiver in, motors out. `loop()` reads one sensor packet from Serial2
  (RX 16, TX 17, 115200), reads optional Pi text lines from USB `Serial`, then runs `throttle_and_brakes()`,
  `reverse()`, `lift()`, `turning()` in that order. Every RC channel is read with a blocking `pulseIn`.
- Sensor ESP (`SenserBoardRedone.ino`): `gyro()`, `warning_switch()`, `bluetooth()`, `battery()`, `lower_limit()`,
  `upper_limit()`, then every 100 ms sends a 10-byte packed struct (`bool danger, bool warning, double turnangle`)
  on Serial1 (RX 16, TX 17) to the master. No sync byte, no checksum, one-way, no heartbeat.
- AUTONOMY IS STEERING ONLY. Throttle, brake, reverse and lift always come from the RC. Pin 22 (`turnswitch`,
  pulse > 1400 us) picks autonomous steering (`turnangle`, +/-30 deg) or RC steering. There is NO path from a speed
  command to throttle, brake or reverse. The rebuild has to add one (open loop, no encoders).
- `turnangle` is never produced: the sensor board sends the default 0 (no GPS or waypoint code exists in these
  files; `TinyGPS++` is included but GPS is never started). The Pi path (`TURN:15.5`, `DANGER` over USB serial) is
  overwritten by the next sensor packet (~100 ms), so it cannot hold. GUESS: autonomous steering was never tested.

### Pin map (master, from `globals.h`; code assumed right because it drives)

| Pin        | Use                                                              |
| ---------- | ---------------------------------------------------------------- |
| 34 / 23    | reverse ch7 in / reverse relay out                               |
| 32 / 25    | throttle+brake ch2 in / DAC throttle out (DAC1)                  |
| 14, 19     | BTS7960 L_EN, R_EN (both just set HIGH)                          |
| 18 / 2     | BTS7960 LPWM / RPWM (brake actuator)                             |
| 13         | brake limit switch (pullup, LOW = pressed)                       |
| 4 / 26, 27 | lift ch6 in / lift outputs A, B (both HIGH = stop)               |
| 35 / 33    | steering ch1 in / steering servo out                             |
| 22         | manual/auto switch in (transmitter channel number still unknown) |
| 16, 17     | UART to sensor board                                             |

- Stale comments in `globals.h` say pin 15 = LPWM and pin 0 = RPWM and swap 14/19; `throttle_and_brakes.cpp` has an
  older 18/21 pair commented out. The 14/19 swap is harmless (same level on both).
- Sensor board: 21/22 I2C to MPU6050 (default Wire pins, not set in code), 34 battery ADC (divider scale
  39.8 / 2.39), 23 override switch (pullup, LOW = alarms overridden), 19 warning light, 13 lower lift limit,
  14 upper lift limit, 16/17 UART to master. Defined but unused: GPS 16/17 (clashes with the master link) and SD
  5/18/19/23 (clash with the light and the switch). Harmless only while no SD card or GPS is wired.

### Behavior

- Throttle: ch2 pulse 980 to 1517 us maps to DAC 255 down to 0 (pulling = LOWER pulse = more throttle). Normal mode
  multiplies by 0.75; the warning block uses the full range. Outside 980 to 1517 the throttle is 0.
- Brake: ch2 pulse > 2000 extends the actuator until the limit switch; otherwise it retracts for a fixed 2000 ms
  (timed, no feedback). The brake is applied only while the stick is held past 2000. The docs' "auto-brake when the
  throttle is off" is not in this code.
- Reverse: ch7 pulse > 2000 toggles the relay, then `delay(500)`.
- Lift: ch6 > 1800 = up (A high, B low), < 1400 = down, else stop. Only when the DAC value is 0 and there is no
  danger. No signal = stop.
- Steering: RC input mapped 1000 to 2000 -> 500 to 2500 us; no signal = 1500 (centered). Auto: +/-30 deg -> 500 to
  2500 us.
- Alarms are TILT ONLY: danger at roll >= 20 or pitch >= 25, warning at roll >= 15 or pitch >= 20. Danger blinks the
  light (300 ms), warning is solid, override switch forces both off. No ultrasonic or collision code exists here
  (`echo_*` and `min_distance` are stubs; Bluetooth `e` always prints 500).
- Roll/pitch use hard-coded offsets (roll assumes z points down; pitch has `+167` and `+13`). GUESS from the
  arithmetic: the board is mounted flipped with a ~13 deg pitch offset and the code is calibrated to that mount.
  Remounting the board or swapping the MPU would make pitch wrong. Level should read ~0 on Bluetooth `g`; check it.
- Bluetooth "Rover": send `e` (distance), `g` (roll/pitch), `d` (danger/warning), `b` (battery string). The last
  character is kept, so the reply repeats every loop until another character is sent. Lift limit presses are
  announced once per press.

### Problems found (ranked; none tested, bench test with the wheels off the ground)

Safety:

1. Danger does not cleanly cut throttle. `warning_switch` sets BOTH flags on danger, and the `if (robot_warning)`
   block in `throttle_and_brakes()` is a separate `if` that runs after the danger block, so it re-reads the stick
   and writes the full-range throttle. It most likely still stops only because `reverse()`'s danger branch writes 0
   again later in the same loop, so the DAC briefly shows the stick value every loop. Fragile. Related oddity:
   normal mode caps at 75% but warning mode (tilted past 15/20 deg) gives 100%. Intent unknown (possibly deliberate
   extra power on slopes).
2. Loss of RC signal: throttle goes to 0 (good) but the brake is RELEASED (pulse 0 is not > 2000, so it retracts).
   The docs say "stop on loss of radio contact"; this coasts.
3. No heartbeat from the sensor board. If it dies or the cable drops, the master keeps the last danger/warning
   forever. Same inside the sensor board: if the MPU6050 stops answering, `gyro()` does nothing and the flags freeze.
4. Reverse relay toggles at any speed (no interlock), and `delay(500)` freezes throttle, steering and lift for
   half a second (the DAC and servo hold their last value). A held switch re-toggles every ~0.5 s.
5. Lift limit switches are monitor-only: they sit on the sensor board and only send a Bluetooth message on the
   press edge. Nothing stops the lift motor and the master never sees them. The docs' lift interlock (alarms off,
   brake set) is only partly there (no throttle, no danger). Also for the ROS plan: `/lift_down` needs the switch
   LEVEL (pressed or not) sent continuously, not a one-time event.
6. Pi commands are overwritten every ~100 ms by the sensor packet (see Architecture).

Timing: 7. Master loop blocks on up to 5 to 6 `pulseIn` calls (20 to 25 ms timeouts) plus `readStringUntil` (1 s default
timeout) plus the 500 ms reverse delay. Estimate (not measured): ~50 to 120 ms per loop, so throttle and steering
refresh at roughly 10 Hz. 8. Sensor loop: `gyro()` calls `mpu.begin()` EVERY loop. From memory of the Adafruit library, `begin()` resets the
chip, sleeps about 100 ms or more, and restores default ranges, so the +/-8 G and 5 Hz filter set in `gyro_setup()`
are lost after the first loop. Add `delay(50)` in each limit function. Likely 300 ms or more per loop, so the tilt
alarm reaches the master at ~2 to 3 Hz. CHECK: the sensor's serial monitor should print `Slow Loop Detected`
constantly if this is right. 9. Packet framing: no sync or CRC, so one lost byte misaligns the link permanently (already noted above).

Correctness: 10. Throttle range cliff: a pulse outside 980 to 1517 gives 0 instead of clamping, so pulling the stick slightly past
980 drops the throttle to zero. GUESS: this is why the ROS firmware widened it to 900/2100. The brake threshold
(> 2000) sits at the edge of a normal 1000 to 2000 channel and works only because this transmitter exceeds 2000
(unmeasured). 11. Steering range: `steering.attach(servopin, 1000, 2000)` but pulses of 500 to 2500 are written. FROM MEMORY,
ESP32Servo clamps `writeMicroseconds` to the attach limits (check the library source). If so, RC steering
saturates at stick 1250/1750 (2x gain) and autonomous saturates at +/-15 deg, not +/-30. Fix, if the servo
tolerates it: `attach(servopin, 500, 2500)`. `turnangle` is degrees mapped linearly to pulse width; it is not
calibrated to real wheel angle (the real max steer angle is still unmeasured; this is how to measure it). 12. `Wire.begin()` on the master claims pins 21/22 while pin 22 is the mode switch. The master has no I2C devices;
remove it. It works only because `turning_setup()` reconfigures pin 22 afterwards. 13. Dead or stale code: `bluetooth.cpp` checks `lcurrentState`/`ucurrentState` globals that nothing updates (the
real limit messages come from `lift_lower_limit.cpp`/`lift_upper_limit.cpp`, which shadow them with locals);
`isOn` is `int` on the master and `bool` on the sensor board (unused); `movingmode` is always 0; `lastBTPrint`
unused. GPIO 2 (RPWM) is a strapping pin and GPIO 14 pulses at boot (GUESS: check for a brake twitch at power-up).
The relay forgets its state on a reset (comes up forward).

### Carry over to the rebuild

- Keep: the RC pin map, the override switch living on the master, tilt alarms, the Bluetooth debug idea, the 10-byte
  idea (but with magic byte, sequence and CRC), and the brake actuator logic (extend to limit switch).
- Replace `pulseIn` with edge-timestamp reads (interrupt or capture) so no RC read ever blocks the loop.
- Clamp RC pulses; do not zero them. Measure and record each channel's real min, neutral and max first.
- Fail-safe on signal loss, Pi silence or sensor silence: throttle 0 AND brake applied.
- Heartbeat in both directions between the boards and from the Pi; a missing sensor packet means "not safe".
- Reverse only at zero throttle, and no `delay` anywhere in the loop.
- Send limit-switch LEVELS (lift down, lift up) so the Pi can publish `/lift_down`.
- Do not mix debug prints with the Pi link on UART0 (the Pi would read them as data).
- A speed-to-throttle map is new work (open loop); it needs calibration against real speed.

### Torque is low (side note, cause not confirmed; motor controller and motor models unknown)

1. Software cap (read from code): normal mode multiplies the DAC by 0.75, so the throttle never goes past about
   191/255, roughly 2.5 V on a 3.3 V DAC. Test: raise or remove the 0.75.
2. DAC ceiling (GUESS): the ESP32 DAC tops out near 3.3 V (a bit under in practice) and is a weak source. A hall-type
   e-bike throttle controller commonly expects up to ~4 to 4.2 V at full throttle (unverified; the "green wire"
   comment suggests that style). So even 100% may be partial throttle. Test: multimeter on the throttle wire at full
   stick, compared with the voltage the controller's own throttle gives. Fix: buffer and amplify the DAC with a
   rail-to-rail op-amp on 5 V (gain ~1.5), or use an external DAC that runs on 5 V.
3. Range cliff (item 10): holding full pull past 980 us drops the throttle to 0.
4. Hardware, not visible from code: controller current limit and low-speed or eco settings, battery sag under load
   (the sensor board's `b` command shows voltage; watch it while accelerating), gearing, and the large wheel
   radius (0.159 m, so ground torque is low for the same motor torque). Torque follows current, so the
   controller's current limit is the first thing to check.

### Bench tests to confirm the above (when the rover is available, wheels off the ground)

- Sensor serial monitor: is `Slow Loop Detected` printing constantly? (item 8)
- Bluetooth `g` with the rover level, nose up, nose down: does level read ~0 and are both directions symmetric?
- Force danger (tilt past 25 deg or fake the flag) and watch the throttle pin with a multimeter or scope. (item 1)
- Turn the transmitter off: does the brake release? (item 2)
- Multimeter on the throttle wire at full stick, with and without the 0.75. (torque 1 and 2)
- Print raw pulse widths for every channel at min, neutral and max. (items 10, 11)

## Old micro-ROS firmware review (2026-11-08)

Read from source only (nothing compiled or run). Labels: "read from code", GUESS, or "from memory" (library
behavior I recalled, not checked). Upload names: master `globals_281233.h` / `globals_9925e1.cpp` (control state),
sensor `globals_f7cd16.h` / `globals_511bd6.cpp`, module files with a `_1` suffix, and `gps_sensor.cpp` (sensor side
only; the pre-ROS firmware had no GPS module). This firmware "does not work at all"; the likely reasons are first.

### Architecture

- Same two-board split as pre-ROS, plus micro-ROS. Each ESP talks micro-ROS over its UART0 (RX0/TX0,
  `set_microros_transports()`, 115200 from memory of the library default): master -> Pi UART0, sensor -> Pi UART2
  (from code comments; Pi overlay names still unchecked). The Pi therefore needs two agents.
- The master<->sensor safety link stays as a hardware backup on pins 16/17: 4-byte packet (magic 0xA5, sequence,
  flags, CRC-8 poly 0x07 over sequence+flags) at 10 Hz. The master resyncs on the magic byte, checks the CRC, and
  treats a packet older than 350 ms as danger. The sequence byte is sent but never checked.
- Control runs in `loop()`; ROS runs in a FreeRTOS task (`rosTask`, core 0, 8 KB stack, priority 1). The master
  loop is non-blocking and times itself (`loop`, `avg`, `max` in us in the debug string; avg is since boot, max never
  resets). The loop cannot be stalled by ROS, so the old "lag" was most likely in the topic path (see below).
- RC channels are read by edge interrupts (pulse accepted if 500 to 2500 us) with a 100 ms freshness check;
  stale counts as 0. This replaces `pulseIn`. Caveat (GUESS, receiver model unknown): many receivers keep outputting
  the last or a preset value on radio loss instead of going silent, so "stale" may never trigger. Set the receiver's
  failsafe (throttle to the brake position) as well.

### ROS interface

- Master subscribes `cmd_vel` (Twist): `linear.x` -> speed (m/s), `angular.z * 180/pi` -> turn angle in degrees,
  `linear.z` -> lift speed (a hack). Subscribes `rover/danger` (Bool, stays latched until the next message).
  Publishes `rover/master_debug` (String, 4 Hz: pulse widths, danger/warning, loop times).
- Sensor publishes `imu` (Imu), `rover/gps` (NavSatFix), `rover/battery` (Float32), all 10 Hz, plus
  `rover/sensor_debug` (4 Hz). All reliable QoS (`..._init_default`).
- NOT published: lift limit state (`/lift_down` does not exist), manual vs autonomous mode, brake state,
  ultrasonics. Limit switches are still Bluetooth text only (name now "Rover_Sensors", replies are one-shot,
  `SerialBT.connected()` guard, non-blocking debounce).

### Control logic (master, `getControlState()` in `globals_9925e1.cpp`)

- Mode from pin 22: <= 1400 us manual, >= 1600 autonomous, in between = manual; 3 consecutive new frames to switch;
  stale = manual. This confirms pin 22 is the manual/auto switch (transmitter channel number still unknown).
- `danger` = ROS danger OR sensor danger OR no sensor packet ever seen OR packet older than 350 ms. In danger:
  DAC 0, brake motor stopped IN PLACE (neither applied nor released), reverse relay off, lift stopped.
  MANUAL DRIVING NEEDS THE SENSOR BOARD'S HEARTBEAT: no packets means no throttle in either mode.
- Autonomous = `autonomousMode` AND a ROS command newer than 500 ms. With no fresh command the throttle, steering and
  lift fall back to the MANUAL path, but `forwardSpeed` is forced to 0, which keeps the brake applied (auto-stop).
- Autonomous throttle: DAC = map(|speed| * 100, 0..150 -> 0..255), i.e. 1.5 m/s assumed full scale (never measured).
  No 0.75 cap. Brake is applied when |speed| <= 0.05 or the stick is past 1850 us; the stick brake always beats ROS.
- Autonomous reverse: relay HIGH when speed < -0.1. Autonomous lift: `linear.z` > 0.1 up, < -0.1 down.
- Lost agent (master): `rmw_uros_ping_agent` every 250 ms; a failed ping calls `setRosCommand(0,0,0)`, which also
  refreshes the command timestamp, so autonomous mode holds zero speed with the brake on (good).
- Kept on purpose: manual throttle x 0.75 when not in warning (comment: "Match the standalone controller's normal
  throttle calibration"), full range in warning. So the pre-ROS 75%/100% split is a design choice, not a bug. The
  torque side note (item 1) stands.
- Sensor `warning_switch.cpp` has `robot_danger = false; // TEMPORARY OVERRIDE` for tilt danger, so tilt can now
  only warn, and warning gives FULL throttle. GUESS: danger at 20/25 deg stopped the rover on real slopes.
- Steering: `attach(1000, 2000)` and all writes are now 1000 to 2000, so no clamp mismatch. RC input is 1:1.
  If my pre-ROS clamp theory holds, the old firmware had 2x gain, so steering feel differs between versions.

### Fixed vs pre-ROS

Non-blocking interrupt RC reads with freshness; framed, CRC'd safety packet with a heartbeat timeout; no `delay()` in
control; reverse relay uses an edge detect plus 500 ms lockout instead of `delay(500)`; relay forced off on danger,
brake request or lost reverse signal; mode switch has hysteresis and confirmation; auto-brake on zero speed; MPU
`begin()` only on (re)connect (the per-loop reset is gone); Bluetooth replies one-shot; sensor-side agent reconnect.

### Why it probably does not work (most likely first)

1. GPS pin clash (sensor board). `Serial1` (link to master) and `gpsSerial` (UART2, `RXD2` 16 / `TXD2` 17, "not sure
   these are right") are both mapped to pins 16/17. `gps_setup()` runs after `Serial1.begin()`, and a pad can have
   only one output signal, so GPS UART2's TX probably takes pin 17 and the safety packets never leave the sensor
   board. The master then sees no heartbeat, `danger` stays true, and nothing moves. GUESS from how the ESP32 pin
   matrix works. CHECK: `rover/master_debug` (or a scope on pin 17) should show `danger=1` constantly. Fix: GPS on
   free pins (needs the GPS TX wire moved, so check the physical wiring first).
2. Master gives up on ROS at boot. `initializeRos()` runs once in `setup()`; if the agent is not up it fails (from
   memory it can block for seconds, during which `loop()` does not run, so manual driving is dead too), `rosReady`
   stays false, and `rosTask` is never created. The Pi boots much slower than the ESP, so this is the normal case.
3. No reconnect on the master (the sensor board has one). After an agent restart the session is gone and the
   subscriptions are dead until the ESP is reset; autonomous then sits at zero speed with the brake on.
4. Sensor `create_entities()` returns early on a partial failure without cleaning up, so each retry (1 s) can leak
   micro-ROS entities until the static pool is exhausted (GUESS, from memory of the pool limits).
5. The master has no debug output except through ROS (no `Serial` prints, Bluetooth, or spare UART), so when ROS is
   dead there is nothing to look at. The old "inoperable lag" (cause never found) candidates, all GUESSES: reliable
   QoS publishes blocking on a full stream over a 115200 UART, ~4.5 KB/s of Imu+GPS over one UART0, 4 Hz debug strings.
   (A related risk: UART0 is also USB, so any stray print corrupts the micro-ROS stream.)

### Other problems (read from code, none tested)

Safety and behavior: 6. Unit bug: `angular.z` is a yaw RATE in rad/s but is converted to a steering ANGLE with `* 180/pi`. wz 1.0 gives
57 deg, clamped to 30, so it saturates at wz ~0.52. The Pi must send a real steering angle: delta =
atan(wheelbase * wz / v), as the plan already says (kinematics on the Pi). Sign is also unchecked: ROS +wz is left
and +turn gives a higher pulse; whether a higher pulse is left or right on this servo is unverified (GUESS: inverted). 7. Throttle map discontinuity (manual, arithmetic from code, with the 3.3 V DAC): `pwmMin/pwmMax` are 900/2100 but
neutral is a +/-60 us dead band around 1500. Neutral does NOT map to 0, so the DAC steps from 0 to ~105 (1.4 V)
at 1440 us and rises to 191 (2.5 V) at 900 us (x0.75 mode). On the brake side, 1560 to 1850 us (before the brake
threshold) still outputs 86 down to 40 (1.1 V to 0.5 V). This answers the earlier "900/2100, not investigated" note.
Measure the real channel before fixing. 8. Autonomous speed range: Nav2 `vx_max` 0.65 gives DAC ~110 (1.4 V), under half scale, and below ~0.3 m/s the voltage
is probably under the controller's activation point (GUESS ~1 V, unmeasured). So autonomous torque and speed will
feel even lower than manual (see the torque side note). Open loop, so it needs a speed-vs-DAC calibration. 9. Direction changes: the relay flips while the DAC is still non-zero (no stop, settle, switch, go sequence), and
speeds between -0.1 and -0.05 give relay forward but throttle > 0, so small backward commands drive FORWARD.
Smac with Reeds-Shepp will command reversals constantly. 10. Autonomous brake release: when speed goes from ~0 to > 0.05, the DAC is applied the same moment the brake starts
its fixed 2 s retract (the throttle fights the brake). Pre-ROS could not do this (one stick). 11. Pressing the brake while in manual reverse forces the relay off, so the NEXT throttle goes forward (state lost). 12. Lift regressions: `safeLift = fresh ? lift : 0` and 0 < 1400 means a stale lift channel drives the lift DOWN
continuously (pre-ROS stopped), and the "no throttle" lift interlock from pre-ROS is gone. Limit switches still
do not stop the lift. 13. A latent race in `getRcInputState()`: `now = micros()` is read BEFORE the interrupt snapshot, so an ISR in that
gap gives `now - at` a huge unsigned value and one false "stale". Rare (GUESS: seconds to minutes), but a false
stale on the reverse channel turns the relay off. Fix: take `now` after the snapshot. `getControlState()` also
keeps static mode state and is called from both cores without a lock.

Sensor data for the Pi (these will bite the EKF): 14. IMU: no `header.stamp` and no `frame_id`, so the Pi bridge must stamp it. Covariances are all zero except
`orientation_covariance[8] = -1` (REP-145 says element 0 = -1 for "no orientation"). The orientation is built from
`Roll_Angle`/`Pitch_Angle`, which are unsigned, integer, offset-hacked tilt values, so it is wrong in sign and
quantized: do not fuse it, fuse yaw rate only. GUESS: the pre-ROS roll arithmetic implies the board is mounted z-down,
so rest `acceleration_z` is about -9.8 and `yaw_rate` is sign-flipped relative to ROS (z up). CHECK the raw values
at rest. Also 10 Hz here vs 100 Hz in the sim, with the MPU's 5 Hz filter on. 15. GPS: the same fix is re-published at 10 Hz (the GPS only updates at ~1 Hz by default), so the EKF would count
repeats as independent. No timestamp. Covariance is `hdop^2`, but HDOP is dimensionless, not meters (real error is
roughly HDOP x ~2.5 to 5 m, GUESS). `gps.location.isValid()` may stay true after a fix is lost (from memory, check
`age()`). The topic is `rover/gps`; the sim EKF expects `gps/fix`. 16. `gyro()`: `getEvent()` (from memory) always returns true, so an unplugged MPU is never detected and the tilt
flags freeze. `battery()` builds a `String` every loop (kHz), so heap churn on a long run (GUESS).

### Carry over / rebuild notes (new ones; the pre-ROS list still applies)

- Keep: ISR RC reads with freshness, the framed CRC'd safety packet plus heartbeat, mode hysteresis and confirmation,
  brake-always-wins, ROS-loss => zero speed plus brake, control independent of the comms task, loop-time
  instrumentation. These work as designs and do not depend on micro-ROS, so they carry straight into the plain-serial plan.
- Drop: the rcl/rclc stack, the agents, and the two-UART-to-Pi layout (still undecided how the Pi links: see the
  UART options above).
- Add: publish mode (manual/auto), danger, brake state and limit-switch LEVELS (`/lift_down`); timestamps, frame_ids
  and real covariances on the Pi; steering-angle kinematics on the Pi; a reverse state machine; speed-to-DAC
  calibration; a debug channel that works when the Pi link is down; GPS on pins that do not clash with the link.
- Decide: should manual driving really require the sensor heartbeat? (It does here. If the sensor board dies you
  cannot drive, which is safe but also means no recovery by hand.)
- Open: where the GPS wires physically land on the sensor ESP; which transmitter channel is the pin 22 switch; the
  receiver's failsafe settings.

## Next steps (in order)

1. Add realistic IMU noise in the sim. On hardware: a real heading source for the map filter, drop
   `gps_covariance.py` and the `gps/fix_cov` remap (a real GPS driver reports its own covariance), and
   measure rf2o's real error to replace the placeholder variances in `odom_covariance.py`.
2. Nav2 leftovers (the ultrasonic range layer is done): real max steer angle for
   `minimum_turning_radius`, behavior when `/scan_filtered` goes silent, and Pi CPU check (MPPI, Smac's ~29 s
   configure).
3. Firmware and hardware link. DONE on the PC (all UNTESTED ON HARDWARE): serial protocol (Python + C++),
   C++ builders and `CMD` parser, pure-logic modules (`watchdog`, `drive`, `reverse_guard`, `mode_switch`),
   `kinematics.py`, the `esp_bridge` ROS node (both directions), and a first master sketch
   (`firmware/master_serial`, no motors). Remaining, in order:
   a. RC reads, mode switch, throttle, brake, manual reverse, and MODE are done, and steering, lift, and sensor danger remain
   b. Sensor sketch: `LIFT` (limit-switch levels), `BAT`, tilt, same `FILL IN` block.
   c. Pi side: publish `MODE`, add `IMU`/`GPS`/`DANGER` to the spec, a hardware launch file (without the
   sim-only nodes), then the bench checklist in "Pre-ROS firmware review".
4. Stuck detector and recovery (needs a second signal besides rf2o speed), the remaining roaming abort
   causes, and a retest of the inflation sliders.

- New chat: paste DEVLOG.md first, plus the file being edited. For the firmware also paste `protocol/SPEC.md`
  and `protocol/cpp/protocol.h`.

## Ideas for later (not committed to)

- SLAM works in the sim. Open question: how it coexists with the GPS EKF (both want `map -> odom`), and
  whether a 2D LIDAR maps well on rough forest terrain. Keep the lift down.
- Portfolio: Gazebo can't run in a browser. Use screen-recorded video, a README with an architecture
  diagram and test results, and the repo link. Decide later whether to make the repo public.
