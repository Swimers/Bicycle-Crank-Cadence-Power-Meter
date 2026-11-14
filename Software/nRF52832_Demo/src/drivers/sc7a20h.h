#ifndef CADENCE_SC7A20H_H_
#define CADENCE_SC7A20H_H_

#include <stdbool.h>
#include <stdint.h>
#include <zephyr/device.h>

#define SC7A20H_ADDR_HIGH 0x19U
#define SC7A20H_ADDR_LOW  0x18U
#define SC7A20H_WHO_AM_I  0x11U
#define SC7A20H_CTRL_COUNT 7U

/* One owner thread must serialize initialization and sample reads. */
struct sc7a20h {
	const struct device *bus;
	uint16_t address;
	uint8_t id;
	uint8_t version;
	uint8_t ctrl[SC7A20H_CTRL_COUNT]; /* Registers 0x1f through 0x25. */
	uint8_t last_reg; /* Last I2C transaction, useful on failure. */
	bool initialized;
};

struct sc7a20h_sample {
	int16_t raw[3]; /* Signed 12-bit counts, XYZ. */
	int32_t mg[3];  /* Ideal +/-8g scaling: raw * 8000 / 2048. */
	uint8_t status;
};

/* Thread context only. Configures 100 Hz, +/-8g, BDU, no FIFO/interrupts.
 * VERSION is reported but only WHO_AM_I is used to reject a device.
 */
int sc7a20h_init(struct sc7a20h *sensor, const struct device *bus, uint16_t address);

/* Returns -EAGAIN if XYZ is not ready; leaves sample unchanged on error. */
int sc7a20h_read(struct sc7a20h *sensor, struct sc7a20h_sample *sample);

#endif
