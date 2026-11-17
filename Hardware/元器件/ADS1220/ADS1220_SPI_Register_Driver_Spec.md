# ADS1220 SPI Register and Driver Specification

> Target: nRF52832 + Zephyr RTOS. Source of truth: Texas Instruments ADS1220 datasheet, ZHCSBH5D (May 2013, revised May 2026), especially sections 6.6, 8.4, 8.5, 8.6, 9.1.6 and 9.2.3. This is a driver-implementation reference, not an assertion about the actual PCB pin mapping. All four configuration registers are 8-bit, read/write, reset to `0x00` and retained in POWERDOWN. There is no device-identification register among these four configuration registers.

## 1. SPI electrical and transaction requirements

| Property | Required value / behavior | Datasheet |
|---|---|---|
| SPI mode | Mode 1: CPOL=0, CPHA=1 (only supported mode) | p.36 |
| Word / bit order | 8 bits, MSB first | pp.36-38 |
| SCLK idle | Low | p.36 |
| DIN capture | ADS1220 samples DIN on falling edge of SCLK | p.36 |
| DOUT change | ADS1220 shifts DOUT on rising edge of SCLK | p.36 |
| CS | Active low; hold low throughout a complete command + payload | p.36 |
| CS high | Resets serial-interface framing, ignores SCLK, DOUT becomes Hi-Z | p.36 |
| DRDY | Dedicated active-low data-ready output; remains usable while CS is high | p.36 |
| DOUT/DRDY | Optional shared ready signal if REG3.DRDYM=1; unavailable with CS high | pp.36,46 |
| CS fall to first SCLK rise | At least 50 ns | p.9 |
| Last SCLK fall to CS rise | At least 25 ns | p.9 |
| CS high width | At least 50 ns | p.9 |
| SCLK period | At least 150 ns | p.9 |
| SCLK high and low time | Each at least 60 ns | p.9 |
| DIN setup / hold around falling edge | At least 50 ns / 25 ns | p.9 |
| Development SPI speed | 1 MHz is a conservative suggested starting point, not a datasheet requirement | Recommendation |

A single `spi_transceive_dt()` is preferable for an RREG read. A single `spi_write_dt()` can carry the complete WREG command and all configuration bytes. Do not allow automatic CS deassertion between command byte and its data. Do not assume I2C-style direct register addressing is supported.

SPI interface timeout if a command is incomplete: `14000 * tMOD` in normal/duty-cycle mode or `28000 * tMOD` in turbo mode; `tMOD = 1/fMOD`. At the nominal clocks these both correspond to approximately 54.7 ms. Do not split a command over long delays.

## 2. Command set (NOT configuration registers)

| Command | Binary form | Recommended opcode | Purpose |
|---|---|---|---|
| RESET | `0000 011x` | `0x06` | Restore all configuration registers to `0x00` |
| START/SYNC | `0000 100x` | `0x08` | Start or restart conversion; reset conversion filter when restarted |
| POWERDOWN | `0000 001x` | `0x02` | Power down analog circuits; preserve configuration registers |
| RDATA | `0001 xxxx` | `0x10` | Load and read most recently completed conversion |
| RREG | `0010 rrnn` | `0x20 | (addr << 2) | (count-1)` | Read sequential registers |
| WREG | `0100 rrnn` | `0x40 | (addr << 2) | (count-1)` | Write sequential registers |

For RREG/WREG: `addr` is the 2-bit starting register address (`0..3`); `count` is `1..4`; `count - 1` is encoded as `nn`. Validate `addr + count <= 4`. The `x` bits are don't-care as specified in the datasheet. Write completion occurs at the last SCLK falling edge. An RREG reply begins **after** the opcode byte, not during that first byte.

### Typical command bytes

