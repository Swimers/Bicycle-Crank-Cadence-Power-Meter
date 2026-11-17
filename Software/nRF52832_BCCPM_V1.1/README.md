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
