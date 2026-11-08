import serial
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
