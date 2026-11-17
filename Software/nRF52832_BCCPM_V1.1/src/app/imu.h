#ifndef BCCPM_IMU_H_
#define BCCPM_IMU_H_

#include <stdint.h>

/* Sensor axes, uncalibrated. Time is the host uptime after the I2C read. */
struct imu_sample {
	float accel_g[3];
	float gyro_dps[3];
	int64_t timestamp_ms;
};

/* Initialize once before sampling. Both functions require thread context;
 * the caller must serialize calls. Returns 0 or a negative errno.
 */
int imu_init(void);
/* Wait up to timeout_ms for both sensors; 0 performs one nonblocking check.
 * -EAGAIN means no data on a nonblocking check, -ETIMEDOUT means timeout.
 */
int imu_sample_read(struct imu_sample *sample, uint32_t timeout_ms);

#endif
