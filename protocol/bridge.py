import serial
import time
from protocol import LineReader, make_cmd, split_body


class Bridge:
    def __init__(self, port, baud=115200):
        self.ser = serial.Serial(port, baud, timeout=0)
        self.reader = LineReader()

    def poll(self):
        """Read whatever has arrived. Returns a list of (kind, fields) pairs."""
        data = self.ser.read(200)
        if not data:
            return []
        text = data.decode("ascii", errors="ignore")
        return [split_body(body) for body in self.reader.feed(text)]

    def send_cmd(self, speed, steer_deg, lift):
        self.ser.write(make_cmd(speed, steer_deg, lift).encode("ascii"))

    def poll_with_lift(self, lift_state):
        """Like poll(), but also feeds LIFT lines into lift_state."""
        messages = self.poll()
        now = time.time()
        for kind, fields in messages:
            if kind == "LIFT":
                lift_state.update(fields, now)
        return messages


class LiftState:
    def __init__(self, timeout=1.0):
        self.timeout = timeout
        self.down = False
        self.up = False
        self.last_time = None

    def update(self, fields, now):
        """fields is the list from a LIFT line, e.g. ['1', '0']."""
        if (
            len(fields) != 2
            or fields[0] not in ("0", "1")
            or fields[1] not in ("0", "1")
        ):
            return
        self.down = fields[0] == "1"
        self.up = fields[1] == "1"
        self.last_time = now

    def lift_down(self, now):
        if self.last_time is None:
            return False
        if now - self.last_time > self.timeout:
            return False
        return self.down and not self.up
