# BCCPM V1.1 board

The board target is `bccpm/nrf52832`, using NCS 3.2.4 and sdk-ant 2.1.1.
For sysbuild, pass the application directory as `BOARD_ROOT` before board
discovery (replace the path as needed):

```powershell
west build -p always -b bccpm/nrf52832 -d build-bccpm . -- -DBOARD_ROOT=D:/Documents/GitHub/CadenceSensor/Software/nRF52832_BCCPM_V1.1
```

The LSM6DSR and ADS1220 bindings describe wiring only; device drivers and
sampling logic are not implemented. IMU interrupt active-high flags are
placeholders until the sensor interrupt registers and polarity are configured.
The battery-enable and power-off values are raw GPIO0 pin numbers, not logical
GPIO specifications: their polarity and timing require hardware verification.
GPIO output safe states are not initialized by the device tree or empty app.
Verify SWD access, reset UICR programming, LF crystal, battery switch, ship
sequence, bridge settling, ADC reference and sensor communications on hardware.
No board UART or USB console is configured.

## Six-axis sampling

Include `app/imu.h`. Call `imu_init()` once in thread context, then call
`imu_sample_read(&sample, timeout_ms)` whenever a fresh sample is needed.
Both functions return zero on success or a negative errno; a zero timeout
returns `-EAGAIN` when no sample is ready, while a positive timeout returns
`-ETIMEDOUT`. Serialize calls from different threads. A failed read does not
modify the output structure.

The fixed profile is 104 Hz, +/-4 g and +/-1000 degrees/s; the returned XYZ
axes are uncalibrated sensor-frame values in g and degrees/s. `timestamp_ms`
is host uptime after the bus read, not a sensor timestamp. BDU protects each
16-bit word but does not guarantee simultaneous six-axis measurements. Poll
at an appropriate rate: this interface reads the latest outputs, not FIFO
history, and cannot report samples missed between calls. The +/-1000 dps range
can saturate above about 167 RPM for a perfectly aligned crank axis; validate
the required range for the application. Interrupts, FIFO and calibration are
not enabled. Hardware identity, startup settling and readings remain to be
verified on the board.
