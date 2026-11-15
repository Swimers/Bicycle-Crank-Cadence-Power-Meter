#ifndef CADENCE_ANT_RECEIVER_H_
#define CADENCE_ANT_RECEIVER_H_

#include <zephyr/device.h>

/* Call from main thread. Callback only enqueues frames; it never writes USB. */
int ant_receiver_start(void);
void ant_receiver_process(const struct device *uart);

#endif
