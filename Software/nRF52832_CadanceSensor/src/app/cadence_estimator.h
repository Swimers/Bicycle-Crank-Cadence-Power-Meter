#ifndef CADENCE_ESTIMATOR_H_
#define CADENCE_ESTIMATOR_H_

#include <stdbool.h>
#include <stdint.h>

#define CADENCE_AXIS_COUNT 3

struct cadence_axis {
	int32_t filtered;
	int32_t baseline;
	uint64_t last_crossing_ms;
	uint32_t period_ms;
	bool armed;
};

struct cadence_estimator {
	struct cadence_axis axes[CADENCE_AXIS_COUNT];
	uint64_t last_sample_ms;
	uint64_t last_event_ms;
	uint8_t active_axis;
	bool initialized;
};

/* Initialize the estimator to zero. True means a confirmed revolution at time_ms. */
bool cadence_estimator_update(struct cadence_estimator *estimator,
			      const int32_t acceleration_mg[CADENCE_AXIS_COUNT],
			      uint64_t time_ms);

#endif
