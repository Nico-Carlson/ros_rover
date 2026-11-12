import pytest
from protocol import parse_mode


def test_all_off():
    assert parse_mode(["0", "0", "0", "0"]) == (False, False, False, False)


def test_all_on():
    assert parse_mode(["1", "1", "1", "1"]) == (True, True, True, True)


@pytest.mark.parametrize("position", [0, 1, 2, 3])
def test_each_field_maps_to_its_own_position(position):
    fields = ["0", "0", "0", "0"]
    fields[position] = "1"
    result = parse_mode(fields)
    expected = [False, False, False, False]
    expected[position] = True
    assert result == tuple(expected)


@pytest.mark.parametrize(
    "fields",
    [
        [],
        ["1"],
        ["1", "0", "0"],
        ["1", "0", "0", "0", "0"],
    ],
)
def test_wrong_field_count_is_rejected(fields):
    assert parse_mode(fields) is None


@pytest.mark.parametrize("bad", ["2", "", "true", "1.0", "-1", " 1"])
def test_bad_value_is_rejected(bad):
    assert parse_mode(["0", bad, "0", "0"]) is None
