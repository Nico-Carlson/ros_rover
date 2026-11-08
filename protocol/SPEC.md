# Rover serial protocol (DRAFT, all values PLACEHOLDER, UNTESTED ON HARDWARE)

Line format: `TYPE,field,field,...*CS\n`
CS = XOR of every character before the `*`, two uppercase hex digits.
Max line length: 100 characters. Bad or missing checksum: drop the line.

## Pi -> ESP

CMD,speed,steer,lift
- speed: m/s, float (negative = reverse)
- steer: steering angle in degrees, float (computed on the Pi)
- lift: lift speed, float

## ESP -> Pi

MODE,auto,danger,warning,brake
- auto: 1 = autonomous switch on, 0 = manual
- danger: 1 or 0
- warning: 1 or 0
- brake: 1 = applied, 0 = released

LIFT,down,up
- down: 1 = lower limit switch pressed (lift fully down), 0 = not pressed
- up: 1 = upper limit switch pressed (lift fully up), 0 = not pressed
- Sent continuously (PLACEHOLDER 10 Hz). The ESP converts pin polarity; the Pi sees only 1 or 0.
- Pi rule: /lift_down is true ONLY when the latest LIFT line is under ~1 s old (PLACEHOLDER,
  matches scan gate `lift_timeout`), down is 1, and up is 0. Anything else = not down.

BAT,volts
- volts: battery voltage, float, one decimal
- Sent at PLACEHOLDER 1 Hz.
