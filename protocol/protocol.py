def checksum(body):
    value = 0
    for char in body:
        value ^= ord(char)
    return f"{value:02X}"


def parse_line(line):
    line = line.strip()
    if "*" not in line:
        return None
    body, received_cs = line.rsplit("*", 1)
    if received_cs != checksum(body):
        return None
    return body


def make_line(*fields):
    body = ",".join(str(f) for f in fields)
    return f"{body}*{checksum(body)}\n"


class LineReader:
    def __init__(self, max_len=100):
        self.buffer = ""
        self.max_len = max_len

    def feed(self, data):
        lines = []
        for char in data:
            if char == "\n":
                body = parse_line(self.buffer)
                if body is not None:
                    lines.append(body)
                self.buffer = ""
            else:
                self.buffer += char
                if len(self.buffer) > self.max_len:
                    self.buffer = ""
        return lines


def split_body(body):
    parts = body.split(",")
    return parts[0], parts[1:]


def make_cmd(speed, steer_deg, lift):
    return make_line("CMD", f"{speed:.2f}", f"{steer_deg:.1f}", f"{lift:.2f}")


def make_lift(down, up):
    return make_line("LIFT", int(bool(down)), int(bool(up)))


def parse_mode(fields):
    """fields = the list after the 'MODE' name.
    Returns (autonomous, danger, warning, brake) as bools, or None if malformed."""
    if len(fields) != 4:
        return None

    flags = []
    for f in fields:
        if f == "0":
            flags.append(False)
        elif f == "1":
            flags.append(True)
        else:
            return None
    return tuple(flags)
