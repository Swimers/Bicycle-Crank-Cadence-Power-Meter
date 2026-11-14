#include "sc7a20h.h"

#include <errno.h>
#include <stddef.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

#define REG_WHO_AM_I 0x0fU
#define REG_CTRL0    0x1fU
#define REG_CTRL1    0x20U
#define REG_STATUS   0x27U
#define REG_OUT_X_L  0x28U
#define REG_VERSION  0x70U
#define AUTO_INCREMENT 0x80U
#define STATUS_XYZ_READY 0x08U

static int read_regs(struct sc7a20h *sensor, uint8_t reg, uint8_t *data, size_t len)
{
	uint8_t sub = reg | (len > 1 ? AUTO_INCREMENT : 0U);

	sensor->last_reg = reg;
	return i2c_write_read(sensor->bus, sensor->address, &sub, 1, data, len);
}

static int write_reg(struct sc7a20h *sensor, uint8_t reg, uint8_t value)
{
	const uint8_t frame[] = {reg, value};

	sensor->last_reg = reg;
	return i2c_write(sensor->bus, frame, sizeof(frame), sensor->address);
}

static int16_t decode12(uint8_t low, uint8_t high)
{
	uint16_t word = ((uint16_t)high << 8) | low;
	int16_t value = (int16_t)(word >> 4);

	if ((value & 0x0800) != 0) {
		value -= 4096;
	}
	return value;
}

int sc7a20h_init(struct sc7a20h *sensor, const struct device *bus, uint16_t address)
{
	static const uint8_t config[][2] = {
		{REG_CTRL1, 0x07}, /* Stop sampling before configuration. */
		{REG_CTRL0, 0x00}, /* HR=0, OSR=0, default DLPF. */
		{0x21, 0x00}, /* Preserve gravity: no high-pass output. */
		{0x22, 0x00}, /* No INT1 routing. */
		{0x23, 0xa0}, /* BDU, little endian, +/-8g. */
		{0x24, 0x00}, /* No FIFO or latched interrupts. */
		{0x25, 0x00}, /* No INT2 routing. */
		{REG_CTRL1, 0x57}, /* 100 Hz, normal mode, XYZ enabled. */
	};
	static const uint8_t expected[] = {0x00, 0x57, 0x00, 0x00, 0xa0, 0x00, 0x00};
	int err;

	if (sensor == NULL || bus == NULL ||
	    (address != SC7A20H_ADDR_HIGH && address != SC7A20H_ADDR_LOW)) {
		return -EINVAL;
	}
	*sensor = (struct sc7a20h){.bus = bus, .address = address};
	if (!device_is_ready(bus)) {
		return -ENODEV;
	}

	err = read_regs(sensor, REG_WHO_AM_I, &sensor->id, 1);
	if (err) {
		return err;
	}
	if (sensor->id != SC7A20H_WHO_AM_I) {
		return -ENODEV;
	}
	err = read_regs(sensor, REG_VERSION, &sensor->version, 1);
	if (err) {
		return err;
	}

	for (size_t i = 0; i < ARRAY_SIZE(config); i++) {
		err = write_reg(sensor, config[i][0], config[i][1]);
		if (err) {
			return err;
		}
	}
	/* Engineering wait: two sample periods; also exceeds the 1 ms
	 * register readback delay recommended in the supplied document.
	 */
	k_msleep(20);
	err = read_regs(sensor, REG_CTRL0, sensor->ctrl, sizeof(sensor->ctrl));
	if (err) {
		return err;
	}
	for (size_t i = 0; i < ARRAY_SIZE(expected); i++) {
		if (sensor->ctrl[i] != expected[i]) {
			sensor->last_reg = REG_CTRL0 + i;
			return -EIO;
		}
	}
	sensor->initialized = true;
	return 0;
}

int sc7a20h_read(struct sc7a20h *sensor, struct sc7a20h_sample *sample)
{
	struct sc7a20h_sample next;
	uint8_t data[6];
	int err;

	if (sensor == NULL || sample == NULL) {
		return -EINVAL;
	}
	if (!sensor->initialized) {
		return -ENODEV;
	}
	err = read_regs(sensor, REG_STATUS, &next.status, 1);
	if (err) {
		return err;
	}
	if ((next.status & STATUS_XYZ_READY) == 0U) {
		return -EAGAIN;
	}
	/* Sends sub-address 0xa8, then repeated START and all six bytes. */
	err = read_regs(sensor, REG_OUT_X_L, data, sizeof(data));
	if (err) {
		return err;
	}
	for (size_t axis = 0; axis < ARRAY_SIZE(next.raw); axis++) {
		next.raw[axis] = decode12(data[2 * axis], data[2 * axis + 1]);
		next.mg[axis] = (int32_t)next.raw[axis] * 8000 / 2048;
	}
	*sample = next;
	return 0;
}
