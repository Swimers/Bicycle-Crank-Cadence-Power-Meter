#ifndef BCCPM_POWER_H_
#define BCCPM_POWER_H_

#include <stdbool.h>
#include <stdint.h>

struct power_battery_sample {
	uint32_t voltage_mv;
	uint8_t percent;
};

/* Initialize GPIOs low and the battery ADC; serialize calls in thread context. */
int power_init(void);
/* Single 4.2 V Li-ion cell estimate; output unchanged on failure. */
int power_battery_read(struct power_battery_sample *sample);
/* Set the physical GPIO levels. ISET controls VBUS current limit, not ICHG. */
int power_set_mode(bool high);
int power_set_iset(bool high);
/* Direct P0.22 level control only: Ship behavior requires board validation. */
int power_set_power_off(bool high);

#endif