| Task | Bytes sent (MOSI / DIN) | Important received byte positions |
|---|---|---|
| Reset | `06` | Ignore simultaneous DOUT |
| Start | `08` | Ignore simultaneous DOUT |
| Power down | `02` | Ignore simultaneous DOUT |
| Read REG0 only | `20 00` | REG0 is received at byte index 1 |
| Read REG1 only | `24 00` | REG1 is received at byte index 1 |
| Read REG2 only | `28 00` | REG2 is received at byte index 1 |
| Read REG3 only | `2C 00` | REG3 is received at byte index 1 |
| Read all four | `23 00 00 00 00` | REG0..REG3 at received indices 1..4 |
| Read REG1..REG3 | `26 00 00 00` | REG1..REG3 at received indices 1..3 |
| Write REG0 only (`0x3E`) | `40 3E` | Not a read transaction |
| Write all four (TI bridge example) | `43 3E 04 98 00` | Not a read transaction |
| RDATA read | `10 00 00 00` | Data at received indices 1..3 |
| Direct data read, after DRDY | `00 00 00` | Data at received indices 0..2 |

RESET recovery: wait at least `50 us + 32*tCLK` before the next command. After chip power-up, allow power supplies and power-on reset to finish (TI example uses at least 50 us before RESET). Continuous conversion requires one START/SYNC **after** configuring `REG1.CM=1`; merely setting CM does not start the ADC. In single-shot mode, send START/SYNC for each conversion. POWERDOWN preserves register contents and START/SYNC resumes previous configuration; when POWERDOWN is sent mid-conversion, the ADC completes that conversion before entering powerdown (datasheet p.38).

## 3. Register map summary

| Address | Name | Bits 7..0 | Reset |
|---|---|---|---|
| `0x00` | REG0 | `MUX[3:0]` [7:4], `GAIN[2:0]` [3:1], `PGA_BYPASS` [0] | `0x00` |
| `0x01` | REG1 | `DR[2:0]` [7:5], `MODE[1:0]` [4:3], `CM` [2], `TS` [1], `BCS` [0] | `0x00` |
| `0x02` | REG2 | `VREF[1:0]` [7:6], `50/60[1:0]` [5:4], `PSW` [3], `IDAC[2:0]` [2:0] | `0x00` |
| `0x03` | REG3 | `I1MUX[2:0]` [7:5], `I2MUX[2:0]` [4:2], `DRDYM` [1], `RESERVED` [0] | `0x00` |

All enumerated field values below are **unshifted** values; assemble into the register using `field << shift`.

## 4. REG0 (address 0x00): analog input routing and gain

Fields: `MUX[3:0]` bit [7:4], default 0; `GAIN[2:0]` bit [3:1], default 0; `PGA_BYPASS` bit [0], default 0.

### MUX[3:0], bits 7..4 (datasheet Table 8-10, p.43)

| Code | Selection / meaning |
|---|---|
| `0x0` | AINP=AIN0, AINN=AIN1 |
| `0x1` | AINP=AIN0, AINN=AIN2 |
| `0x2` | AINP=AIN0, AINN=AIN3 |
| `0x3` | AINP=AIN1, AINN=AIN2 |
| `0x4` | AINP=AIN1, AINN=AIN3 |
| `0x5` | AINP=AIN2, AINN=AIN3 |
| `0x6` | AINP=AIN1, AINN=AIN0 |
| `0x7` | AINP=AIN3, AINN=AIN2 |
| `0x8` | AINP=AIN0, AINN=AVSS; PGA must be bypassed |
| `0x9` | AINP=AIN1, AINN=AVSS; PGA must be bypassed |
| `0xA` | AINP=AIN2, AINN=AVSS; PGA must be bypassed |
| `0xB` | AINP=AIN3, AINN=AVSS; PGA must be bypassed |
| `0xC` | Internal monitor: selected external VREF differential / 4; PGA bypass |
| `0xD` | Internal monitor: (AVDD - AVSS) / 4; PGA bypass |
| `0xE` | Short AINP and AINN internally to (AVDD + AVSS) / 2; useful for offset calibration |
| `0xF` | RESERVED: do not use |

### GAIN[2:0], bits 3..1

| Code | Total gain |
|---|---|
| `0b000` | 1 |
| `0b001` | 2 |
| `0b010` | 4 |
| `0b011` | 8 |
| `0b100` | 16 |
| `0b101` | 32 |
| `0b110` | 64 |
| `0b111` | 128 |

### PGA_BYPASS, bit 0

