#include "ant_decode.h"

#include <string.h>

/* Provisional application policy, not an ANT+ profile requirement. */
#define CADENCE_STOP_MS 3000U
#define CADENCE_REBASE_MS 60000U
#define MAX_CADENCE_RPM_X10 3000U

static uint16_t le16(const uint8_t *p)
{
	return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static void cadence_update(struct ant_decode_state *state, const uint8_t *p,
			   uint32_t now, struct ant_measurement *out)
{
	uint16_t time = le16(p);
	uint16_t count = le16(p + 2);
	uint32_t elapsed = now - state->last_crank_ms;

	if (!state->cadence_baseline_valid || elapsed >= CADENCE_REBASE_MS) {
		state->cadence_baseline_valid = true;
		state->crank_time = time;
		state->crank_count = count;
		state->last_crank_ms = now;
		return;
	}

	uint16_t revolutions = (uint16_t)(count - state->crank_count);
	uint16_t ticks = (uint16_t)(time - state->crank_time);

	if (revolutions == 0) {
		if (elapsed >= CADENCE_STOP_MS) {
			out->has_cadence = true;
			out->cadence_rpm_x10 = 0;
		}
		return;
	}

	state->crank_time = time;
	state->crank_count = count;
	state->last_crank_ms = now;
	if (ticks != 0) {
		uint32_t rpm_x10 = (uint32_t)((uint64_t)revolutions * 614400U / ticks);
		if (rpm_x10 <= MAX_CADENCE_RPM_X10) {
			out->has_cadence = true;
			out->cadence_rpm_x10 = (uint16_t)rpm_x10;
		}
	}
}

bool ant_decode_page(struct ant_decode_state *state, uint8_t device_type,
		     const uint8_t payload[8], uint32_t now_ms,
		     struct ant_measurement *out)
{
	if (state == NULL || payload == NULL || out == NULL) {
		return false;
	}
	memset(out, 0, sizeof(*out));
	if (device_type == 120U && (payload[0] & 0x7fU) <= 7U) {
		out->heart_rate_bpm = payload[7];
		out->has_heart_rate = payload[7] != 0xffU;
		return true;
	}
	if (device_type == 11U && payload[0] == 0x10U) {
		out->power_w = le16(payload + 6);
		out->has_power = out->power_w != 0xffffU;
		if (payload[3] != 0xffU) {
			out->has_cadence = true;
			out->cadence_rpm_x10 = (uint16_t)payload[3] * 10U;
		}
		return true;
	}
	if (device_type == 121U) {
		cadence_update(state, payload, now_ms, out);
		return true;
	}
	if (device_type == 122U && (payload[0] & 0x7fU) <= 5U) {
		cadence_update(state, payload + 4, now_ms, out);
		return true;
	}
	return false;
}
