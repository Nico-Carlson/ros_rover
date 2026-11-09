from kinematics import twist_to_steer_deg


def test_straight():
    assert twist_to_steer_deg(0.5, 0.0) == 0.0


def test_left_turn_is_positive():
    assert abs(twist_to_steer_deg(0.5, 0.3) - 22.37) < 0.01


def test_right_turn_is_negative():
    assert abs(twist_to_steer_deg(0.5, -0.3) + 22.37) < 0.01


def test_reverse_flips_the_sign():
    assert abs(twist_to_steer_deg(-0.5, 0.3) + 22.37) < 0.01


def test_clamps_to_max():
    assert twist_to_steer_deg(0.5, 1.0) == 30.0  # unclamped it would be 53.9
    assert twist_to_steer_deg(0.5, -1.0) == -30.0


def test_no_steering_when_stopped():
    assert twist_to_steer_deg(0.0, 1.0) == 0.0
    assert twist_to_steer_deg(0.05, 1.0) == 0.0  # exactly the stop band


def test_garbage_gives_straight():
    assert twist_to_steer_deg(float("nan"), 0.3) == 0.0
    assert twist_to_steer_deg(0.5, float("inf")) == 0.0