- `0`: PGA enabled (default).
- `1`: disable/bypass low-noise PGA. Only valid when gain is 1, 2 or 4.
- For gain >= 8, PGA is always enabled irrespective of this bit.
- Single-ended modes AINN=AVSS (MUX 8..B) require PGA_BYPASS=1, and gain must be 1, 2 or 4.
- When PGA is enabled, verify common-mode limits and full-scale range. Approximate ideal differential full-scale `+/- VREF / gain`, additionally constrained by input common mode and supply rails. See datasheet pp.21-25.

Examples: `MUX=3, GAIN=128, bypass=0` => `(3<<4)|(7<<1)|0 = 0x3E`; `MUX=0, GAIN=16, bypass=0` => `0x08`.

## 5. REG1 (address 0x01): rates, operation, conversion mode

| Field | Bits | Reset | Values / behavior |
|---|---|---|---|
| `DR[2:0]` | 7..5 | `000` | Sample rate code, table below |
| `MODE[1:0]` | 4..3 | `00` | `00` Normal; `01` duty-cycle; `10` Turbo; `11` RESERVED |
| `CM` | 2 | `0` | `0` single-shot; `1` continuous |
| `TS` | 1 | `0` | `0` normal analog conversion; `1` internal temperature sensor, overrides REG0 and selects internal reference |
| `BCS` | 0 | `0` | `0` burnout current sources off; `1` 10 uA burnout source for sensor fault checking |

### DR values (Table 8-12, p.44; nominal internal clock / external 4.096 MHz)

| DR code | Normal SPS | Duty-cycle SPS | Turbo SPS |
|---|---:|---:|---:|
| `000` | 20 | 5 | 40 |
| `001` | 45 | 11.25 | 90 |
| `010` | 90 | 22.5 | 180 |
| `011` | 175 | 44 | 350 |
| `100` | 330 | 82.5 | 660 |
| `101` | 600 | 150 | 1200 |
| `110` | 1000 | 250 | 2000 |
| `111` | RESERVED | RESERVED | RESERVED |

The actual output data rate scales with an external clock if not 4.096 MHz. Normal/duty modulator: nominal 256 kHz; Turbo: nominal 512 kHz. Example: `DR=010`, `MODE=00`, `CM=1`, `TS=0`, `BCS=0` => `REG1=0x44` (normal 90 SPS continuous). `REG1=0x04` means normal 20 SPS continuous.

## 6. REG2 (address 0x02): reference, line filter, low-side switch, excitation

| Field | Bits | Reset | Purpose |
|---|---|---|---|
| `VREF[1:0]` | 7..6 | `00` | Voltage-reference selection |
| `50/60[1:0]` | 5..4 | `00` | Line-rejection FIR coefficient selection |
| `PSW` | 3 | `0` | Automatic analog low-side switch |
| `IDAC[2:0]` | 2..0 | `000` | Excitation current for both IDAC1 and IDAC2 |

### VREF[1:0]

| Code | Reference |
|---|---|
| `00` | Internal 2.048 V reference |
| `01` | External reference pins REFP0 / REFN0 |
| `10` | External REFP1 / REFN1 shared with AIN0 / AIN3 |
| `11` | Analog supply voltage, AVDD - AVSS |

REFP1/REFN1 share physical pins with AIN0 and AIN3. Select only a reference compatible with the PCB. For ratiometric bridge measurements, a reference that tracks actual bridge excitation helps cancel excitation variation; the supply-reference option is suitable only if excitation tracks the ADC analog supply as assumed. Observe reference input and common-mode specifications.

### 50/60[1:0]

| Code | Rejection selection |
|---|---|
| `00` | No special 50/60 Hz rejection setting |
| `01` | Simultaneous 50 and 60 Hz rejection |
| `10` | 50 Hz only |
| `11` | 60 Hz only |

**Mandatory limitation:** use these selections only at `20 SPS Normal` or `5 SPS Duty-cycle`. At all other data rates, set `50/60=00`. Example: normal 90 SPS + `50/60=01` is NOT a supported configuration to request simultaneous notch rejection.

### PSW, bit 3

- `0`: low-side switch from AIN3/REFN1 to AVSS always open (default).
- `1`: low-side switch closes automatically on START/SYNC and opens on POWERDOWN.
- Only enable if the actual schematic uses the internal low-side switch to power the sensor. It is not a general-purpose GPIO switch.

### IDAC[2:0] current setting

