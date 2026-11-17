#include "imu.h"
#include "drivers/lsm6dsr.h"

#include <errno.h>
#include <zephyr/kernel.h>

int imu_init(void)
{
	return lsm6dsr_init();
}

int imu_sample_read(struct imu_sample *sample, uint32_t timeout_ms)
{
	struct lsm6dsr_raw raw;
	struct imu_sample result;
	int64_t deadline = k_uptime_get() + timeout_ms;
	int ret;

	if (!sample) {
		return -EINVAL;
	}
	for (;;) {
		ret = lsm6dsr_read_raw(&raw);
		if (ret != -EAGAIN) {
			break;
		}
		if (timeout_ms == 0) {
			return -EAGAIN;
		}
		if (k_uptime_get() >= deadline) {
			return -ETIMEDOUT;
		}
		k_sleep(K_MSEC(1));
	}
	if (ret < 0) {
		return ret;
	}
	result.timestamp_ms = k_uptime_get();
	for (int i = 0; i < 3; ++i) {
		result.accel_g[i] = raw.accel[i] * 0.000122f;
		result.gyro_dps[i] = raw.gyro[i] * 0.035f;
	}
	*sample = result;
	return 0;
}
