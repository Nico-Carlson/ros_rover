# Rover serial protocol (DRAFT, all values PLACEHOLDER, UNTESTED ON HARDWARE)

Line format: `TYPE,field,field,...*CS\n`
CS = XOR of every character before the `*`, two uppercase hex digits.
Max line length: 100 characters. Bad or missing checksum: drop the line.

## Pi -> ESP

CMD,speed,steer,lift
- speed: m/s, float (negative = reverse)
- steer: steering angle in degrees, float, positive = left (computed on the Pi, clamped to +-30, PLACEHOLDER)
- lift: lift speed, float
There is no DANGER message from the Pi. To stop, send CMD with speed 0; the master also brakes
when CMD goes stale (PLACEHOLDER 500 ms). Danger from the sensor board reaches the master over
the safety link, not through the Pi.

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

IMU,ax,ay,az,gx,gy,gz
- ax, ay, az: acceleration in m/s^2, float, two decimals (body frame, x forward, y left, z up)
- gx, gy, gz: angular rate in rad/s, float, three decimals (same axes)
- The ESP applies the sensor scaling, axis mapping and gyro bias; the Pi sees SI units only.
- Sent from the sensor ESP at PLACEHOLDER 50 Hz. No timestamp: the Pi stamps it on receipt.
- The MPU6050 has no compass, so there is no orientation.

GPS,fix,sats,lat,lon,alt,hdop
- fix: 1 = valid fix, 0 = none
- sats: satellites used, integer
- lat, lon: decimal degrees, float, 7 decimals (negative = S / W)
- alt: metres above mean sea level, float, one decimal
- hdop: horizontal dilution of precision, float, two decimals
- The ESP parses the module's NMEA; the Pi never sees raw NMEA. Sent once per new fix
  (PLACEHOLDER 1 to 5 Hz, whatever the module outputs). Longest case is about 55 characters.
- Pi rule: fix = 0 means publish nothing on /gps/fix. Covariance is computed on the Pi from hdop.
