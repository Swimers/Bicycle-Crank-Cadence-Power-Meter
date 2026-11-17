#ifndef BCCPM_BATTERY_ADC_H_
#define BCCPM_BATTERY_ADC_H_

#include <stdint.h>

/* Thread context only; caller serializes all calls. */
int battery_adc_init(void);
int battery_adc_read_mv(uint32_t *battery_mv);

#endif
