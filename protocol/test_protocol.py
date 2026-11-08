from protocol import make_line, parse_line, split_body, LineReader


def test_round_trip():
    line = make_line("CMD", 0.3, 12.5, 0.0)
    assert parse_line(line) == "CMD,0.3,12.5,0.0"


def test_bad_checksum_dropped():
    assert parse_line("CMD,0.3,12.5,0.0*00\n") is None


def test_missing_checksum_dropped():
    assert parse_line("CMD,0.3,12.5,0.0\n") is None


def test_split_no_fields():
    assert split_body("MODE") == ("MODE", [])


def test_reader_half_line():
    reader = LineReader()
    line = make_line("BAT", 12.4)
    assert reader.feed(line[:5]) == []
    assert reader.feed(line[5:]) == ["BAT,12.4"]


def test_reader_overflow_recovers():
    reader = LineReader(max_len=20)
    good = make_line("BAT", 12.4)
    assert reader.feed("x" * 50 + "\n") == []
    assert reader.feed(good) == ["BAT,12.4"]
