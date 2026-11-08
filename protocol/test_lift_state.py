from bridge import LiftState


def test_no_line_yet_is_not_down():
    assert LiftState().lift_down(0.0) is False


def test_fresh_down_line_is_down():
    s = LiftState()
    s.update(["1", "0"], now=10.0)
    assert s.lift_down(10.5) is True


def test_stale_line_is_not_down():
    s = LiftState()
    s.update(["1", "0"], now=10.0)
    assert s.lift_down(11.5) is False


def test_lift_not_down():
    s = LiftState()
    s.update(["0", "0"], now=10.0)
    assert s.lift_down(10.1) is False


def test_both_pressed_is_fault():
    s = LiftState()
    s.update(["1", "1"], now=10.0)
    assert s.lift_down(10.1) is False


def test_malformed_line_ignored():
    s = LiftState()
    s.update(["1", "0"], now=10.0)
    s.update(["1"], now=10.9)
    s.update(["2", "0"], now=10.9)
    assert s.lift_down(11.5) is False
