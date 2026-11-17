# BCCPM V1.1 board

The board target is `bccpm/nrf52832`, using NCS 3.2.4 and sdk-ant 2.1.1.
For sysbuild, pass the application directory as `BOARD_ROOT` before board
discovery (replace the path as needed):

```powershell
west build -p always -b bccpm/nrf52832 -d build-bccpm . -- -DBOARD_ROOT=D:/Documents/GitHub/CadenceSensor/Software/nRF52832_BCCPM_V1.1
```

IMU interrupt active-high flags are placeholders until the sensor interrupt
registers and polarity are configured. The power-off GPIO's effect on Ship
mode and its timing require hardware verification.
The main entry initializes board functions through `board_io_init()`.
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

## Bridge ADC sampling

Include `app/bridge_adc.h`, call `bridge_adc_init()` once, then call
`bridge_adc_sample_read(&sample, timeout_ms)` from serialized thread context.
The fixed ADS1220 profile uses AIN0-AIN1, gain 16, external REFP0/REFN0,
normal 20 SPS and single-shot conversion. Initialization enables bridge
excitation continuously, waits 100 ms and discards the first conversion.
The 100 ms settling delay is provisional and must be verified on hardware.
`bridge_adc_set_enabled(false)` switches excitation off; `true` switches it
on, waits for settling and discards a conversion. Switching while a conversion
is pending returns `-EBUSY`; collect the pending conversion first. A failed
first conversion after re-enabling requires `bridge_adc_init()` again. Do not
mix direct ADS1220 bridge control with the application sampling interface.

Each read starts or collects one conversion. Zero timeout returns `-EAGAIN`
while pending; a positive timeout returns `-ETIMEDOUT` without canceling the
conversion, so the next call collects it. SPI/GPIO errors propagate as negative
errno; a later call starts a new conversion. The output is unchanged on error.
The sample contains signed raw code, uncalibrated `input_over_reference`
(raw / (2^23 * 16)), and host read time. This ratio is not a voltage or force
calibration. Reference voltage, PGA common-mode range, settling time, bridge
noise and conversion values must be checked on the actual board. The current
application does not automatically call the sampling interface.

## Battery measurement

Call `power_battery_read()` after `board_io_init()` in serialized thread
context; see `app/power.h`. The sample contains battery millivolts and an
approximate percentage. The active-high P0.06 enable is inferred from the Q1
schematic; measurement turns it on for 20 ms, discards one SAADC reading,
averages four readings and turns it off. The software reverses the nominal
1.5 Mohm / 220 kohm divider. GPIO and ADC errors return negative errno without
changing the output. The application does not call this interface automatically.

The percentage curve is a provisional approximation for an unloaded single
4.2 V Li-ion cell, not a fuel gauge or protection threshold. Actual voltage
depends on load, charging, temperature, cell chemistry and age. Verify Q1
polarity, high-impedance ADC acquisition and settling against a multimeter on
hardware, then calibrate resistor ratio and the curve for the actual cell.

`power_init()` configures MODE, ISET and POWER_OFF low. `power_set_mode()`,
`power_set_iset()` and `power_set_power_off()` control their physical levels.
ISET selects a VBUS input limit, not battery charge current. The POWER_OFF
function is only a GPIO operation: diode drop, SHPACT levels, SHPHLD, VBUS and
required hold time must be verified before relying on it to enter Ship mode.

## Board functions

`board_io_init()` initializes power, SW5 and the buzzer and enables the bridge
by default; call it before starting bridge sampling, not during operation. The main
entry calls it once. Use `board_io_button_pressed()` to read SW5: it compares
two samples 20 ms apart and returns `-EAGAIN` if they differ. This is a
blocking stability check, not an event queue. `board_io_buzzer_set(true)`
starts 4 kHz PWM at 50% duty, and `false` stops it. To set another frequency
and duty, call `board_io_buzzer_configure(frequency_hz, duty_percent)`; the
PWM controller may reject unsupported periods. `board_io_buzzer_set()` always
uses the default frequency and duty. Serialize these calls in
thread context. ADC DRDY, sensor interrupts, power control and bridge enable
remain assigned to their respective modules; reset remains a hardware reset.
Validate PWM output, acoustic operation and button behavior on the board.
