#include "cadence_estimator.h"

#define FILTER_SCALE 256
#define HYSTERESIS (180 * FILTER_SCALE)
/* Edge tolerances for a nominal 20 to 240 rpm range and 100 Hz sampling. */
#define MIN_PERIOD_MS 230U
#define MAX_PERIOD_MS 3200U
#define SAMPLE_GAP_MS 150U
#define PERIOD_SCALE 256
#define MAX_EVENT_DELAY_MS 80
#define NO_AXIS CADENCE_AXIS_COUNT

static bool axis_revolution(struct cadence_axis *axis, int32_t sample, uint64_t time_ms)
{
	/* At 100 Hz, smooth vibration and remove the slowly varying DC offset. */
	axis->filtered += (sample - axis->filtered) / 4;
	axis->baseline += (axis->filtered - axis->baseline) / 64;
	int32_t signal = axis->filtered - axis->baseline;

	if (signal <= -HYSTERESIS) {
		axis->armed = true;
	}
	if (!axis->armed || signal < HYSTERESIS) {
		return false;
	}
	axis->armed = false;

	uint64_t previous_crossing = axis->last_crossing_ms;
	uint64_t interval = time_ms - previous_crossing;
	uint32_t previous = axis->period_ms;

	/* Record short crossings too, so fast vibration cannot alias to half speed. */
	axis->last_crossing_ms = time_ms;
	if (previous_crossing == 0 || interval < MIN_PERIOD_MS || interval > MAX_PERIOD_MS) {
		axis->period_ms = 0;
		return false;
	}
	axis->period_ms = (uint32_t)interval;
	/* Two consecutive plausible periods reject isolated movements. */
	uint32_t difference = axis->period_ms > previous ?
			      axis->period_ms - previous : previous - axis->period_ms;

	return difference <= previous / 3U;
}

static uint64_t estimate_event_time(struct cadence_estimator *estimator,
				    uint64_t time_ms, uint32_t period_ms, uint8_t axis)
{
	int32_t period = (int32_t)period_ms * PERIOD_SCALE;

	/* Re-anchor at the measured crossing after a gap or a change of axis. */
	if (estimator->active_axis != axis || time_ms - estimator->last_event_ms != period_ms) {
		estimator->filtered_period = period;
		estimator->event_delay_ms = 0;
		return time_ms;
	}

	estimator->filtered_period += (period - estimator->filtered_period) / 4;
	int32_t predicted_period = (estimator->filtered_period + PERIOD_SCALE / 2) / PERIOD_SCALE;
	int32_t delay = (int32_t)period_ms + estimator->event_delay_ms - predicted_period;

	/* Correct a quarter of the phase error each turn. Keep the estimate close
	 * to the measured crossing and never report a future event time.
	 */
	delay -= delay / 4;
	int32_t max_delay = (int32_t)period_ms / 8;
	if (max_delay > MAX_EVENT_DELAY_MS) {
		max_delay = MAX_EVENT_DELAY_MS;
	}
	if (delay > max_delay) {
		delay = max_delay;
	}
	estimator->event_delay_ms = delay > 0 ? delay : 0;
	return time_ms - (uint64_t)estimator->event_delay_ms;
}

bool cadence_estimator_update(struct cadence_estimator *estimator,
			      const int32_t acceleration_mg[CADENCE_AXIS_COUNT],
			      uint64_t time_ms, uint64_t *event_time_ms)
{
	if (!estimator->initialized || time_ms - estimator->last_sample_ms > SAMPLE_GAP_MS) {
		*estimator = (struct cadence_estimator){
			.last_sample_ms = time_ms,
			.active_axis = NO_AXIS,
			.initialized = true,
		};
		for (unsigned int axis = 0; axis < CADENCE_AXIS_COUNT; axis++) {
			int32_t sample = acceleration_mg[axis] * FILTER_SCALE;

			estimator->axes[axis].filtered = sample;
			estimator->axes[axis].baseline = sample;
		}
		return false;
	}

	estimator->last_sample_ms = time_ms;
	if (time_ms - estimator->last_event_ms > MAX_PERIOD_MS) {
		estimator->active_axis = NO_AXIS;
	}

	bool revolution = false;

	for (uint8_t axis = 0; axis < CADENCE_AXIS_COUNT; axis++) {
		if (axis_revolution(&estimator->axes[axis], acceleration_mg[axis] * FILTER_SCALE,
				    time_ms) &&
		    (estimator->active_axis == NO_AXIS || estimator->active_axis == axis)) {
			/* Keep the chosen phase while turning, even if other axes also cross. */
			*event_time_ms = estimate_event_time(estimator, time_ms,
							    estimator->axes[axis].period_ms, axis);
			estimator->active_axis = axis;
			estimator->last_event_ms = time_ms;
			revolution = true;
		}
	}
	return revolution;
}
