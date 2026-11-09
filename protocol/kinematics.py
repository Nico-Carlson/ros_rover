import math


def twist_to_steer_deg(v, wz, wheelbase=0.686, max_deg=30.0, stop_speed=0.05):
    """Turn a speed v (m/s) and yaw rate wz (rad/s) into a steering angle in degrees.

    Positive = left (the ROS convention). Bicycle model: wz = v * tan(delta) / wheelbase,
    so delta = atan(wheelbase * wz / v).
    """
    if not (math.isfinite(v) and math.isfinite(wz)):
        return 0.0
    if abs(v) <= stop_speed:  # an Ackermann rover cannot turn on the spot
        return 0.0
    delta = math.degrees(math.atan(wheelbase * wz / v))
    return max(-max_deg, min(max_deg, delta))
