import sys
import time
import serial
from protocol import LineReader, make_line

port = sys.argv[1]
ser = serial.Serial(port, 115200, timeout=0)
reader = LineReader()
last_send = 0.0

print("fake ESP listening on", port)
while True:
    data = ser.read(100)
    if data:
        text = data.decode("ascii", errors="ignore")
        for body in reader.feed(text):
            print("ESP got:", body)

    now = time.time()
    if now - last_send >= 1.0:
        ser.write(make_line("MODE", 0, 0, 0, 0).encode("ascii"))
        last_send = now

    time.sleep(0.01)