| Code | Current of IDAC1 and IDAC2 |
|---|---|
| `000` | Off |
| `001` | 10 uA |
| `010` | 50 uA |
| `011` | 100 uA |
| `100` | 250 uA |
| `101` | 500 uA |
| `110` | 1000 uA |
| `111` | 1500 uA |

Example: `VREF=11`, `50/60=00`, `PSW=0`, `IDAC=000` => `REG2=0xC0`. TI bridge example `REG2=0x98` means `VREF=10`, `50/60=01`, `PSW=1`, IDAC off. Do not apply `0x98` unless the bridge reference and low-side switch wiring matches that TI example.

## 7. REG3 (address 0x03): current-source routing, DRDY

| Field | Bits | Reset | Function |
|---|---|---|---|
| `I1MUX[2:0]` | 7..5 | `000` | Route IDAC1 |
| `I2MUX[2:0]` | 4..2 | `000` | Route IDAC2 |
| `DRDYM` | 1 | `0` | `0` only dedicated DRDY pin reports ready; `1` DRDY and DOUT/DRDY report ready |
| RESERVED | 0 | `0` | MUST always write zero |

Both I1MUX and I2MUX use the same routing codes:

| Code | IDAC output destination |
|---|---|
| `000` | Disabled |
| `001` | AIN0/REFP1 |
| `010` | AIN1 |
| `011` | AIN2 |
| `100` | AIN3/REFN1 |
| `101` | REFP0 |
| `110` | REFN0 |
| `111` | RESERVED |

For an external voltage-excited Wheatstone bridge using a dedicated DRDY pin and no IDACs, `REG3=0x00`. DOUT/DRDY cannot serve as ready while CS is high, so the dedicated DRDY pin is preferable for a shared SPI bus.

## 8. 24-bit conversion data format and read protocols

- ADS1220 converts the differential input `VIN=VAINP-VAINN`.
- The result is a signed 24-bit **two's complement** value, **MSB first**, exactly three data bytes.
- `0x000000` => zero; `0x000001` => +1; `0x7FFFFF` => +8,388,607; `0xFFFFFF` => -1; `0x800000` => -8,388,608.
- The output saturates at the corresponding positive and negative full-scale codes.
- Ideal LSB: `VREF / (gain * 2^23)` volts.
- Ideal input voltage: `signed_raw * VREF / (gain * 8388608.0)`.
- With a bridge excited by a tracked reference, consider expressing the measurement as a ratio to excitation rather than relying on a hardcoded voltage.

**Method A, recommended when DRDY pin is connected:** wait for DRDY low/falling edge; assert CS; transfer 3 dummy `0x00` bytes in a single transaction; 3 received bytes are the conversion result. No RDATA command needed. Read promptly enough to avoid losing sample events. The dedicated DRDY returns high at the next SCLK rising edge.

**Method B, command-based:** assert CS; transfer `0x10 0x00 0x00 0x00` in a single transaction; ignore RX[0], decode RX[1..3]. RDATA loads the most recent completed conversion; it is not a trigger to start conversion and is not proof that a new result has arrived.

**C helper (portable signed conversion):**

```c
#include <stdint.h>

static inline int32_t ads1220_decode_s24(const uint8_t bytes[3])
{
    uint32_t u = ((uint32_t)bytes[0] << 16)
               | ((uint32_t)bytes[1] << 8)
               |  (uint32_t)bytes[2];
    return (u & 0x800000u) ? ((int32_t)u - 0x1000000) : (int32_t)u;
}

static inline double ads1220_code_to_volts(int32_t sample,
                                             double vref_volts,
                                             unsigned gain)
{
    return ((double)sample * vref_volts) / ((double)gain * 8388608.0);
}
```

For device settings, use actual reference value and gain from the applied register config; do not assume 2.048 V when REG2.VREF selects an external/supply reference. Conversion and calibration should account for input offset, reference variation and gain error. Enabling the temperature sensor (`REG1.TS=1`) changes the interpretation of the conversion output; do not blindly feed temperature-mode results through the normal voltage conversion routine.

## 9. C constants and encode/decode helpers

