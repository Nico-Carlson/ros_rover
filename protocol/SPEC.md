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
