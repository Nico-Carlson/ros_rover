import os
import subprocess
import time

import pytest
import serial

from bridge import Bridge
from protocol import LineReader

PI_PORT = "/tmp/ttyTEST_PI"
ESP_PORT = "/tmp/ttyTEST_ESP"


@pytest.fixture
def link():
    proc = subprocess.Popen(
        ["socat", "pty,raw,echo=0,link=" + PI_PORT, "pty,raw,echo=0,link=" + ESP_PORT],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    for _ in range(50):
        if os.path.exists(PI_PORT) and os.path.exists(ESP_PORT):
            break
        time.sleep(0.1)
    else:
        proc.kill()
        pytest.fail("socat did not create the virtual ports")
    bridge = Bridge(PI_PORT)
    esp = serial.Serial(ESP_PORT, 115200, timeout=0)
    yield bridge, esp
    bridge.ser.close()
    esp.close()
    proc.terminate()
    proc.wait()


def collect_pi(bridge, seconds=0.3):
    got = []
    end = time.time() + seconds
    while time.time() < end:
        got += bridge.poll()
        time.sleep(0.01)
    return got


def collect_esp(esp, seconds=0.3):
    reader = LineReader()
    got = []
    end = time.time() + seconds
    while time.time() < end:
        data = esp.read(200)
        if data:
            got += reader.feed(data.decode("ascii", errors="ignore"))
        time.sleep(0.01)
    return got


def test_good_line_reaches_pi(link):
    bridge, esp = link
    esp.write(b"BAT,12.4*62\n")
    assert collect_pi(bridge) == [("BAT", ["12.4"])]


def test_bad_lines_dropped_and_link_recovers(link):
    bridge, esp = link
    esp.write(b"garbage\nCMD,0.4,12.5,0.0*7D\nBAT,12.4*62\n")
    assert collect_pi(bridge) == [("BAT", ["12.4"])]


def test_cmd_reaches_esp(link):
    bridge, esp = link
    bridge.send_cmd(0.3, 12.54, 0.0)
    assert collect_esp(esp) == ["CMD,0.30,12.5,0.00"]


def test_lift_line_updates_state(link):
    from bridge import LiftState

    bridge, esp = link
    state = LiftState()
    esp.write(b"LIFT,1,0*16\n")
    end = time.time() + 0.3
    while time.time() < end:
        bridge.poll_with_lift(state)
        time.sleep(0.01)
    assert state.lift_down(time.time()) is True


def test_lift_goes_not_down_when_esp_silent(link):
    from bridge import LiftState

    bridge, esp = link
    state = LiftState(timeout=0.3)
    esp.write(b"LIFT,1,0*16\n")
    end = time.time() + 0.15
    while time.time() < end:
        bridge.poll_with_lift(state)
        time.sleep(0.01)
    assert state.lift_down(time.time()) is True
    time.sleep(0.4)
    bridge.poll_with_lift(state)
    assert state.lift_down(time.time()) is False
