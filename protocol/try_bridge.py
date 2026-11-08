import time
from bridge import Bridge

b = Bridge("/tmp/ttyROVER_PI")
last_send = 0.0

while True:
    for kind, fields in b.poll():
        print("Pi got:", kind, fields)

    now = time.time()
    if now - last_send >= 0.5:
        b.send_cmd(0.3, 12.54, 0.0)
        last_send = now

    time.sleep(0.01)
