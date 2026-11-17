#include "lsm6dsr.h"

#include <errno.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>

#define REG_WHO_AM_I 0x0f
#define REG_CTRL1_XL 0x10
#define REG_CTRL2_G  0x11
#define REG_CTRL3_C  0x12
#define REG_CTRL4_C  0x13
#define REG_STATUS   0x1e
#define REG_OUT_G    0x22
#define WHO_AM_I     0x6b
#define CTRL3_RESET  0x01
#define CTRL3_CONFIG 0x44
#define XL_CONFIG    0x48
#define G_CONFIG     0x48
#define RESET_TIMEOUT_MS 100

static const struct i2c_dt_spec bus = I2C_DT_SPEC_GET(DT_NODELABEL(imu));
static bool initialized;

static int write_and_verify(uint8_t reg, uint8_t value)
{
	uint8_t actual;
	int ret = i2c_reg_write_byte_dt(&bus, reg, value);
	if (ret < 0) {
		return ret;
	}
	ret = i2c_reg_read_byte_dt(&bus, reg, &actual);
	return ret < 0 ? ret : (actual == value ? 0 : -EIO);
}

int lsm6dsr_init(void)
{
	uint8_t value;
	int ret;
	int64_t deadline;

	initialized = false;
	if (!i2c_is_ready_dt(&bus)) {
		return -ENODEV;
	}
	ret = i2c_reg_read_byte_dt(&bus, REG_WHO_AM_I, &value);
	if (ret < 0) {
		return ret;
	}
	if (value != WHO_AM_I) {
		return -ENODEV;
	}
	ret = i2c_reg_write_byte_dt(&bus, REG_CTRL3_C, CTRL3_RESET);
	if (ret < 0) {
		return ret;
	}
	deadline = k_uptime_get() + RESET_TIMEOUT_MS;
	do {
		k_sleep(K_MSEC(1));
		ret = i2c_reg_read_byte_dt(&bus, REG_CTRL3_C, &value);
		if (ret < 0) {
			return ret;
		}
		if (!(value & CTRL3_RESET)) {
			break;
		}
	} while (k_uptime_get() < deadline);
	if (value & CTRL3_RESET) {
		return -ETIMEDOUT;
	}
	ret = write_and_verify(REG_CTRL3_C, CTRL3_CONFIG);
	if (ret < 0) {
		return ret;
	}
	ret = i2c_reg_read_byte_dt(&bus, REG_CTRL4_C, &value);
	if (ret < 0) {
		return ret;
	}
	if (value & 0x04) {
		return -EIO;
	}
	ret = write_and_verify(REG_CTRL1_XL, XL_CONFIG);
	if (ret < 0) {
		return ret;
	}
	ret = write_and_verify(REG_CTRL2_G, G_CONFIG);
	if (ret < 0) {
		return ret;
	}
	initialized = true;
	return 0;
}

static int16_t unpack(const uint8_t *p)
{
	uint16_t u = (uint16_t)p[0] | ((uint16_t)p[1] << 8);
	return (int16_t)((u & 0x8000u) ? (int32_t)u - 65536 : (int32_t)u);
}

int lsm6dsr_read_raw(struct lsm6dsr_raw *sample)
{
	uint8_t status, bytes[12];
	struct lsm6dsr_raw result;
	int ret;

	if (!sample) {
		return -EINVAL;
	}
	if (!initialized) {
		return -EACCES;
	}
	ret = i2c_reg_read_byte_dt(&bus, REG_STATUS, &status);
	if (ret < 0) {
		return ret;
	}
	if ((status & 0x03) != 0x03) {
		return -EAGAIN;
	}
	ret = i2c_burst_read_dt(&bus, REG_OUT_G, bytes, sizeof(bytes));
	if (ret < 0) {
		return ret;
	}
	for (int i = 0; i < 3; ++i) {
		result.gyro[i] = unpack(&bytes[2 * i]);
		result.accel[i] = unpack(&bytes[6 + 2 * i]);
	}
	*sample = result;
	return 0;
}