```c
#include <stdint.h>
#include <stdbool.h>

#define ADS1220_REG0 0x00u
#define ADS1220_REG1 0x01u
#define ADS1220_REG2 0x02u
#define ADS1220_REG3 0x03u
#define ADS1220_REGISTER_COUNT 4u

#define ADS1220_CMD_RESET     0x06u
#define ADS1220_CMD_START     0x08u
#define ADS1220_CMD_POWERDOWN 0x02u
#define ADS1220_CMD_RDATA     0x10u

#define ADS1220_RREG(addr, count) ((uint8_t)(0x20u | (((addr) & 3u) << 2) | (((count) - 1u) & 3u)))
#define ADS1220_WREG(addr, count) ((uint8_t)(0x40u | (((addr) & 3u) << 2) | (((count) - 1u) & 3u)))
/* Validate addr in [0,3], count in [1,4], addr+count <= 4 before macro use. */

#define ADS1220_REG0_MUX_SHIFT   4u
#define ADS1220_REG0_GAIN_SHIFT  1u
#define ADS1220_REG0_BYPASS     (1u << 0)

#define ADS1220_REG1_DR_SHIFT    5u
#define ADS1220_REG1_MODE_SHIFT  3u
#define ADS1220_REG1_CM         (1u << 2)
#define ADS1220_REG1_TS         (1u << 1)
#define ADS1220_REG1_BCS        (1u << 0)

#define ADS1220_REG2_VREF_SHIFT  6u
#define ADS1220_REG2_FILTER_SHIFT 4u
#define ADS1220_REG2_PSW        (1u << 3)
#define ADS1220_REG2_IDAC_SHIFT 0u

#define ADS1220_REG3_I1MUX_SHIFT 5u
#define ADS1220_REG3_I2MUX_SHIFT 2u
#define ADS1220_REG3_DRDYM     (1u << 1)
#define ADS1220_REG3_RESERVED_MASK (1u << 0)

static inline uint8_t ads1220_make_reg0(unsigned mux, unsigned gain_code, bool bypass)
{
    return (uint8_t)(((mux & 15u) << 4) | ((gain_code & 7u) << 1) | (bypass ? 1u : 0u));
}
static inline uint8_t ads1220_make_reg1(unsigned dr, unsigned mode,
                                         bool continuous, bool temperature, bool burnout)
{
    return (uint8_t)(((dr & 7u) << 5) | ((mode & 3u) << 3)
                     | (continuous ? 4u : 0u) | (temperature ? 2u : 0u)
                     | (burnout ? 1u : 0u));
}
static inline uint8_t ads1220_make_reg2(unsigned vref, unsigned filter,
                                         bool psw, unsigned idac)
{
    return (uint8_t)(((vref & 3u) << 6) | ((filter & 3u) << 4)
                     | (psw ? 8u : 0u) | (idac & 7u));
}
static inline uint8_t ads1220_make_reg3(unsigned i1mux, unsigned i2mux, bool shared_drdy)
{
    return (uint8_t)(((i1mux & 7u) << 5) | ((i2mux & 7u) << 2)
                     | (shared_drdy ? 2u : 0u));
}
```

The above constructors encode the fields but intentionally do **not** validate semantic restrictions. The driver API should reject reserved values and incompatible combinations instead of silently masking invalid caller inputs.

## 10. Verified example configuration sets

| Scenario | REG0 | REG1 | REG2 | REG3 | Explanation / caveat |
|---|---|---|---|---|---|
| TI continuous example, p.51 | `08` | `04` | `10` | `00` | AIN0-AIN1, gain16, Normal 20SPS continuous, internal VREF, simultaneous 50/60 rejection, dedicated DRDY |
| TI Wheatstone bridge example, p.63 | `3E` | `04` | `98` | `00` | AIN1-AIN2, gain128, Normal 20SPS continuous, REFP1/REFN1 external ratiometric reference, simultaneous rejection, PSW on; **only if PCB matches** |
| Conditional project example | `3E` | `44` | `C0` | `00` | AIN1-AIN2, gain128, Normal 90SPS continuous, AVDD-AVSS reference, no special line rejection, low-side switch off, IDAC off; **must verify wiring / analog limits** |
| All reset defaults | `00` | `00` | `00` | `00` | AIN0-AIN1, gain1, Normal 20SPS single-shot, internal VREF, no special rejection, no IDAC, dedicated DRDY |

