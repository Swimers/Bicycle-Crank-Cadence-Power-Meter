#ifndef BCCPM_BRIDGE_ADC_H_
#define BCCPM_BRIDGE_ADC_H_

#include <stdint.h>
#include <stdbool.h>

struct bridge_adc_sample {
	int32_t raw;
	/* Ideal differential input / excitation reference; uncalibrated. */
	float input_over_reference;
	/* Host uptime after reading the conversion, not a sensor timestamp. */
	int64_t timestamp_ms;
};

/* Enables bridge excitation continuously. Requires thread context;
 * serialize all calls. Returns 0 or negative errno. */
int bridge_adc_init(void);
/* Switching on waits for settling and discards one conversion. */
int bridge_adc_set_enabled(bool enabled);
/* Starts one conversion as needed. Zero timeout polls (-EAGAIN while pending).
 * Timeout leaves the conversion pending for a subsequent call to collect.
 * Output remains unchanged on failure. */
int bridge_adc_sample_read(struct bridge_adc_sample *sample, uint32_t timeout_ms);

#endif
