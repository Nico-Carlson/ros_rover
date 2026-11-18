from protocol import parse_imu, parse_gps


def test_imu_valid():
    fields = ["0.10", "0.00", "9.81", "0.000", "0.000", "0.200"]
    assert parse_imu(fields) == (0.10, 0.0, 9.81, 0.0, 0.0, 0.2)


def test_imu_wrong_count():
    assert parse_imu([]) is None
    assert parse_imu(["0.10", "0.00", "9.81"]) is None
    assert parse_imu(["1"] * 7) is None


def test_imu_not_a_number():
    assert parse_imu(["0.10", "0.00", "abc", "0", "0", "0"]) is None


def test_imu_not_finite():
    for bad in ("nan", "inf", "-inf"):
        assert parse_imu([bad, "0", "0", "0", "0", "0"]) is None


def test_gps_valid_fix():
    fields = ["1", "9", "48.1234567", "-114.1234567", "900.5", "1.20"]
    assert parse_gps(fields) == (True, 9, 48.1234567, -114.1234567, 900.5, 1.2)


def test_gps_no_fix_still_parses():
    result = parse_gps(["0", "0", "0.0000000", "0.0000000", "0.0", "99.00"])
    assert result is not None
    assert result[0] is False


def test_gps_bad_fix_flag():
    assert parse_gps(["2", "9", "48.0", "-114.0", "900.5", "1.2"]) is None
    assert parse_gps(["yes", "9", "48.0", "-114.0", "900.5", "1.2"]) is None


def test_gps_wrong_count():
    assert parse_gps([]) is None
    assert parse_gps(["1", "9", "48.0", "-114.0", "900.5"]) is None
    assert parse_gps(["1", "9", "48.0", "-114.0", "900.5", "1.2", "7"]) is None


def test_gps_bad_satellites():
    for bad in ("x", "-1", "9.5"):
        assert parse_gps(["1", bad, "48.0", "-114.0", "900.5", "1.2"]) is None


def test_gps_out_of_range():
    assert parse_gps(["1", "9", "91.0", "-114.0", "900.5", "1.2"]) is None
    assert parse_gps(["1", "9", "48.0", "181.0", "900.5", "1.2"]) is None
    assert parse_gps(["1", "9", "48.0", "-114.0", "900.5", "-1.0"]) is None


def test_gps_not_finite():
    assert parse_gps(["1", "9", "nan", "-114.0", "900.5", "1.2"]) is None
    assert parse_gps(["1", "9", "48.0", "-114.0", "inf", "1.2"]) is None
