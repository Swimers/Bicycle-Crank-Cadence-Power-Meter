#ifndef CADENCE_BATTERY_H_
#define CADENCE_BATTERY_H_

int battery_init(void);

/* Acquire one batch and update BLE. Returns 1-100 on success, or a negative errno. */
int battery_sample(void);

int battery_request_power_off(void);

#endif
