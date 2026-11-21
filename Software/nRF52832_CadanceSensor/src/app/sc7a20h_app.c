#include "sc7a20h_app.h"
#include "ble_csc.h"
#include "cadence_estimator.h"
#include "drivers/sc7a20h.h"

#include <errno.h>
#include <zephyr/kernel.h>

#define SENSOR_NODE DT_ALIAS(accel0)

static void sample_thread(void *p1, void *p2, void *p3)
{
	const struct device *bus = DEVICE_DT_GET(DT_BUS(SENSOR_NODE));
	struct sc7a20h sensor;

	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	for (;;) {
		if (sc7a20h_init(&sensor, bus, DT_REG_ADDR(SENSOR_NODE)) == 0) {
			struct cadence_estimator estimator = {0};
			int64_t last_sample = k_uptime_get();

			for (;;) {
				struct sc7a20h_sample sample;

				/* The 100 Hz sensor supplies new samples when XYZ is ready. */
				k_msleep(5);
				int err = sc7a20h_read(&sensor, &sample);
				int64_t now = k_uptime_get();

				if (err == -EAGAIN && now - last_sample < 1000) {
					continue;
				}
				if (err != 0) {
					break;
				}

				last_sample = now;
				if (cadence_estimator_update(&estimator, sample.mg, (uint64_t)now)) {
					ble_csc_record_revolution((uint64_t)now);
				}
			}
		}

		/* Keep the last confirmed event while the sensor recovers. */
		k_msleep(2000);
	}
}

K_THREAD_DEFINE(sc7a20h_thread, 1536, sample_thread, NULL, NULL, NULL, 7, 0, SYS_FOREVER_MS);

void sc7a20h_app_start(void)
{
	k_thread_start(sc7a20h_thread);
}
