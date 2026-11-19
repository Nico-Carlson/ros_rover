# Rover Project Devlog

Full project history, decisions and instructions, organized for a new reader. `NOTES.md` is the one-page
current-state sheet; keep both up to date. To start a new chat, paste this file plus the file being edited
(for firmware also `protocol/SPEC.md` and `protocol/cpp/protocol.h`). Last updated 2026-11-17.

How to read the labels: **PLACEHOLDER** = a guess, replace with a real value. **UNTESTED ON HARDWARE** = only run
on the PC or in the sim. **GUESS** / **from memory** / **read from code** say how a claim was obtained. Anything
without a label was run and checked.

## 1. Summary

An autonomous Ackermann rover for forest navigation (data collection for another school's camera project).
Modes: (1) navigate to a GPS pin avoiding obstacles, (2) roam inside a drawn boundary, (3) get unstuck (skipped for
now). Mapping is secondary.

| Area                                   | State                                                                                                                                             |
| -------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------- |
| Simulation and navigation stack        | Done in Gazebo only: robot model, obstacle world, dual EKF, rf2o LIDAR odometry, scan gating, Nav2, roaming, control panel. Never run on the Pi. |
| Pi <-> ESP serial protocol and bridge  | Done and tested on the PC with virtual serial cables (`socat`). Never connected to a real ESP.                                                    |
| ESP32 firmware (master and sensor)     | Written. Bench-tested on the real rover 2026-11-12: RC inputs, receiver failsafe, safety link, MPU6050, all motor outputs with wheels off the ground. |
| Open firmware blocker                  | None. Lift interlock verified on the bench 2026-11-19 in manual mode (see 8.6). Auto mode path untested until the Pi sends `CMD`. |
| Missing for real navigation            | IMU and GPS lines from the sensor ESP, RPLIDAR driver, a heading source for GPS fusion, Pi wiring, hardware Nav2 params, field testing.          |

Read next: section 10 (open questions) and section 11 (next steps).

## 2. Hardware and stack

- Software: ROS 2 Jazzy, Gazebo Harmonic (Gazebo Sim 8.15.0), Ubuntu 24.04 in a distrobox on Arch (Hyprland). Pi code
  in Python, firmware in C++ (Arduino IDE).
- Hardware: Raspberry Pi 5, 2x ESP32 (master and sensor boards), RPLIDAR S2 (variant 30 m or 18 m unchecked),
  NEO-6M GPS (GY-NEO6MV2), MPU-6050 IMU (GY-521, answered on the sensor board 2026-11-12), RC receiver RC6GS V3,
  DIHOOL scissor lift (controller probably DIHOOL IPS-S2), steering servo (probably DOCYKE), 36 V 500 W brushed motor
  controller (label partly unreadable; battery probably 36 V nominal, GUESS), BTS7960 brake driver, 6 HC-SR04
  ultrasonics (removed from the real rover for now; simulated only).
- Part specs below come from listing titles or memory and are NOT verified: HC-SR04 about 15 deg cone, ~4 m, 5 V echo
  needs level shifting for the 3.3 V ESP32; NEO-6M about 2.5 m accuracy, 1 to 5 Hz, heading only while moving; MPU-6050
  6-axis with NO magnetometer, so no absolute heading; DIHOOL lift listed at about 0.83 m travel while the xacro uses
  1.2192 m (reconcile before trusting LIDAR height).
- The real rover has NO wheel encoders. Wheel odometry exists only in the sim and is not fused. Speed is open loop.
- Possible over-voltage on a 36 to 42 V pack (which supply each part sits on is not recorded): relay contacts 28 VDC,
  IPS-S2 30 V max, BTS7960 about 27 V (from memory). The converter is 5 V / 5 A.
- Drive controller input: throttle range unknown (typical hall style 1 to 4.2 V, UNVERIFIED). The ESP32 DAC does reach
  full speed (bench-verified 2026-11-12).

## 3. Setup, running and conventions

### Environment

- Login shell is fish, but ROS work happens in zsh (ROS has no fish setup script). Type `zsh` first.
- zsh config is `~/.config/zsh/.zshrc` (ZDOTDIR), NOT `~/.zshrc`. It sets `QT_QPA_PLATFORM=xcb` so Gazebo and RViz run
  through XWayland. `ModuleNotFoundError: rclpy` means ROS is not sourced in that terminal.
- Hyprland custom settings go in `~/hakuspace-control/hyprland-custom.lua`, never `config.lua`
  (`xwayland.force_zero_scaling = true` keeps RViz sharp at 0.67 scale). Do NOT set `QT_SCALE_FACTOR` (black flashing).
- `pip` is not installed in the distrobox: `sudo apt install python3-serial socat`.

### Run the sim

```
cd ~/rover_ws
colcon build --packages-select rover_bringup     # after ANY edit; launch uses the installed copy
ros2 launch rover_bringup sim_nav.launch.py
```

- `sim_nav` starts, on timers: sim (0 s), `us_to_range` + rf2o + scan gate (10 s), EKFs (15 s), Nav2 (22 s), RViz (25 s),
  `boundary_canvas` (26 s), `roam` (30 s), control panel (30 s). All delays are guesses; Nav2 needs about 30 s more to
  become active ("Managed nodes are active").
- Gazebo runs server-only (`-s`); attach a window with `gz sim -g`. Closing it does NOT stop the sim. Stop with Ctrl+C in
  the launch terminal; if stuck: `pkill -9 -f "gz sim"; pkill -9 -f parameter_bridge; pkill -9 -f robot_state_publisher; pkill -9 -f ekf_node`.
- Point-to-point goals: RViz "2D Goal Pose". Manual driving only when Nav2 has no goal:
  `ros2 run teleop_twist_keyboard teleop_twist_keyboard` (avoid `j`/`l`; `k` stops).
- Start fresh before any test that compares positions (the EKFs start at (0, 0)).
- `slam.launch.py` is NOT part of `sim_nav` and conflicts with the EKF (both publish `map -> odom`).
- `ekf`, `lidar_odom` and `nav2` launch files take `use_sim_time:=false`; `ekf` and `lidar_odom` also take `sim:=false`
  (leaves out `sim_lift_signal`, `gps_covariance` and the `gps/fix` -> `gps/fix_cov` remap).
- To log: `ros2 launch rover_bringup sim_nav.launch.py 2>&1 | tee ~/sim_nav.log`, then
  `grep -i "abort\|fail\|occupied\|progress" ~/sim_nav.log | tail -20`.

### Tests (serial protocol)

- Python: `cd ~/rover_ws/protocol && python3 -m pytest -q`. Run from INSIDE `protocol/` (from `~/rover_ws` it also collects
  the unrelated failing `rover_basics` flake8 lint). 43 tests at 2026-11-12; `test_bridge.py` starts its own `socat`.
- C++: `cd ~/rover_ws/protocol/cpp && g++ -Wall -o test_protocol test_protocol.cpp protocol.cpp watchdog.cpp drive.cpp reverse_guard.cpp mode_switch.cpp brake.cpp tilt.cpp safety_link.cpp && ./test_protocol | grep FAIL` (no output = pass).
- `vectors.txt` is a shared example list (`ok|line|body`, `bad|line`) read by BOTH test suites so the languages cannot
  drift. Add every new message's examples there; get checksums from `checksum()`, never by hand.

### Git

- `main` always works; one descriptive branch per piece of work (no phase numbers); commit after each working step; merge
  with `--no-ff` when a branch's goal is done; tag each milestone before deleting a branch.
- Tags: `robot-model-v1`, `gazebo-sim-v1`, `obstacle-world-v1`, `ultrasonics-v1`, `slam-mapping-v1`,
  `ekf-localization-v1`, `gps-covariance-v1`, `hardware-matched-ekf-v1`, `scan-gating-v1`, `nav2-sim-v1`,
  `roaming-and-panel-v1`. Finished branches still exist (all merged); deleting them is optional.
- `serial-protocol` (firmware, protocol, bridge, launch files) is NOT merged or tagged. When ready:
  `git checkout main && git merge --no-ff serial-protocol && git tag serial-protocol-v1`.
- Check `git log` / `git diff` before assuming a change is not committed (one edit once went in under the wrong message).

### Repo layout

- Repo: github.com/Nico-Carlson/ros_rover (private), local `~/rover_ws`.
- `src/rover_basics`: ament_python practice package.
- `src/rover_description` (ament_cmake): `urdf/rover.urdf.xacro` (model), `urdf/rover.gazebo.xacro` (Gazebo plugins and
  sensors), `worlds/rover_world.sdf`, `launch/display.launch.py`, `launch/sim.launch.py`, `rviz/rover.rviz`.
  Measurements are `xacro:property` values at the top of the xacro; edit those, not the geometry.
- `src/rover_bringup` (ament_cmake): `config/` (`slam_mapping.yaml`, `ekf.yaml`, `nav2_params.yaml`, `boundary.yaml`
  git-ignored); `launch/` (`slam`, `ekf`, `lidar_odom`, `nav2`, `sim_nav`, `hardware`, `control`); `scripts/`
  (`gps_covariance`, `odom_covariance`, `scan_gate`, `sim_lift_signal`, `boundary_canvas`, `roam`, `control_panel`,
  `us_to_range`, `esp_bridge`). Every script must be in `install(PROGRAMS ...)` in CMakeLists.txt and `chmod +x`.
- `src/rf2o_laser_odometry`: cloned separately, git-ignored, NOT in this repo:
  `cd ~/rover_ws/src && git clone -b ros2 https://github.com/MAPIRlab/rf2o_laser_odometry.git`, then
  `colcon build --packages-select rf2o_laser_odometry`.
- `protocol/` (NOT a ROS package): `SPEC.md`, `protocol.py`, `bridge.py`, `kinematics.py`, `cpp/`, tests, `vectors.txt`,
  `fake_esp.py`, `try_bridge.py` (manual tools for a socat cable). `esp_bridge` finds `bridge.py`, `protocol.py` and
  `kinematics.py` through an `install(FILES ../../protocol/... )` block above `ament_package()` in CMakeLists.txt.
- `firmware/master_serial/`, `firmware/sensor_serial/`: the two ESP32 sketches. Each holds COPIES of the `protocol/cpp/`
  files it needs (the Arduino IDE only compiles files inside the sketch folder): re-copy after any change there.
- Pi clean build: `rosdep install --from-paths src --ignore-src -r -y --skip-keys "cmake_modules ros_gz_sim ros_gz_bridge joint_state_publisher_gui"`
  (`exec_depend` entries are declared in both packages; untried on a real Pi).

### Workflow gotchas (these have bitten us)

- After editing a package: `colcon build --packages-select <pkg>`, then launch. After creating a NEW package, open a new
  terminal. A stale launch left running causes odd symptoms (old GUI, wheels stacked at the origin): check `ros2 node list`.
- Launch files need their folder installed (`data_files` in setup.py, or `install(DIRECTORY ...)` in CMake).
- Xacro: macros must be defined above the lines that call them; properties only use ones defined above. `$(find ...)` reads
  the INSTALLED copy, so build before testing an xacro with `xacro` or `gz sdf`.
- A block pasted outside `</robot>` or `</world>` is silently ignored; every world `<plugin>` must be a sibling (a misplaced
  one once disabled the IMU). Check with `grep -n` / `sed -n`.
- A YAML paste can replace the file instead of appending. After editing `ekf.yaml`: `python3 -c "import yaml; print(list(yaml.safe_load(open('src/rover_bringup/config/ekf.yaml')).keys()))"` must list all three node names.
- A robot_localization sensor numbered `odom1` with no `odom0` is ignored. Number from 0.
- Do not delete blocks from `nav2_params.yaml` assuming they are unused (see Nav2, 6.6).
- RViz config: `sim_nav` loads the INSTALLED `rover.rviz`. Save with File > Save Config As into
  `~/rover_ws/src/rover_description/rviz/rover.rviz`, then rebuild `rover_description`; a plain Save is overwritten by the
  next build. Rebuilding also discards RViz displays added live and never saved.
- RViz started by hand needs `--ros-args -p use_sim_time:=true`. With Fixed Frame `map`, odom-attached items shake (map -> odom
  follows the noisy sim GPS): use `odom` to watch driving, `map` only to click boundary points.
- `ros2 topic echo` truncates arrays to 128 entries: use `--truncate-length 0` (count rays with `grep -o inf | wc -l`). "A message
  was lost!!!" in echo output is the echo client. `pgrep` needs `-f` for names over 15 characters.
- Known harmless warnings: EKF `Failed to meet update rate!` (may matter on the Pi); `/rf2o_laser_odometry` listed twice;
  `controller_server.verbose not found`; a one-time `Invalid frame ID "odom"` if Nav2 starts before the EKF; `/cmd_vel_teleop` in
  the topic list; rf2o `Waiting for laser_scans....`; navsat_transform "Latitude 45d more than 20d from N pole" (unconfirmed).
- The sim runs slow (real-time factor about 52 to 55%, `/scan` about 6.7 Hz); it says nothing about Pi performance.
- the variant must be kept in sync with nav2_params.yaml when you tune the original (it’s a copy, so tuning one doesn’t change the other).

## 4. Frames, data flow and measurements

- `base_footprint`: on the ground under the rear axle center. `base_link`: one wheel radius above it (X forward, Y left, Z up).
  `lidar_link` rides on `lift_platform` (prismatic `lift_joint`, 0 to 1.2192 m).
- Rule: drive and navigate only with the lift fully down. The lift has limit switches only (no position feedback).
- Ackermann cannot turn in place: always send forward speed with turn rate.
- The LIDAR scans a flat plane (about 0.57 m in the sim); anything lower is a blind spot, and the GPS, IMU and any mast must
  stay out of that plane (the sim LIDAR also detected a GPS box that had no collision).

Sim data flow: Gazebo -> `ros_gz_bridge` -> `/scan`, `/imu`, `/gps/fix`, `/odom` (unused by the EKFs), `/joint_states`, `/us/*`.
`/scan` -> `scan_gate` -> `/scan_filtered` -> rf2o -> `/odom_rf2o` -> `odom_covariance` -> `/odom_rf2o_cov`. Two EKFs:
`ekf_filter_node` (odom frame, `/odometry/local`, owns `odom -> base_footprint`) and `ekf_filter_node_map` (`/odometry/global`, owns
`map -> odom`, also fuses `/odometry/gps` from navsat_transform). Nav2 reads `/odometry/local` and `/scan_filtered`. Velocity chain:
`cmd_vel_nav` -> velocity smoother -> `cmd_vel_smoothed` -> collision monitor -> `/cmd_vel` (to Gazebo in the sim, to `esp_bridge` on
hardware). Ultrasonics: `/us/*` (5-ray scans) -> `us_to_range` (sim only) -> `/us_range/*` (Range) -> `range_layer` in both costmaps.

Hardware data flow (planned; Pi never wired yet): `/cmd_vel` -> `esp_bridge` -> `CMD` -> master ESP. Master ESP -> `MODE` -> bridge ->
`/rover/*`. Sensor ESP -> `LIFT`, `BAT` (done), `IMU`, `GPS` (bridge side done and PC-tested, sketch side written 2026-11-19, UNTESTED ON HARDWARE) -> bridge -> `/lift_down`,
`/battery_voltage`, `/imu`, `/gps/fix`. `/lift_down` feeds `scan_gate`. The RPLIDAR driver publishing `/scan` is not written.

### Measurements and placeholders

Measured: wheelbase 0.686; wheel radius / width 0.15875 / 0.1016; chassis 1.118 x 0.562 x 0.457; ultrasonic height 0.0762 above
the wheel tops. Track width 0.559 is measured but nearly equals the chassis width: recheck (do the wheels sit outside the chassis?).

| Item                                                      | Value                                                  | Status                                              |
| --------------------------------------------------------- | ------------------------------------------------------ | --------------------------------------------------- |
| Total mass                                                | ~30 kg                                                 | PLACEHOLDER (xacro `chassis_mass` 24.0, rest unsummed) |
| Ground clearance                                          | 0.08                                                   | PLACEHOLDER                                         |
| Max steer angle                                           | 0.7 rad                                                | PLACEHOLDER (measure via servo test)                |
| `minimum_turning_radius`                                  | 0.8 (MPPI and Smac)                                    | derived: 0.686 / tan(0.7) = 0.81                    |
| MPPI `vx_max` / `vx_min` / `wz_max`                       | 0.65 / -0.45 / 1.0                                     | untested guesses                                    |
| Goal tolerances `xy` / `yaw`                              | 0.5 / 3.14                                             | tuned in the sim only                               |
| Costmap `inflation_radius` / `cost_scaling_factor`        | 1.5 / 1.0                                              | PLACEHOLDER (old project)                           |
| Lift mass / size / speed / `lift_travel`                  | 5 kg / 0.4 x 0.4 m / 0.05 m/s / 1.2192 m               | PLACEHOLDER                                         |
| Sensor positions, ultrasonic angles, cone, range          | see xacro (`us_side_y` 0.28, `us_angle` pi/4; 4 m)     | PLACEHOLDER                                         |
| Sim GPS noise and covariance                              | 1.5 m / 2.25 m^2                                       | PLACEHOLDER                                         |
| rf2o speed / yaw-rate variance; gate limits               | 0.01 / 0.01; 1.5 m/s, 2.0 rad/s                        | PLACEHOLDER guesses                                 |
| Scan gate `lift_timeout`; lift down threshold             | 1.0 s; 0.01 m                                          | PLACEHOLDER                                         |
| Nav2 footprint                                            | [0.902, +-0.33] to [-0.216, +-0.33]                    | derived from xacro, depends on track width          |
| Range layer (`phi`, thresholds, `clear_on_max_reading`)   | 1.2, 0.2 / 0.8, true                                   | from memory, not tuned                              |
| Datum (lat, lon)                                          | 45.0, -110.0                                           | PLACEHOLDER; lives in THREE places that must match: world file, `ekf.yaml`, `boundary_canvas.py` parameters |
| Extra sim trees                                           | `tree_5` to `tree_26`                                  | PLACEHOLDER positions                               |
| `sim_nav` start delays                                    | 10, 15, 22, 25, 26, 30, 30 s                           | guesses                                             |
| `hardware.launch.py` timers                               | 5, 10, 17, 21, 25 s                                    | guesses                                             |
| Serial baud; safety link timeout                          | 115200; 350 ms                                         | PLACEHOLDER (timeout from the old firmware)         |
| Command timeout (master and `esp_bridge`); mode timeout   | 500 ms; 500 ms                                         | PLACEHOLDER                                         |
| Reverse settle; brake retract                             | 500 ms; 2000 ms                                        | PLACEHOLDER                                         |
| `esp_bridge` steering clamp; drive full scale; stop band  | 30 deg; 1.5 m/s; 0.05 m/s                              | PLACEHOLDER                                         |
| `gyro_variance`, `accel_variance`, `gps_uere`             | 0.0004, 0.04, 1.5                                      | PLACEHOLDER                                         |
| IMU / GPS / BAT / LIFT rates                              | 50 Hz / per new fix / 1 Hz / 10 Hz                     | PLACEHOLDER                                         |
| RC pulse constants (throttle, brake, reverse)             | min 1010, max 1500; 1900; 1700                         | proposed from 2026-11-12 data, confirm in the sketch |

## 5. Old project files (reference only)

Earlier uploaded files, NOT trusted, reused selectively: `ekf.yaml` (dual-EKF design, reused), `nav2_params.yaml` (copied and edited,
see 6.6; original in `~/SmartFiresFa26/smart_fires_rover/config/`), `ackermann_bt.xml` and `ackermann_through_poses_bt.xml` (not used,
not reviewed), `robot.xacro` / `robot.gazebo` (old model, replaced; useful plugin reference), `Rover_Documentation.docx` (RC channels:
1 steering, 2 throttle and brake, 5 mode, 6 lift, 7 reverse; partly blank pin tables; feature list: tilt and collision alarms,
Bluetooth telemetry, lift interlock, auto-brake when throttle is off, override switch, stop on loss of radio; no encoders mentioned).
Wiring PDFs (unverified): master ESP drives a BTS7960 for the brake, a steering servo, an RC receiver (7 channels) and the lift
controller (relay, limit switches); the sensor ESP handles ultrasonics, gyro (I2C), battery voltage and an override switch.

## 6. Simulation and navigation (all sim-only, UNTESTED ON HARDWARE)

### 6.1 Model, world and sensors (tag gazebo-sim-v1, obstacle-world-v1)

- `rover.gazebo.xacro` holds the plugins: AckermannSteering, JointStatePublisher, NavSat, Imu, Sensors (ogre2). Old model names
  translate: `wheel1/2_joint` -> `rear_right/left_wheel_joint`, `body_link` -> `base_link`, `wheel_separation` -> `track_width`,
  `wheelbase_length` -> `wheelbase`, `steering_limit` -> `max_steer_angle`.
- `worlds/rover_world.sdf`: 1 ms physics, 100 x 100 m ground, `<spherical_coordinates>` at the datum (ENU), obstacles: `tree_1` to
  `tree_4` (cylinders r 0.2 to 0.35, 2 m), `rock_1` (0.5 m), `boulder_1` (0.9 m), `stump_1` (0.4 m, 2.5 m ahead of spawn), plus 22
  more trees. The error "Root object can only contain one of model, light or actor" means something is outside `<world>`.
- `sim.launch.py`: Gazebo with `-r`, robot_state_publisher, spawn (`-topic robot_description -z 0.3`), `ros_gz_bridge`. The bridge sends
  `/clock`, `/odom`, `/scan`, `/joint_states`, `/imu`, `/gps/fix` and the six `/us/*` topics to ROS and `/cmd_vel` to Gazebo. Its `/tf`
  line is commented out so the EKF owns `odom -> base_footprint` (first suspect if transforms break). The "gazebo process has died,
  exit code -2" message on Ctrl+C is harmless.
- Verified: `/cmd_vel` drives and steers the rover; the plugin keeps the LAST command until a new one arrives (the real firmware must
  and does time out, see 7.3). LIDAR: `gpu_lidar` on `lidar_link`, 3200 samples (0.1125 deg), 10 Hz, 0.05 to 10 m (the S2's 30 m is for
  bright targets; dark forest gives about 10 m), `gz_frame_id` keeps the frame name because Gazebo lumps `lidar_link` into
  `lift_platform`. IMU about 100 Hz (noise values PLACEHOLDER). GPS about 10 Hz. Scan plane at about 0.57 m (chassis top 0.537 + 0.03):
  sees the boulder, misses `rock_1` and `stump_1`.
- The sim bridge zeroes GPS and odom covariance; handled in 6.3 and 6.4.

### 6.2 Ultrasonics (tag ultrasonics-v1)

- Layout: front and rear center sensors plus four corner sensors at 45 deg outward (positions PLACEHOLDER except height); the sides
  between corners are not covered. Simulated as 5-ray `gpu_lidar` fans (cone +-0.131 rad, 0.03 to 4 m, 10 Hz).
- A real ultrasonic reports ONE distance per cone and struggles with angled or soft surfaces like foliage. `us_to_range.py` (SIM ONLY)
  converts each 5-ray scan to one `sensor_msgs/Range` on `/us_range/<name>` (nearest valid ray, `inf` if none). Real firmware should
  publish Range directly; keep the costmap config, drop the node.
- `nav2_params.yaml` has a `range_layer` (RangeSensorLayer, six `/us_range/*` topics) in BOTH costmaps between `obstacle_layer` and
  `inflation_layer` (inflation must stay last). Verified by eye: the stump (invisible to the LIDAR) shows in both costmaps; a front
  sensor read 1.388 m to it, matching the hand calculation. Limit: sensors see about 4 m. If `us_to_range` is not running, short
  obstacles silently vanish.

### 6.3 Localization (tag ekf-localization-v1, gps-covariance-v1, hardware-matched-ekf-v1)

Final design (superseding the early single-EKF and wheel-odometry steps, which are not described here):

- Neither EKF fuses the sim `/odom` (no encoders on the real rover). Odom filter: `/odom_rf2o_cov` forward speed and yaw rate, plus IMU yaw
  rate (and, SIM ONLY, absolute yaw). Map filter: GPS x/y as `odom0`, IMU yaw rate (and SIM ONLY absolute yaw), `/odom_rf2o_cov` as `odom1`.
  `navsat_transform` reads `odometry/global`, uses `wait_for_datum: true` and `datum: [45.0, -110.0, 0.0]` (without a datum it used the
  first noisy fix as origin, a fixed ~2.3 m offset).
- The sim IMU acts as a perfect compass (ENU, yaw 0 = facing east). The real MPU6050 has no magnetometer, so the real rover needs
  another heading source for GPS fusion, and the hardware `ekf.yaml` must NOT fuse IMU orientation. Open.
- GPS covariance (SIM ONLY): `gps_covariance.py` fills a diagonal covariance (`horizontal_variance` 2.25, `vertical_variance` 9.0) and
  publishes `/gps/fix_cov`; `ekf.launch.py` remaps navsat_transform to it. On hardware `esp_bridge` supplies its own covariance, so drop
  the node and the remap (`sim:=false` does). Sim GPS noise: horizontal stddev 0.0000135 deg (about 1.5 m N-S), vertical 3.0.
- Verified stationary (15 samples): raw `/odometry/gps` x std dev about 1.1 m, `/odometry/global` about 0.28 m. Before the covariance
  node, global copied GPS to 1e-10.
- Verified with the rover held against the stump, wheels spinning: truth x 1.398, wheel odometry 2.730 (overcount), `/odometry/gps` and
  global 1.3975, so GPS fusion corrects wheel slip. With rf2o replacing wheels: `/odometry/local` 1.379, global about 1.31 (std dev 0.12).
- Absolute yaw in the map EKF fixed a yaw wander (-90 to -51 deg) with noisy GPS while stationary (cause a guess; the fix worked).
- Stuck-detection idea changed for hardware: compare COMMANDED speed with rf2o speed (no wheel speed exists).

### 6.4 rf2o LIDAR odometry

- rf2o (GPL v3) is cloned separately (see Repo layout). Its own launch file defaults to `publish_tf: True` and `base_link`, which would
  fight the EKF; use `lidar_odom.launch.py` (`publish_tf` False, `base_frame_id` base_footprint, 10 Hz). Publishes `/odom_rf2o` with all-zero
  covariance; `odom_covariance.py` fills the twist covariance (0.01, PLACEHOLDER) and publishes `/odom_rf2o_cov`.
- rf2o sometimes publishes impossible speeds (seen 18.3 m/s and -3.83 rad/s; the odom EKF then reached positions of hundreds of meters,
  once 7.6 km). Cause UNKNOWN (guesses: irregular scan timing, bad matches). `odom_covariance.py` drops non-finite readings and anything over
  `max_speed` 1.5 m/s or `max_yaw_rate` 2.0 rad/s. KEEP THIS GATE ON HARDWARE. rf2o's own pose still wanders 10 to 25 m, which does not matter
  because the EKFs use only its speed and yaw rate.
- rf2o froze when every ray was `inf` (rover far from trees), so the world got 22 extra trees. It needs the lift down and structure in range;
  forest performance and Pi CPU cost (3200 points per scan) are unmeasured. Verified in single sim runs: reverse 1.56 m gave rf2o -1.579 vs
  wheels -1.558; against the stump rf2o read 1.364 vs truth 1.398 (assumes the same stopping spot).

### 6.5 Scan gating (tag scan-gating-v1)

- LIDAR consumers (rf2o, Nav2, later SLAM) must only see scans taken with the lift fully down. One signal: `/lift_down` (`Bool`). `scan_gate.py`
  subscribes `/scan` (best-effort sensor QoS) and `/lift_down`, republishes `/scan_filtered`. FAIL-SAFE: blocks scans if no `/lift_down` ever
  arrived, it is false, or the last one is older than `lift_timeout` (1.0 s).
- `sim_lift_signal.py` (SIM ONLY) derives `/lift_down` from `lift_joint` (< 0.01 m). The sim lift cannot move (no controller), so the gate was
  tested by hand-publishing `/lift_down`: no signal blocks, `true` passes (about 9.9 Hz), `false` blocks, a stopped publisher blocks within about a
  second. The node publishes on every joint-state message (about 400 Hz); a 10 Hz timer is a later fix.
- Open: when `/scan_filtered` goes silent Nav2's costmaps just stop updating, which looks like "no obstacles". The rover should stop; not designed.

### 6.6 Nav2 (tag nav2-sim-v1)

- `nav2_params.yaml` (copied from the old project): footprint [0.902, +-0.33] to [-0.216, +-0.33]; scan topics `/scan_filtered`; odom topics
  `/odometry/local`; `consider_footprint: true`; no static map (rolling costmaps); Smac Hybrid planner (Reeds-Shepp, reverse allowed); MPPI controller
  (Ackermann model, `vx_min` -0.45); dropped amcl, route_server, docking_server and loopback_simulator blocks.
- `nav2.launch.py` is our OWN launch, because the stock `navigation_launch.py` hard-codes `route_server` and `docking_server`, and docking fails
  without a dock plugin and aborts the whole bringup. (Deleting the `docking_server` block from the params file did not help for the same reason.)
  Starts eight nodes plus a lifecycle manager; start it LAST. Smac takes about 29 s to configure (check on the Pi).
- Not done: real steer angle for `minimum_turning_radius`, scan-silent handling, Pi CPU check, nav2_params_hardware.yaml (same as nav2_params.yaml but range_layer dropped from both plugins: lists, blocks left in place) exists, nav2.launch.py takes params_file, sim_nav takes nav2_params (default: original), hardware.launch.py takes nav2_params (default: the variant). Verified 2026-11-18: sim with the variant reaches “Managed nodes are active” with both costmaps lacking the range layer; hardware launch on socat cables shows the local costmap without it.

### 6.7 Roaming, boundary and control panel (tag roaming-and-panel-v1)

- `boundary_canvas.py`: RViz "Publish Point" clicks become a closed polygon (Fixed Frame must be `map`). A 200 x 200 m slab Marker gives Publish
  Point a surface; it is hidden until New Boundary and removed when the polygon closes, so press New Boundary first even on a fresh start. Click
  within 3 m of the first point (3+ points) to close; saves `config/boundary.yaml` (git-ignored) as lat/lon around the datum and publishes
  `/boundary_closed`. Assumes map origin = datum, x east, y north (inferred, not separately checked). Services: `/clear_boundary` (Empty).
- `roam.py`: `VisitGrid` (5 m cells), `Walker` (hops 5 to 8 m, up to +-60 deg turn; when nearby cells are all visited it targets the least-visited cell),
  `RoamNode` (reads `map -> base_footprint`, sends `NavigateToPose` goals). Services `/start_patrol`, `/stop_patrol` (Trigger; start answers
  `success=False` with no boundary). `/roam_markers` shows the boundary (yellow) and goal (green ball). Goal watchdog: `goal_timeout` 90 s sim time
  (PLACEHOLDER) cancels an accepted goal that runs too long. `/patrol_status` (String, 1 Hz): `<state> | goals ok N, failed N, canceled N | unvisited cells N`
  (states: No boundary, Stopped, Patrolling, Patrolling (waiting for a goal)); status 4 = ok, 5 = canceled.
- Lost-result bug (fixed): rclpy's action client holds only a weak reference to the goal handle, so `roam.py` keeps `self.goal_future`, `self.goal_handle` and
  `self.result_future`. After this, goals finished back to back (inferred, not proven).
- `control_panel.py` (PyQt5): New Boundary, Start Patrol, Stop Patrol, a mode toggle (only greys out buttons; does not stop a running patrol), a
  `/patrol_status` label (stale after 5 s), and parameter sliders that set a DOUBLE parameter via `set_parameters` on release (roam `goal_timeout`;
  controller_server `FollowPath.vx_max`, `wz_max`, `general_goal_checker.yaw_goal_tolerance`, `xy_goal_tolerance`; both costmaps
  `inflation_layer.inflation_radius`, `cost_scaling_factor`). Sliders start at hard-coded values (not read from the nodes) and changes last until relaunch:
  copy keepers into `nav2_params.yaml`. "(rejected)" means the node refused the value (integer-typed or not runtime-changeable).
- Rover should be inside or near the polygon at start, or roam prints `No goal found inside the boundary`.
- Results (sim, runs not comparable in length): latest run 34 goals ok, 5 aborted, 4 `Failed to make progress`, 1 `Start occupied`. Remaining aborts: cancels
  (watchdog), `Start occupied` next to trees (the costmap sees a tree only on approach, so a blind goal can land beside it), occasional planner timeouts. `Failed to
  make progress` mid-route is usually recovered by the behavior tree clearing the costmap; cause unknown (stale costmap obstacle is a guess). Earlier runs and the inflation-slider
  experiments were confounded by the rf2o spikes (6.4); retest them. `yaw_goal_tolerance` 3.14 is committed but its heading hypothesis is UNSUPPORTED.
- Stuck detector and recovery: SKIPPED by decision. It needs a second signal besides rf2o speed (rf2o fails in empty scenes and spikes).

### 6.8 SLAM

- `slam.launch.py` wraps stock slam_toolbox online_async with `slam_mapping.yaml` (`max_laser_range` 10). Mapped the trees and boulder correctly in the sim (easy case: no slip).
  `use_sim_time` is hard-coded true and `scan_topic` still reads `/scan`: change to `/scan_filtered` and a launch argument when next used. SLAM and the GPS EKF both want `map -> odom`,
  so SLAM is a separate mapping mode, and running it alone also needs an `odom -> base_footprint` source (unsolved).

## 7. Pi <-> ESP serial link (protocol, bridge, launch)

### 7.1 Decisions (2026-11-08)

- Plain serial instead of micro-ROS (which lagged badly in the old build, cause never found; see section 9). The ESPs run no ROS; a pyserial bridge on the Pi translates to topics.
- Manual/auto override lives in the master ESP and works with the Pi off. First goal was fully manual driving with an RC override of any Nav2 command.
- One dedicated UART per ESP, no shared link and no USB adapters. PLACEHOLDER: both ESPs on UART0 (master -> Pi UART0, sensor -> Pi UART2). Consequence: USB flashing and debug
  prints share these pins, so unplug the Pi to flash and set `DEBUG_PRINT` to 0 before wiring the Pi. Pi 5 UART names and `config.txt` overlays are UNCHECKED
  (probably `/dev/ttyAMA...`).
- Pure logic is kept free of Arduino calls so it compiles and is tested on the PC; the bridge is tested against a fake ESP on a virtual serial port.

### 7.2 Protocol (`protocol/SPEC.md` is the source of truth; DRAFT, rates PLACEHOLDER)

Lines are `TYPE,field,...*CS\n`, CS = XOR of every character before `*` as two uppercase hex digits, max 100 characters, a bad or missing checksum drops the line, the receiver never
blocks and recovers on the next newline. Booleans are `1`/`0`; field order is part of the spec.

| Direction | Line                                | Meaning                                                                                                                                      |
| --------- | ----------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------- |
| Pi -> ESP | `CMD,speed,steer,lift`              | speed m/s (negative = reverse, 2 decimals), steering ANGLE in degrees (1 decimal, positive = left, computed on the Pi), lift speed (always 0.00 for now) |
| ESP -> Pi | `MODE,auto,danger,warning,brake`    | autonomous switch, danger, warning, brake applied                                                                                            |
| ESP -> Pi | `LIFT,down,up`                      | limit-switch LEVELS (1 = pressed), continuous (10 Hz). `/lift_down` is true ONLY when the latest line is under 1 s old, down is 1 and up is 0 |
| ESP -> Pi | `BAT,volts`                         | battery voltage, one decimal, 1 Hz                                                                                                           |
| ESP -> Pi | `IMU,ax,ay,az,gx,gy,gz`             | m/s^2 (2 decimals), rad/s (3 decimals), body frame x forward, y left, z up; ESP applies scaling, axis mapping and gyro bias; 50 Hz; Pi stamps on receipt; no orientation |
| ESP -> Pi | `GPS,fix,sats,lat,lon,alt,hdop`     | fix 1/0, satellites, decimal degrees (7 decimals), altitude m (1 decimal), HDOP (2 decimals); ESP parses NMEA; sent once per NEW fix; fix 0 = Pi publishes nothing |

There is no `DANGER` message (decided 2026-11-17): a Pi stop is `CMD` with speed 0, and the master brakes on a stale `CMD`. A separate one-way packet runs sensor -> master on pins 16/17 (the safety link, 8.3).

### 7.3 Python and Pi side

- `protocol.py`: `checksum`, `parse_line`, `make_line`, `make_cmd`, `make_lift`, `LineReader`, `split_body`, `parse_mode`, `parse_floats`, `parse_imu`, `parse_gps` (the last three reject wrong
  field counts, non-numeric or non-finite values, bad ranges, negative HDOP or satellites). parse_imu and parse_gps are tested in protocol/test_imu_gps.py (11 tests); IMU, GPS and MODE lines are in vectors.txt.
- `bridge.py`: `Bridge` (pyserial, non-blocking, `poll()`, `send_cmd()`, `poll_with_lift()`) and `LiftState` (`lift_down` per the rule above; "both limits pressed" counts as a fault).
  `poll_with_lift` is assumed to pass unknown kinds (`BAT`, `IMU`, `GPS`) through unchanged. `kinematics.py`: `twist_to_steer_deg(v, wz, wheelbase=0.686, max_deg=30, stop_speed=0.05)` =
  atan(L * wz / v) in degrees, positive = left, 0 at or below the stop speed, clamped. (The old firmware's `wz * 180/pi` was wrong: it saturated at wz about 0.52.)
- C++ (`protocol/cpp/`): `protocol`, `watchdog`, `drive`, `reverse_guard`, `mode_switch`, `brake`, `tilt`, `safety_link`; IMU and GPS builders are now in protocol/cpp/ (protocol_make_imu, protocol_make_gps), tested against vectors.txt and copied into both sketch folders. Known small gaps: an over-long line leaves its
  tail read as a new line (one extra drop, harmless).
- `esp_bridge.py` (`rover_bringup/scripts`), PC-tested, UNTESTED ON HARDWARE. Parameters: `master_port`, `sensor_port` (PLACEHOLDER `/dev/ttyUSB0`, `/dev/ttyUSB1`), `baud` 115200, `lift_timeout` 1.0,
  `wheelbase` 0.686, `max_steer_deg` 30, `cmd_rate_hz` 20, `cmd_timeout` 0.5, `mode_timeout` 0.5, `gyro_variance`, `accel_variance`, `gps_uere`. It exits at startup if either port is missing.
  - Sensor port: `LIFT` -> `/lift_down` (Bool, 10 Hz timer, false when silent); `BAT` -> `/battery_voltage` (Float32, placeholder name); `IMU` -> `/imu` (`imu_link`, stamped on the Pi,
    `orientation_covariance[0]` = -1, gyro and accel covariance from the parameters); `GPS` -> `/gps/fix` (`gps_link`, STATUS_FIX, covariance from `max(hdop, 0.5) * gps_uere`: horizontal (h)^2, vertical
    4 * (h)^2, type APPROXIMATED; nothing published when fix is 0).
  - Master port: `MODE` -> `/rover/autonomous`, `/rover/danger`, `/rover/warning`, `/rover/brake` (Bool) and `/rover/esp_link_ok` (false when no `MODE` for 0.5 s; a silent master also forces
    `autonomous` false). `/cmd_vel` -> `CMD` at 20 Hz through the master only; a `/cmd_vel` older than 0.5 s, or non-finite, sends speed 0.
- Launch files (PC-tested with no rover): `control.launch.py` (laptop: RViz with the saved `rover.rviz` plus the panel, `use_sim_time` false). `hardware.launch.py` (Pi): `esp_bridge` (arguments `master_port` and
  `sensor_port`, edited 2026-11-17; rerun 2026-11-17 on socat cables, esp_bridge opens both ports, then on timers 5/10/17/21/25 s `lidar_odom`, `ekf`, `nav2`, `boundary_canvas`, `roam`, all `use_sim_time` false.
  `boundary_canvas` and `roam` run on the Pi because the canvas saves `boundary.yaml` and roam reloads it from the same folder. Still missing from it: the RPLIDAR S2 driver (`/scan`, frame `lidar_link`), `DEBUG_PRINT` 0 in the firmware. Stable port names (`/dev/serial/by-id/...` or the Pi's own UART names) are needed once wired.

### 7.4 Testing the bridge on the PC (no ESP needed)

Use fixed names, because plain socat pty numbers change on every restart and a wrong number prints into a terminal instead of reaching the bridge.

```
# terminal A (master cable)    socat -d -d pty,raw,echo=0,link=/tmp/bridge_master pty,raw,echo=0,link=/tmp/esp_master
# terminal B (sensor cable)    socat -d -d pty,raw,echo=0,link=/tmp/bridge_sensor pty,raw,echo=0,link=/tmp/esp_sensor
# terminal C (bridge)          ros2 run rover_bringup esp_bridge.py --ros-args -p master_port:=/tmp/bridge_master -p sensor_port:=/tmp/bridge_sensor
# then echo /lift_down, /rover/autonomous, /imu, /gps/fix; write lines to /tmp/esp_sensor or /tmp/esp_master; cat /tmp/esp_master shows CMD
```

Verified 2026-11-17 on two socat cables:

- `LIFT,1,0*16` on the sensor cable sets `/lift_down` true; `MODE,1,0,0,0*02` on the master cable sets `/rover/autonomous` true (resend within 0.5 s); CMD appears only on the master cable.
- CMD follows `/cmd_vel`: x 0.3, z 0.2 gives `CMD,0.30,24.6,0.00*7B` (atan(0.686 * 0.2 / 0.3)); it returns to speed 0 within 0.5 s of the publisher stopping; negative z gives negative steer; 0.03 m/s gives steer 0.0; large z clamps
  at 30.0; negative speed passes through.
- `IMU,0.10,0.00,9.81,0.000,0.000,0.200*62` gives accel x 0.1, z 9.81, gyro z 0.2, `orientation_covariance[0]` -1, covariances 0.0004 / 0.04. `GPS,1,9,48.1234567,-114.1234567,900.5,1.20*66` gives status 0 and covariance 3.24 / 12.96.
  `GPS,0,...`, an `IMU` line with the wrong field count and one with a bad checksum publish nothing.

## 8. Firmware and bench results (real rover)

### 8.1 Files

- New serial firmware (2026-11-12): `firmware/master_serial/master_serial.ino` and `firmware/sensor_serial/`. Every uncertain value (pins, pulse ranges, thresholds) is in one `FILL IN` block at the top of each sketch.
  `ENABLE_OUTPUTS` 0 in the master means no motor pin is touched. Pins are the pre-ROS ones.
- Older firmware (reference only, see section 9): `SenserBoardRedone.ino` + `MasterBoardRedone.ino` (pre-ROS, drove well manually) and `ROSSensorBoard.ino` + `ROSMasterBoard.ino` (micro-ROS, does not work).

### 8.2 Master sketch behavior (DECIDED)

- Auto only if the switch says auto AND the Pi `CMD` is fresh (500 ms); otherwise manual. Mode switch: manual at or below 1400 us, auto at or above 1600, the dead zone between is manual immediately, 3 frames in a row to change,
  stale = manual. Pin 22 is transmitter channel 5.
- RC channels are read by edge interrupts (`RcChannel` in `rc_channel.h`, its own header because the Arduino IDE's auto prototypes broke when it sat in the .ino) with a 100 ms freshness check; `rc_clamp_map` saturates instead of returning 0.
  Manual throttle is 100% (`MANUAL_THROTTLE_SCALE` 1.0; the old firmware capped at 0.75).
- `drive_from_speed`: ONE threshold decides throttle DAC, reverse relay and brake together (fixes the old "small reverse drives forward" bug). Reverse uses `reverse_guard`: stop, settle 500 ms, flip relay, go (no encoders, so it only waits).
  Manual reverse is a ch7 rising edge toggle. Brake always wins over throttle. Brake: extend until the limit switch, retract for a fixed 2000 ms.
- Danger and warning: danger stops and brakes AUTONOMOUS driving only; manual ignores it (so the rover cannot strand itself on a slope). Danger does not touch the reverse relay or the lift. Manual driving does NOT need the sensor heartbeat.
- Steering: servo output from RC or the Pi angle (`STEER_AUTO_SIGN`, +1 = higher pulse = left, direction UNKNOWN for the auto path). Manual lift with a throttle interlock (no lift movement while the throttle is on).
- `MODE` is sent to the Pi at 10 Hz. Debug: `DEBUG_PRINT` and the pulse line share UART0 with the Pi link.
- Servo bug found and fixed 2026-11-12: calling `writeMicroseconds()` every loop knocked out the RC input (no signal -> brake requested -> everything twitched). Writing only when the target changes by 8 us or more works; WHY repeated writes
  break it is unknown.

### 8.3 Sensor sketch and safety link

- Sends `LIFT` at 10 Hz, `BAT` at 1 Hz, `IMU` at 50 Hz, `GPS` once per GGA sentence, reads the MPU6050 tilt at 50 Hz (a dead sensor for 350 ms = danger), and sends the safety packet.
- `IMU` and `GPS` sending written 2026-11-19, compiles, UNTESTED ON HARDWARE. IMU: same read as the tilt code, mounting signs applied, gyro bias averaged over 200 samples (about 2 s) at boot, so the rover must be still at power-up (PLACEHOLDER). The 13 deg mounting offset is NOT applied to the IMU line (only to tilt). GPS: TinyGPSPlus on Serial2, 9600 baud (from memory), one line per GGA via `TinyGPSCustom` on field 6 (never per location update, which would double-count fixes), no fix sends `fix=0`. Pins RX 36 / TX 33 UNVERIFIED. Assumes the module sends `$GPGGA` (a `$GNGGA` module would send nothing). Assumes reading `TinyGPSCustom::value()` clears its update flag (not checked against the installed library version).
- Safety link (sensor -> master, one way, pins 16/17, shared ground, 115200 PLACEHOLDER): 4 bytes at 10 Hz: magic 0xA5, sequence, flags (bit 0 danger, bit 1 warning, bit 2 lift not down), CRC-8 (poly 0x07, init 0) over sequence + flags.
  Code in `protocol/cpp/safety_link.*` (CRC, builder, byte reader that resyncs on the magic byte, freshness wrapper). Danger and warning HOLD until the next good packet; "stale" (no good packet in 350 ms, PLACEHOLDER) only shows as `stale=` in the
  master's debug line and does not raise danger. The sequence number is not checked. Verified on hardware: after flashing, the master showed `stale=0`.
- Tilt: the MPU6050 is mounted upside down about the X axis (rest read roll -166.4, pitch -1.2, GUESS from the numbers). `MPU_SIGN_Y` and `MPU_SIGN_Z` are flipped and level offsets applied in the sketch's `FILL IN` block (calibrated 2026-11-12; the four numbers
  are not recorded here, copy them from the sketch). Not recorded: `danger=0 warn=0` at rest, and the nose up/down and left/right symmetry check. The MPU connection looked marginal (dropped out until power and ground were reseated).
- 2026-11-19 bench, GPS: NO DATA. The `# GPSDBG` counters stayed at chars=0 ok=0 bad=0 for three pin combinations (RX 36/TX 33, RX 35/TX 33, RX 33/TX 35), so no bytes reach Serial2 and it is not a baud or talker-prefix problem. Wiring as last stated: module RX wire on ESP pin 35 (so the module TX would be on 33; unconfirmed, I only had the user's word for it). Not yet checked: module power LED, VCC, shared ground, voltage on the module's TX pin, continuity to the ESP header pin. A classroom with no fix is NOT the cause (the module sends NMEA without a fix). Sketch currently has RX 33 / TX 35. Pin 35 is input-only so it cannot drive TX; harmless since we never send to the GPS.

### 8.4 Bench session 2026-11-12 (first hardware contact)

RC pulses (us) measured from the master's serial monitor:

| Channel                | Values                                                                                        |
| ---------------------- | --------------------------------------------------------------------------------------------- |
| steer (ch1)            | left 983, rest 1511, right 2051                                                               |
| throttle/brake (ch2)   | full throttle 1001 (more throttle = LOWER pulse), rest 1554, brake (pushed away) 2061         |
| mode (ch5, pin 22)     | down 982, middle 1514, up 2048 (up = auto; the 1400/1600 thresholds fit unchanged)            |
| lift (ch6)             | down 983, rest 1515, up 2048                                                                  |
| reverse (ch7, pin 34)  | about 979 off, 2045 held (the old `REVERSE_PULSE_MIN` 2000 left only 45 us of margin)         |

Proposed constants (confirm in the `FILL IN` block): `THROTTLE_PULSE_MIN` 1010, `THROTTLE_PULSE_MAX` 1500, `BRAKE_PULSE_MIN` 1900, `REVERSE_PULSE_MIN` 1700 (1500 would be wrong because the failsafe value 1512 would read as a press).

Receiver failsafe (RC6GS V3), DECIDED and verified: with the transmitter OFF the receiver keeps sending centred pulses, so the master never sees "no signal" and its stale path never fires, and at thr 1514 there is NO brake (the rover coasts).
So the failsafe is part of the safety design: ch2 set to the brake end (about 2060), ch7 set to about 980. Retested: with the transmitter off, `thr` goes above 2000 and `mbrake` becomes 1 in under 1 s (not timed more precisely; roughly up to a
second of coasting); switching back on restores both. Mode ch5 drops to the dead zone (manual), so a lost transmitter can never leave the rover in autonomous. The other channels' centred failsafe is acceptable (lift stops, steering centres).
If pulses ever stop altogether, the master's no-signal path also brakes (read from the sketch, not tested).

### 8.5 Output tests (wheels off the ground, brake actuator disconnected from the lever)

Precautions for any future stage: rover on blocks, a known fast battery cut-off with a hand near it, master on USB with the Pi unplugged, motor supplies off until the stage says otherwise. NOTE: GPIO 14 pulses at boot and feeds the brake driver's
L_EN, so keep the brake driver supply off for the first power-ups with outputs enabled.

Verified 2026-11-12 evening: the reverse relay clicks and the wheels reverse; manual throttle reaches full speed (so the 3.3 V DAC is enough for this controller); transmitter off cuts the drive quickly (not timed); lift up and down work (up needed a loose wire fixed);
the lift will not move while the throttle is on; the brake extends on the stick push (BTS7960 LPWM pin 18, as guessed), the limit switch stops it and retract works; steering follows the stick.
Not done: brake actuator connected to the lever, `STEER_AUTO_SIGN` and the steering angle calibration (the real max steer angle is still unmeasured), the Pi link, and the lift is slow (the firmware only sets two pins, so check the DIHOOL manual and its supply).
- Debug with ESPs on the Pi (decided 2026-11-18, nothing built): UART0 carries both the Pi link and debug text. Bring-up: stop the bridge,
  `ssh pi` and read the port with picocom (only one reader per port). Later: esp_bridge forwards '#' debug lines to /rover/master_debug
  (needs the 100-char line limit sorted; the current master debug line is probably longer). Bluetooth is an optional extra, not the main path.cat firmware/sensor_serial/sensor_serial.ino

### 8.6 Lift "not down" interlock (RESOLVED 2026-11-19)

- Gap found: the throttle worked with the lift not fully down, because the master never knew the lift state (the sensor sent `LIFT` only to the Pi; `LIFT,1,0` = down, `LIFT,0,1` = up, `LIFT,0,0` = between).
- Fix in progress: bit 2 (0x04) of the safety packet flags = "lift not down" (the sensor sends it from the lower limit; true = not pressed). `SafetyLink.lift_not_down` defaults to TRUE until a good packet says otherwise
  (fail-safe), and the master forces throttle 0 and brake on while it is true, in manual and auto. The API change uses default arguments, so the old C++ tests still compile.
- BUG: the master's debug field `lift_nd` stays 1 when the limit switch toggles (not explained). Suspect: the line in `safety_link_push` that stores the bit may be missing: check that first, then add C++ tests for the new bit. The interlock line in
  `master_serial.ino` was still commented out at the last note. Until fixed, the master will not drive. Recheck on the rover after reflashing both boards.
- 2026-11-17 (PC only): library, call sites and sketch copies checked; C++ tests for the bit exist and
  pass (tested). Remaining suspects: LIMIT_PRESSED_LEVEL on pin 13 (UNVERIFIED), or stale flash on
  one board. On the rover: reflash both, print digitalRead(13) while pressing the lower limit,
  watch the LIFT line on the Pi side.
- 2026-11-19 bench: sensor pins 13/14 read LOW when pressed (middle 1/1, bottom 0/1, top 1/0). After reflashing both boards, master `lift_nd` follows the lower limit with `stale=0`, so the 11-12 stuck value was stale flash, not a code bug. Interlock line re-enabled. Verified in manual mode: lift not down + full throttle gives dac 0 and brake 1; lower limit pressed gives throttle passes through; releasing the limit with throttle held cuts throttle and brakes. NOT tested: auto mode.
- Known gap: the link HOLDS the last `lift_not_down`. If the sensor board dies or its wire comes loose after reporting "down", the interlock stays open. Decide whether a stale link should force it back to true.

### 8.7 Pin map

| Master ESP | Use                                                                                      | Sensor ESP | Use                                                                         |
| ---------- | ---------------------------------------------------------------------------------------- | ---------- | --------------------------------------------------------------------------- |
| 34 / 23    | reverse ch7 in / reverse relay out                                                       | 21 / 22    | I2C to MPU6050 (default Wire pins)                                          |
| 32 / 25    | throttle+brake ch2 in / DAC throttle out                                                 | 34         | battery ADC (divider 39.8 / 2.39)                                           |
| 14, 19     | BTS7960 L_EN, R_EN (set HIGH)                                                            | 23         | override switch (pullup, LOW = alarms overridden)                           |
| 18 / 2     | BTS7960 LPWM (extends brake) / RPWM (GPIO 2 is a strapping pin)                          | 19         | warning light                                                               |
| 13         | brake limit switch (pullup, LOW = pressed)                                               | 13 / 14    | lower / upper lift limit                                                    |
| 4 / 26, 27 | lift ch6 in / lift outputs A, B (both HIGH = stop)                                       | 16 / 17    | UART to master (safety link)                                                |
| 35 / 33    | steering ch1 in / steering servo out                                                     | 32 / 33    | GPS RX 36 / TX 33 in the sketch (UNVERIFIED: wires reported there, TX unconfirmed; 36 is input-only so it can only be RX)                                    |
| 22         | mode switch in (ch5)                                                                     |            |                                                                             |
| 16, 17     | UART to sensor board                                                                     |            |                                                                             |

GPS pins are UNRESOLVED. Old sketches say 16/17 (which clashes with the safety link) or RX 3 / TX 1. The wires were reported on pins 33 and 36 with TX unconfirmed (GPIO 36 is input-only on the ESP32, so it can only be the ESP's RX; GUESS). The ESP32 has
3 hardware UARTs mappable to most free pins; the sensor board needs three links (Pi, master, GPS) and UART0 doubles as USB. Check the physical wiring, then match the pin defines.

## 9. Legacy firmware: what was learned (read from source 2026-11-08, nothing run unless noted)

**Pre-ROS firmware** (drives manually today). Master reads each RC channel with a blocking `pulseIn` plus `readStringUntil` (1 s default timeout) plus a 500 ms reverse `delay`, so throttle and steering refresh at roughly 10 Hz (estimate). Autonomy is steering only: there is no
path from a speed command to throttle, brake or reverse, and the `turnangle` it uses is never produced. Alarms are tilt only (danger at roll >= 20 or pitch >= 25, warning at roll >= 15 or pitch >= 20; no ultrasonic code). Problems found (none run):
danger does not cleanly cut throttle (warning mode runs full range after the danger block), loss of RC releases the brake, no heartbeat from the sensor board, reverse relay with no interlock, lift limits monitor-only (Bluetooth message on the press edge), pi commands overwritten every
100 ms by the sensor packet, no sync or CRC on the 10-byte sensor packet (one lost byte misaligns it for good), `mpu.begin()` called every loop (from memory: it resets the chip, so the sensor loop is slow and tilt alarms reach the master at about 2 to 3 Hz), throttle range cliff
(pulses outside 980 to 1517 give 0), a servo clamp mismatch (`attach(1000, 2000)` but 500 to 2500 written; from memory ESP32Servo clamps), `Wire.begin()` on the master claiming pin 22.

**micro-ROS firmware** (does not work; "inoperable lag", cause never found). Good ideas kept in the new firmware: interrupt RC reads with freshness, the framed CRC'd safety packet plus heartbeat, mode hysteresis, brake-always-wins, loop-time instrumentation, ROS-loss => zero speed plus brake.
Likely reasons it fails (GUESSES, from reading): GPS UART2 and the master link both mapped to pins 16/17 (the pin matrix lets only one output drive a pad, so safety packets probably never left the sensor board and the master stayed in danger); the master gives up on ROS at boot if the agent is
not up yet; no reconnect on the master; reliable QoS over a 115200 UART with about 4.5 KB/s of Imu + GPS; UART0 doubling as USB so any stray print corrupts the stream. Bugs also found there (not carried over): the `angular.z * 180/pi` yaw-rate-to-angle error, a throttle map with a dead-band discontinuity,
relay flipping while the DAC is non-zero, throttle fighting the brake retract, `safeLift` driving the lift down on a stale channel, IMU with no stamp or frame and wrong-sign orientation, the same GPS fix re-published at 10 Hz as if independent with covariance `hdop^2` (HDOP is dimensionless; real error is
roughly HDOP x 2.5 to 5 m, GUESS).

**Torque is low** (side note, cause unconfirmed). Candidates: the old 0.75 manual throttle cap (removed in the new sketch), the DAC range versus a controller that may expect up to about 4.2 V (manual full speed is now reached, so this looks fine), the throttle range cliff, and hardware not visible from code (controller current
limit and eco settings, battery sag, gearing, the large 0.159 m wheel radius). Torque follows current, so check the controller's limit first. Autonomous speed commands are open loop: Nav2 `vx_max` 0.65 maps to about 1.4 V on the DAC, and below about 0.3 m/s the voltage is probably under the controller's activation point
(GUESS); it needs a speed-vs-DAC calibration.

## 10. Open questions and known issues

Hardware and firmware
- Sensor sketch `IMU` and `GPS` lines are written but unverified. Bench checks needed: IMU axis mapping (move the rover by hand and note which of gx, gy, gz responds and its sign; yaw left should give positive gz), gyro near 0 and accel magnitude near 9.8 at rest, the 13 deg mounting offset not applied to the IMU line, GPS wire pins, whether the module sends `$GPGGA` or `$GNGGA`, whether `TinyGPSCustom` fires once per fix, and the real baud rate.
- 2026-11-19 bench: yaw sign verified (left turn by hand gives gz up to +0.34 rad/s; gx, gy stay under 0.09, consistent with the board's tilt leaking into them). Assumes the turn was left (counterclockwise). Mounting tilt still not applied to the IMU line.
- `STEER_AUTO_SIGN`, real max steer angle, brake actuator connected to the lever, lift speed, the throttle controller and brake driver model numbers, supplies and over-voltage risks, an emergency cut-off, the transmitter-loss brake delay of up to about 1 s (acceptable?).
- RPLIDAR variant (30 m or 18 m), exact ultrasonic model, real GPS antenna position (needs a mast out of the LIDAR plane), real mass, clearance and sensor positions; the DIHOOL lift travel versus the xacro; do the wheels sit outside the chassis.
- Pi: UART names and `config.txt` overlays, CPU load (rf2o, MPPI, Smac's 29 s configure, SLAM if used), whether EKF update-rate warnings repeat, `rosdep` on a real Pi.
- The serial baud and 350 ms timeout are placeholders. 

Localization and navigation
- Heading source for GPS fusion: DECIDED 2026-11-18 (design only, nothing built). GPS-pin navigation stays a goal even under
  canopy with noisy GPS. No calibration drive: the rover starts toward the pin on a guessed heading (yaw 0 = east) and keeps
  correcting. A Pi-side node fits odom displacement (rf2o + gyro) against GPS displacement (east/north) with a running
  least-squares fit, weighted so the estimate barely moves until the rover has travelled several times the GPS error.
  Corrections are applied only in the goal node (pin lat/lon -> odom frame); the EKFs and navsat_transform are left alone.
  Goals are re-sent only when the estimate changes by more than about 10 deg or the pin moves more than a couple of meters.
  Goal tolerance sized to GPS error (5 to 10 m, GUESS). Odom-frame roaming and avoidance do not need heading.
  Magnetometer rejected for now (motor and 36 V wiring interference); dual NEO-6M GPS rejected (baseline too short).
  Sim IMU stays a perfect compass: the sim is not degraded, the flaws are known.
- Why rf2o produces impossible speeds (the gate only hides it); how rf2o behaves on rough, cluttered forest terrain; a second stuck-detection signal.
- Realistic sim IMU noise; tune covariances (GPS, rf2o, IMU) against real data; real steer angle for `minimum_turning_radius`; Nav2 behavior when `/scan_filtered` goes silent.
- Remaining roaming aborts (`Start occupied`, planner timeouts); retest the inflation sliders; sliders reading real values at startup.

## 11. Next steps (in order)

1. DONE 2026-11-19: lift interlock verified (manual). Open: auto-mode check, stale-link hold decision (8.6).
2. GPS wiring: no bytes arrive. Check power LED, VCC, GND, voltage on the module TX pin, continuity to the ESP pin, then add an edge counter on the RX pin if still silent. Then confirm $GPGGA vs $GNGGA and one line per fix outdoors.
3. IMU: decide whether to apply the mounting offset (13 deg roll, about 12 deg pitch if the rover was level) to the IMU line; check accel scale.
4. Pi link (3a to 3e above), with DEBUG_PRINT 0 first.
5. Real-hardware localization: RPLIDAR driver, heading estimate (see section 10), BAT divider check.
6. Remaining output tests: brake on the lever, STEER_AUTO_SIGN, lift speed; then field tests.
7. Merge serial-protocol and tag serial-protocol-v1.
8. Later: stuck detector, roaming aborts, inflation-slider retest, Pi CPU checks.

## 12. Working agreements (for any assistant working with the maintainer)

- ONE step at a time; never a long multi-step list. Wait for the output before the next step. When editing files, show only the lines that change unless a full file is asked for.
- The maintainer types all code to learn it: explain what each part does, briefly. New to ROS, weak in terminal and C++; Python is fine; editor is LazyVim (`nvim`); give commands for zsh.
- Branch names are descriptive, never phase numbers. Say when to commit, push, branch and merge; tag before deleting a merged branch.
- Say when something is a guess or cannot be verified; do not state it as fact. Do not over-verify: check only what can actually break something.
- Keep this file current as work progresses (it is the project's "second brain") and update `NOTES.md` too.

## 13. Ideas for later (not committed to)

- Terrain from a Gazebo heightmap (USGS elevation) plus a SLAM map of the real site. The Bob Marshall is designated wilderness (motorized equipment is generally barred; check with the Forest Service) and Lubrecht is a UM research forest (ask them).
- Hall sensors or encoders if rf2o proves weak in a real forest. Making the sim lift controllable (Gazebo joint controller and a bridged command topic).
- Portfolio: Gazebo cannot run in a browser; use screen-recorded video, a README with an architecture diagram and test results, and the repo link. Decide later whether to make the repo public.
