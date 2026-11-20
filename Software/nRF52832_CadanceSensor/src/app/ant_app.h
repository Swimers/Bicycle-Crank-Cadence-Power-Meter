#ifndef CADENCE_ANT_APP_H_
#define CADENCE_ANT_APP_H_

#include <stdint.h>

/* Open the ANT+ Bicycle Cadence channel before starting the sensor. */
int ant_app_init(void);

/* Record one confirmed crank revolution, with its uptime in milliseconds. */
void ant_app_record_revolution(uint64_t time_ms);

#endif
