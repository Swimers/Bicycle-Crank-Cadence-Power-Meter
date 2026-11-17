#ifndef BCCPM_LSM6DSR_H_
#define BCCPM_LSM6DSR_H_

#include <stdint.h>

struct lsm6dsr_raw {
	int16_t gyro[3];
	int16_t accel[3];
};

/* Fixed profile: 104 Hz, +/-4 g, +/-1000 dps. Thread context only. */
int lsm6dsr_init(void);
/* Returns -EAGAIN when either sensor has no unread sample. */
int lsm6dsr_read_raw(struct lsm6dsr_raw *sample);

#endif