All hexadecimal register cells above are byte values. For example, the conditional project sequence is `43 3E 44 C0 00`. It must NOT be presented as the hardware-validated configuration without checking the actual schematic.

## 11. Agent implementation contract and correctness checks

### Recommended function-level API

```c
int ads1220_reset(void);
int ads1220_start(void);
int ads1220_powerdown(void);
int ads1220_read_regs(uint8_t first, uint8_t *dest, uint8_t count);
int ads1220_write_regs(uint8_t first, const uint8_t *src, uint8_t count);
int ads1220_read_conversion_direct(int32_t *out);  /* after DRDY */
int ads1220_read_conversion_rdata(int32_t *out);    /* latest result */
int ads1220_set_config(const uint8_t regs[4]);
int ads1220_get_config(uint8_t regs[4]);
```

Implementation guidelines:

1. Read the board's actual devicetree SPI chip select, SPI pins, clock, DRDY GPIO and power-control pins; do not invent the board pin mapping.
2. Configure Zephyr SPI to Mode 1 (8-bit, MSB-first). Check `spi_is_ready_dt()`.
3. Use atomic complete SPI transactions for commands plus their payload; never let CS go high between RREG/WREG opcode and bytes.
4. On init: validate supplies; RESET (`06`), wait `>= 50us + 32*tCLK`; WREG all four; RREG readback and compare; configure DRDY; START (`08`) if conversion requested.
5. If waiting via DRDY GPIO falling edge, notify a sampling thread (e.g. semaphore); do not perform blocking SPI from GPIO interrupt handler.
6. Validate and reject reserved: REG0.MUX `0xF`, REG1.DR `0x7`, REG1.MODE `0x3`, REG3.I1MUX/I2MUX `0x7`, REG3 bit0 must be zero; prohibit PGA bypass with gain >4 or single-ended modes without bypass.
7. Enforce `REG2.50/60=00` except at Normal 20SPS or Duty-cycle 5SPS. Do not assume filtering settings apply at 90SPS/175SPS.
8. Do not enable REG2.PSW or IDAC paths without hardware evidence they are connected as intended. Verify VREF against actual power and bridge wiring.
9. Preserve error propagation (`-EINVAL`, `-EIO`, `-ENODEV`, `-ETIMEDOUT` as appropriate); add bounded DRDY wait/timeout.
10. Test register readback, timing, data freshness, signed data decoding, known input conditions, and sample-rate/gain changes; consider zero-offset calibration when gain changes, as TI suggests on p.51.

### Minimum verification vectors

```text
RREG(0,1)  = 20
RREG(1,1)  = 24
RREG(2,1)  = 28
RREG(3,1)  = 2C
RREG(0,4)  = 23
RREG(1,3)  = 26
WREG(0,1)  = 40
WREG(0,2)  = 41
WREG(0,4)  = 43
make_reg0(mux=3,gain_code=7,bypass=0)      = 3E
make_reg1(dr=2,mode=0,continuous=1,ts=0,bcs=0) = 44
make_reg2(vref=3,filter=0,psw=0,idac=0)   = C0
make_reg3(i1=0,i2=0,shared_drdy=0)        = 00
s24(00 00 00) = 0
s24(7F FF FF) = 8388607
s24(80 00 00) = -8388608
s24(FF FF FF) = -1
```

## 12. Datasheet traceability and unresolved PCB-dependent items

- Datasheet pp.9,36: timing and SPI interface.
- Datasheet pp.37-40: signed output, commands, direct/RDATA reading, CS and DIN interactions.
- Datasheet pp.42-46: four registers and **all** bit-field definitions in Tables 8-8 to 8-14.
- Datasheet p.51: official continuous-conversion startup pseudocode, reset/readback and offset adjustment suggestion.
- Datasheet pp.62-63: TI example bridge configuration (`3E 04 98 00`).

**Must confirm before adopting conditional project config:** physical AIN+/AIN- connections, bridge-excitation voltage, reference source/pins, whether AIN3 low-side switch is routed to the sensor, whether IDACs are needed, analog PGA common-mode range at maximum strain, and dedicated DRDY connectivity. `0x3E 0x44 0xC0 0x00` is an engineering proposal, not a register set uniquely implied by the datasheet.
