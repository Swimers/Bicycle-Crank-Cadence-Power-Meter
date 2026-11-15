#ifndef CADENCE_ANT_DECODE_H_
#define CADENCE_ANT_DECODE_H_

#include <stdbool.h>
#include <stdint.h>

/* Caller owns one state per (device number, type, transmission type). */
struct ant_decode_state {
	bool cadence_baseline_valid;
	uint16_t crank_time;
	uint16_t crank_count;
	uint32_t last_crank_ms;
};

struct ant_measurement {
	bool has_heart_rate;
	bool has_cadence;
	bool has_power;
	uint8_t heart_rate_bpm;
	uint16_t cadence_rpm_x10;
	uint16_t power_w;
};

/* Decode an eight-byte ANT+ broadcast page. Returns false for unsupported pages. */
bool ant_decode_page(struct ant_decode_state *state, uint8_t device_type,
		     const uint8_t payload[8], uint32_t now_ms,
		     struct ant_measurement *out);

#endif
