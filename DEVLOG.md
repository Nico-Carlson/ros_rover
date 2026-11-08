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
| Item | Value | Status |
|---|---|---|
| Wheelbase | 0.686 | measured |
| Track width | 0.559 | measured (overlaps chassis width, recheck) |
| Wheel radius / width | 0.15875 / 0.1016 | measured |
| Chassis L x W x H | 1.118 x 0.562 x 0.457 | measured |
| Ultrasonic mounting height | 3 in (0.0762 m) above the top of the wheels | measured |
| Total mass | ~30 kg | PLACEHOLDER (xacro `chassis_mass` is 24.0 kg; the rest of the model's mass was not summed, check) |
| Ground clearance | 0.08 | PLACEHOLDER |
| Max steer angle | 0.7 rad | PLACEHOLDER |
| Lift platform mass / size / speed | 5 kg / 0.4 x 0.4 m / 0.05 m/s | PLACEHOLDER |
| Ultrasonic positions, angles, cone | front, rear, 4 corners at 45 deg (`us_side_y` 0.28, `us_angle` pi/4); see xacro | PLACEHOLDER |
| LIDAR / GPS / IMU positions | see xacro (GPS flat on the chassis) | PLACEHOLDER |
| Nav2 MPPI speeds (`vx_max` 0.65, `vx_min` -0.45, `wz_max` 1.0) | `nav2_params.yaml` | PLACEHOLDER (untested guesses) |
| `minimum_turning_radius` | 0.8 (both MPPI and Smac) | PLACEHOLDER (derived from the placeholder steer angle: 0.686 / tan(0.7) = ~0.81) |
| Goal tolerances | `xy_goal_tolerance` 0.5, `yaw_goal_tolerance` 3.14 (was 1.5) | tuned in the sim only |
| Costmap `inflation_radius` / `cost_scaling_factor` | 1.5 / 1.0 | PLACEHOLDER (from the old project) |
| Scan gate `lift_timeout` | 1.0 s | PLACEHOLDER |
| `sim_nav` start delays | 10, 15, 22, 25, 26, 30, 30 s | PLACEHOLDER (guesses) |
| Nav2 footprint | [0.902, +-0.33] to [-0.216, +-0.33] | derived from the xacro; depends on the unverified track width |
| rf2o gate limits (`odom_covariance.py`) | 1.5 m/s, 2.0 rad/s | PLACEHOLDER (generous guesses) |
| Extra sim trees | `tree_5` to `tree_26` | PLACEHOLDER positions |
| Range layer (`phi`, thresholds, `clear_on_max_reading`) | 1.2, 0.2 / 0.8, true | from memory, not tuned |

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
- [ ] Boundary roaming + stuck recovery (roaming, control panel and goal watchdog done; stuck detector and
      recovery NOT built, see "Roaming and stuck recovery")
- [x] Control panel with parameter sliders (see "Control panel")
- [x] Ultrasonic range layer in both Nav2 costmaps (sim converter `us_to_range.py`, see "Ultrasonics in Nav2")
- [x] rf2o speed gate in `odom_covariance.py` and 22 extra sim trees (see "Localization failures and fixes")
- [x] Canvas slab only while drawing; `/patrol_status` topic from roam (see "Canvas, status topic and RViz config")
- [ ] Show `/patrol_status` on the control panel (next, small)
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
  + yaw rate (sim only, see GPS section) and `/odom_rf2o_cov` forward speed + yaw rate. Map filter: GPS x/y as
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

## Roaming and stuck recovery (branch roaming-and-recovery, IN PROGRESS, not merged)
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
  >= 1 visit it targets the least-visited cell, in short hops), and `RoamNode` (reads `map -> base_footprint`
  from TF, sends `NavigateToPose` goals, a 1 s timer marks cells and sends the next goal when `busy` is
  False). Offline test: 26 cells, 23 visited after 60 hops. Run with
  `python3 ~/rover_ws/src/rover_bringup/scripts/roam.py --ros-args -p use_sim_time:=true` (needs
  `use_sim_time` or timestamps do not match the sim).
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
- `/patrol_status` exists (see "Canvas, status topic and RViz config") but the panel does not show it yet. NEXT: add a
  label that subscribes to it.
- Not done: patrol status label on the panel, reading real values at startup, tabs if it gets too tall.

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
- `rover_bringup/package.xml` was never checked: it probably lacks `exec_depend` entries for
  `rover_description`, `nav2_bringup`, `robot_localization`, `slam_toolbox` and the nav2 packages the launches
  use. A clean build on the Pi could fail without them.
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
3. Clean build: `rover_bringup/package.xml` probably lacks `exec_depend` entries (see Open questions).
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

## Next steps (in order)
1. Add realistic IMU noise in the sim. On hardware: a real heading source for the map filter, drop
   `gps_covariance.py` and the `gps/fix_cov` remap (a real GPS driver reports its own covariance), and
   measure rf2o's real error to replace the placeholder variances in `odom_covariance.py`.
2. Nav2 leftovers (the ultrasonic range layer is done): real max steer angle for
   `minimum_turning_radius`, behavior when `/scan_filtered` goes silent, and Pi CPU check (MPPI, Smac's ~29 s
   configure).
3. Panel status label (subscribe to /patrol_status), stuck detector and recovery (needs a second signal besides rf2o speed), the remaining abort causes and a retest of the inflation sliders, then the ESP32 firmware and Pi bridge.
- New chat: paste DEVLOG.md first, plus the file being edited.

## Ideas for later (not committed to)
- SLAM works in the sim. Open question: how it coexists with the GPS EKF (both want `map -> odom`), and
  whether a 2D LIDAR maps well on rough forest terrain. Keep the lift down.
- Portfolio: Gazebo can't run in a browser. Use screen-recorded video, a README with an architecture
  diagram and test results, and the repo link. Decide later whether to make the repo public.
