# Rover Notes (quick reference)

Current state of the project for people working on it. For the full history and step-by-step
instructions (and what to paste into a new chat), see `DEVLOG.md`. For the public overview, see `README.md`.

## What this is
Autonomous Ackermann rover for forest navigation (data collection for another school's camera project).
Modes: (1) navigate to a GPS pin avoiding obstacles, (2) roam inside a boundary, (3) get unstuck.
Mapping is secondary. Everything below is tested in the Gazebo sim only; no hardware code exists yet.

## Stack
- ROS 2 Jazzy + Gazebo Harmonic, Ubuntu 24.04 in a distrobox on Arch (Hyprland)
- Hardware (planned): Pi 5, 2x ESP32 (Arduino IDE, C++), RPLIDAR S2, GPS, IMU, ultrasonics (removed from
  the real rover for now; simulated)
- Pi code in Python, firmware in C++
- Parts (from listing titles, specs from memory, NOT verified): HC-SR04 ultrasonics (5 V echo needs level
  shifting for the ESP32), NEO-6M GPS (~2.5 m accuracy), MPU-6050 IMU (NO magnetometer, so no heading),
  DIHOOL scissor lift (~0.83 m travel listed; the xacro says 1.2192 m, reconcile)
- The real rover has NO wheel encoders. Wheel odometry exists only in the sim and is not fused.

## Status
Done in the sim:
- Robot model, Gazebo sim, obstacle world, simulated ultrasonics, SLAM (separate mode)
- Dual EKF: odom filter (IMU + rf2o) and map filter (adds noisy GPS via navsat_transform)
- rf2o LIDAR odometry, scan gating (lift down only), Nav2 (Smac Hybrid + MPPI, rolling costmaps)
- Boundary drawing, roaming, goal watchdog, control panel with parameter sliders

Open:
- Stuck detector + recovery (NOT built; the branch is not merged until it is)
- Patrol state shown in the panel; sliders reading real values at startup
- Ultrasonic range layer for Nav2 (LIDAR is blind below ~0.57 m)
- Behavior when `/scan_filtered` goes silent (Nav2 would look like "no obstacles")
- Real heading source for GPS fusion on hardware (the sim IMU is a perfect compass)
- Realistic sim IMU noise; tune covariances against real data
- Real measurements, ESP32 firmware (with command-timeout watchdog), Pi bridge, field testing
- `rover_bringup/package.xml` probably lacks `exec_depend` entries (clean build on the Pi may fail)

## Run it (sim)
```
cd ~/rover_ws
colcon build --packages-select rover_bringup     # after ANY edit; launch uses the installed copy
ros2 launch rover_bringup sim_nav.launch.py
```
One command starts: sim (0 s), rf2o + scan gate (10 s), both EKFs (15 s), Nav2 (22 s), RViz (25 s),
boundary canvas (26 s), roam (30 s), control panel (30 s). All delays are guesses. Nav2 needs ~30 s more
to become active ("Managed nodes are active").

- Point to point: RViz "2D Goal Pose".
- Manual driving only when Nav2 has no goal: `ros2 run teleop_twist_keyboard teleop_twist_keyboard`
  (avoid `j`/`l`; `k` stops).
- Stop: Ctrl+C in the launch terminal. Closing the Gazebo window does NOT stop the sim. If stuck:
  `pkill -9 -f "gz sim"; pkill -9 -f parameter_bridge; pkill -9 -f robot_state_publisher; pkill -9 -f ekf_node`
- Start fresh before any test comparing positions (EKFs start at (0, 0)).
- Log to a file when the output scrolls too fast:
  `ros2 launch rover_bringup sim_nav.launch.py 2>&1 | tee ~/sim_nav.log`, then
  `grep -i "abort\|fail\|occupied\|progress" ~/sim_nav.log | tail -20`
- `slam.launch.py` is NOT part of `sim_nav` and conflicts with the EKF (both publish `map -> odom`).

## Control panel and patrol
- Panel (PyQt5) opens with `sim_nav`. Buttons: New Boundary, Start Patrol, Stop Patrol. Mode toggle
  (Point to point / Boundary) only greys out those buttons; it does not stop a running patrol.
- Draw a boundary: press New Boundary (stops patrol, clears the old shape), set RViz Fixed Frame to `map`,
  use "Publish Point" for 3+ corners, click within 3 m of the first point to close. It saves
  `config/boundary.yaml` (git-ignored) and announces `/boundary_closed`; roam reloads it.
  Wait for roam to print `Loaded boundary with N corners`, then press Start Patrol.
- Roam starts without a boundary file (warns); Start Patrol answers "No boundary yet" until one exists.
- Services: `/start_patrol`, `/stop_patrol` (Trigger, on `roam`), `/clear_boundary` (Empty, on
  `boundary_canvas`). Topic: `/boundary_closed`. Markers: `/roam_markers` (boundary line, goal ball).
- Rover should be inside or near the polygon at start, or roam prints `No goal found inside the boundary`.
- Goal watchdog: `roam` parameter `goal_timeout` (90 s sim time, placeholder) cancels a goal that runs too
  long. A goal that is never accepted is not covered.
- Sliders set a DOUBLE parameter via the node's `set_parameters` service when released. Roam:
  `goal_timeout`. controller_server: `FollowPath.vx_max`, `FollowPath.wz_max`,
  `general_goal_checker.yaw_goal_tolerance`, `general_goal_checker.xy_goal_tolerance`. Local and global
  costmap: `inflation_layer.inflation_radius`, `inflation_layer.cost_scaling_factor`.
- Slider limits: they start at values hard-coded in `control_panel.py`, not read from the nodes. Changes
  last only until relaunch: copy keepers into `nav2_params.yaml`. A "(rejected)" label means the node
  refused the value (often an integer-typed parameter or one that cannot change at runtime).
- `FollowPath.CostCritic.cost_weight` (3.81) exists but has no slider.
- Local vs global (my reading of Nav2, not tested here): global = planner routes and "Start occupied"
  failures; local = how the controller passes obstacles and "Failed to make progress". Keep local at or
  below global.

## Known roaming problems
- "Start occupied" (planner): rover parks near a tree and the planner rejects its own footprint. The costmap
  has no static map, so a blind goal can land near an obstacle. Possible lever: global inflation radius.
- `Failed to make progress` mid-route: the behavior tree clears the local costmap and retries, usually
  recovering. Cause unknown (stale costmap obstacle is a guess).
- `yaw_goal_tolerance` is 3.14 (committed). The heading hypothesis behind it is UNSUPPORTED by testing.

## Environment quirks
- Login shell is fish, but ROS work happens in zsh (ROS has no fish setup script). Type `zsh` first.
- zsh config is `~/.config/zsh/.zshrc` (ZDOTDIR), NOT `~/.zshrc`. `QT_QPA_PLATFORM=xcb` is set there so
  Gazebo/RViz run through XWayland.
- Hyprland custom settings go in `~/hakuspace-control/hyprland-custom.lua`, never `config.lua`
  (`xwayland.force_zero_scaling = true` keeps RViz sharp at 0.67 scale).
- Do NOT set `QT_SCALE_FACTOR` (black flashing in RViz).
- `ModuleNotFoundError: rclpy` means ROS is not sourced in that terminal: use zsh.
- RViz started by hand needs `--ros-args -p use_sim_time:=true` (`sim_nav` handles this).
- With Fixed Frame `map`, odom-attached things shake (map -> odom follows noisy sim GPS). Use `odom` to
  watch driving, `map` only to click boundary points.

## Workflow gotchas
- Edit a package, then `colcon build --packages-select <pkg>` from `~/rover_ws`, then launch.
- New package: open a new terminal (or `source ~/.config/zsh/.zshrc`).
- Every new script goes in `install(PROGRAMS ...)` in `rover_bringup/CMakeLists.txt` and needs `chmod +x`.
- After editing a xacro, build BEFORE testing it with `xacro` or `gz sdf` (`$(find ...)` reads the
  installed copy).
- Xacro macros must be defined above the lines that call them.
- A block pasted outside `</robot>` or `</world>` is silently ignored; every world `<plugin>` must be a
  sibling. Check with `grep -n` / `sed -n`.
- A YAML paste can replace the file instead of appending. Check the keys after editing `ekf.yaml`.
- A sensor numbered `odom1` with no `odom0` is ignored by robot_localization. Number from 0.
- A stale launch causes odd symptoms (old GUI, wheels stacked at the origin). Check `ros2 node list`.
- Do not delete blocks from `nav2_params.yaml` assuming they are unused: our own `nav2.launch.py` replaces
  the stock one because the stock one starts the docking server and aborts when it fails.
- Check `git log` / `git diff` before assuming a change is not committed.
- Known harmless warnings: EKF `Failed to meet update rate!` (watch on the Pi), `/rf2o_laser_odometry`
  listed twice, `controller_server.verbose not found`, one-time `Invalid frame ID "odom"` if Nav2 starts
  before the EKF, `/cmd_vel_teleop` in the topic list, rf2o `Waiting for laser_scans....`.
- Sim runs slow (Gazebo real-time factor ~52 to 55%, `/scan` ~6.7 Hz). Not representative of the Pi.

## Frames and rules
- `base_footprint`: on the ground under the rear axle center. `base_link`: one wheel radius above it
  (X forward, Y left, Z up). `lidar_link` rides on `lift_platform` (prismatic `lift_joint`).
- Drive and navigate only with the lift fully down. The lift has limit switches only (no position feedback).
- Ackermann cannot turn in place: always send forward speed with turn rate.
- The sim plugin keeps the last `/cmd_vel` until a new one arrives. The real firmware needs a
  command-timeout watchdog.
- The LIDAR scans a flat plane (~0.57 m in the sim). Keep the GPS, IMU, and any mast out of that plane.
  Anything lower is a blind spot for the 2D LIDAR.
- Datum (45.0, -110.0) lives in THREE places that must match: world file, `ekf.yaml`, `boundary_canvas.py`
  parameters. All placeholders until the real test site is chosen.
- Hardware: leave `sim_lift_signal.py` and `gps_covariance.py` (and the `gps/fix_cov` remap) out of the Pi
  launch; the ESP32 publishes `/lift_down` and the Pi bridge must treat a missing signal as "lift not down".
  `lidar_odom.launch.py` has no argument for that yet.

## Data flow (sim)
Gazebo -> ros_gz_bridge -> `/scan`, `/imu`, `/gps/fix`, `/odom` (unused by the EKFs), `/joint_states`, `/us/*`.
`/scan` -> `scan_gate` -> `/scan_filtered` -> rf2o -> `/odom_rf2o` -> `odom_covariance` -> `/odom_rf2o_cov`.
EKFs: `ekf_filter_node` (odom frame, `/odometry/local`, owns `odom -> base_footprint`) and
`ekf_filter_node_map` (`/odometry/global`, owns `map -> odom`). Nav2 reads `/odometry/local` and
`/scan_filtered`. Velocity chain: `cmd_vel_nav` -> velocity smoother -> `cmd_vel_smoothed` -> collision
monitor -> `/cmd_vel`.

## Placeholders (replace with real values)
| Item | Value | Status |
|---|---|---|
| Wheelbase, wheel radius/width, chassis L x W x H | 0.686, 0.15875/0.1016, 1.118 x 0.562 x 0.457 | measured |
| Track width | 0.559 | measured, nearly equal to chassis width, recheck |
| Ultrasonic mounting height | 0.0762 m above the wheel tops | measured |
| Total mass | ~30 kg | placeholder (xacro `chassis_mass` 24.0) |
| Ground clearance | 0.08 | placeholder |
| Max steer angle | 0.7 rad | placeholder |
| `minimum_turning_radius` | 0.8 | derived from the placeholder steer angle |
| MPPI speeds | `vx_max` 0.65, `vx_min` -0.45, `wz_max` 1.0 | untested guesses |
| Goal tolerances | `xy` 0.5, `yaw` 3.14 | tuned in the sim only |
| Costmap `inflation_radius` | 1.5 | placeholder (old project); `cost_scaling_factor` 1.0 |
| Lift platform mass/size/speed, `lift_travel` | 5 kg / 0.4 x 0.4 m / 0.05 m/s / 1.2192 m | placeholder |
| Sensor positions, ultrasonic angles/cone/range | see xacro | placeholder |
| Sim GPS noise and covariance | 1.5 m / 2.25 m^2 | placeholder |
| rf2o speed / yaw-rate variance | 0.01 / 0.01 | placeholder guesses |
| Scan gate `lift_timeout`, lift down threshold | 1.0 s, 0.01 m | placeholder |
| `sim_nav` start delays | 10, 15, 22, 25, 26, 30, 30 s | guesses |
| Nav2 footprint | [0.902, +-0.33] to [-0.216, +-0.33] | derived from the xacro, depends on track width |

## Open questions
- Exact ultrasonic model, RPLIDAR S2 variant (30 m or 18 m), real GPS antenna position (needs a mast, out
  of the LIDAR plane), real mass / clearance / steer angle / lift speed.
- Do the real wheels sit outside the chassis?
- Is the DIHOOL lift confirmed at ~0.83 m travel? Reconcile with the xacro before relying on LIDAR height.
- Heading source for GPS fusion on the real rover.
- Does rf2o hold up on rough, cluttered forest terrain? Pi CPU cost of rf2o + Nav2 (S2 gives 3200 points
  per scan) and Smac's ~29 s configure are unmeasured.

## Repo layout
- Repo: github.com/Nico-Carlson/ros_rover (private), local folder `~/rover_ws`
- `src/rover_basics`: ament_python practice package
- `src/rover_description`: ament_cmake. `urdf/rover.urdf.xacro` (model), `urdf/rover.gazebo.xacro` (Gazebo
  plugins and sensors), `worlds/rover_world.sdf`, `launch/display.launch.py`, `launch/sim.launch.py`,
  `rviz/rover.rviz` (saved config used by `sim_nav`)
- `src/rover_bringup`: ament_cmake
  - `config/`: `slam_mapping.yaml`, `ekf.yaml`, `nav2_params.yaml`, `boundary.yaml` (git-ignored)
  - `launch/`: `slam`, `ekf`, `lidar_odom`, `nav2`, `sim_nav`
  - `scripts/`: `gps_covariance`, `odom_covariance`, `scan_gate`, `sim_lift_signal`, `boundary_canvas`,
    `roam`, `control_panel`
- `src/rf2o_laser_odometry`: cloned separately, git-ignored, NOT in this repo. Fetch:
  `cd ~/rover_ws/src && git clone -b ros2 https://github.com/MAPIRlab/rf2o_laser_odometry.git`, then
  `colcon build --packages-select rf2o_laser_odometry`
- Measurements live as `xacro:property` values at the top of the xacro. Edit those, not the geometry.

