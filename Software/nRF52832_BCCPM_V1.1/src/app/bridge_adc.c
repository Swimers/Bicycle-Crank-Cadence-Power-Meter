#include "bridge_adc.h"
#include "drivers/ads1220.h"

#include <errno.h>
#include <stdbool.h>
#include <zephyr/kernel.h>

/* Conservative provisional value; the actual bridge settling must be measured. */
#define BRIDGE_SETTLE_MS 100
#define FIRST_SAMPLE_TIMEOUT_MS 500
#define ADC_GAIN 16.0f

static bool initialized;
static bool conversion_pending;
static bool bridge_on;

static int read_conversion(int32_t *raw, uint32_t timeout_ms)
{
	int64_t deadline = k_uptime_get() + timeout_ms;
	int ret;

	if (!conversion_pending) {
		ret = ads1220_start();
		if (ret < 0) {
			return ret;
		}
		conversion_pending = true;
	}
	for (;;) {
		ret = ads1220_read(raw);
		if (ret != -EAGAIN) {
			conversion_pending = false;
			return ret;
		}
		if (timeout_ms == 0) {
			return -EAGAIN;
		}
		if (k_uptime_get() >= deadline) {
			return -ETIMEDOUT;
		}
		k_sleep(K_MSEC(1));
	}
}

int bridge_adc_init(void)
{
	int32_t discarded;
	int ret;

	initialized = false;
	conversion_pending = false;
	ret = ads1220_init();
	if (ret < 0) {
		return ret;
	}
	ret = ads1220_bridge_set(true);
	if (ret < 0) {
		return ret;
	}
	k_sleep(K_MSEC(BRIDGE_SETTLE_MS));
	ret = read_conversion(&discarded, FIRST_SAMPLE_TIMEOUT_MS);
	if (ret < 0) {
		return ret;
	}
	bridge_on = true;
	initialized = true;
	return 0;
}

int bridge_adc_set_enabled(bool enabled)
{
	int ret;
	int32_t discarded;
	if (!initialized) { return -EACCES; }
	if (conversion_pending) { return -EBUSY; }
	if (bridge_on == enabled) { return 0; }
	ret = ads1220_bridge_set(enabled);
	if (ret < 0) { return ret; }
	bridge_on = enabled;
	if (!enabled) { return 0; }
	k_sleep(K_MSEC(BRIDGE_SETTLE_MS));
	ret = read_conversion(&discarded, FIRST_SAMPLE_TIMEOUT_MS);
	if (ret < 0) {
		/* Require reinitialization if the first conversion did not complete. */
		initialized = false;
	}
	return ret;
}

int bridge_adc_sample_read(struct bridge_adc_sample *sample, uint32_t timeout_ms)
{
	struct bridge_adc_sample result;
	int ret;

	if (!sample) {
		return -EINVAL;
	}
	if (!initialized || !bridge_on) {
		return -EACCES;
	}
	ret = read_conversion(&result.raw, timeout_ms);
	if (ret < 0) {
		return ret;
	}
	result.timestamp_ms = k_uptime_get();
	result.input_over_reference = (float)result.raw / (8388608.0f * ADC_GAIN);
	*sample = result;
	return 0;
}
