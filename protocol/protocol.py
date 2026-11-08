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


reader = LineReader()


def split_body(body):
    parts = body.split(",")
    return parts[0], parts[1:]


a = make_line("CMD", 0.3, 12.5, 0.0)
b = make_line("BAT", 12.4)

print(split_body("CMD,0.3,12.5,0.0"))
print(split_body("BAT,12.4"))
print(split_body("MODE"))
