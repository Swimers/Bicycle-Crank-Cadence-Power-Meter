#include "sc7a20h_app.h"
#include "drivers/sc7a20h.h"

#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#define SENSOR_NODE DT_ALIAS(accel0)
#define SENSOR_ADDRESS DT_REG_ADDR(SENSOR_NODE)
#define STATUS_OVERRUN 0x80U

struct sensor_snapshot {
	struct sc7a20h sensor;
	struct sc7a20h_sample sample;
	uint32_t samples;
	uint32_t overruns;
	uint32_t errors;
	int error;
	int64_t timestamp;
	uint32_t generation;
};

static struct sensor_snapshot snapshot;
K_MUTEX_DEFINE(snapshot_lock);

static void sample_thread(void *p1, void *p2, void *p3)
{
	struct sensor_snapshot state = {0};
	const struct device *bus = DEVICE_DT_GET(DT_BUS(SENSOR_NODE));
	int64_t last_data;

	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);
	for (;;) {
		state.error = sc7a20h_init(&state.sensor, bus, SENSOR_ADDRESS);
		if (state.error != 0) {
			state.errors++;
		}
		state.generation++;
		last_data = k_uptime_get();
		for (;;) {
			k_mutex_lock(&snapshot_lock, K_FOREVER);
			snapshot = state;
			k_mutex_unlock(&snapshot_lock);
			if (state.error != 0) {
				break;
			}
			/* Poll faster than the 100 Hz ODR; do not print in this thread. */
			k_msleep(5);
			int err = sc7a20h_read(&state.sensor, &state.sample);

			if (err == -EAGAIN) {
				if (k_uptime_get() - last_data < 1000) {
					continue;
				}
				err = -ETIMEDOUT;
			}
			if (err != 0) {
				state.error = err;
				state.errors++;
				continue;
			}
			state.timestamp = k_uptime_get();
			last_data = state.timestamp;
			state.samples++;
			if ((state.sample.status & STATUS_OVERRUN) != 0U) {
				state.overruns++;
			}
		}
		/* Keep BLE usable if the sensor is absent or its bus fails. */
		k_msleep(2000);
	}
}

K_THREAD_DEFINE(sc7a20h_thread, 1536, sample_thread, NULL, NULL, NULL, 7, 0, SYS_FOREVER_MS);

void sc7a20h_app_start(void)
{
	k_thread_start(sc7a20h_thread);
}

void sc7a20h_app_process(void)
{
	static int64_t next_print;
	static uint32_t last_generation;
	static uint32_t last_sample;
	struct sensor_snapshot state;
	int64_t now = k_uptime_get();

	if (now < next_print) {
		return;
	}
	next_print = now + 100;
	k_mutex_lock(&snapshot_lock, K_FOREVER);
	state = snapshot;
	k_mutex_unlock(&snapshot_lock);
	if (state.generation == 0) {
		return;
	}
	if (state.generation != last_generation) {
		last_generation = state.generation;
		printk("SC7 init: addr=0x%02x id=0x%02x version=0x%02x err=%d\n",
		       state.sensor.address, state.sensor.id, state.sensor.version, state.error);
		printk("SC7 CTRL[1f..25]=%02x %02x %02x %02x %02x %02x %02x\n",
		       state.sensor.ctrl[0], state.sensor.ctrl[1], state.sensor.ctrl[2],
		       state.sensor.ctrl[3], state.sensor.ctrl[4], state.sensor.ctrl[5],
		       state.sensor.ctrl[6]);
	}
	if (state.error != 0) {
		printk("SC7 error=%d reg=0x%02x errors=%u; retry in 2s\n",
		       state.error, state.sensor.last_reg, state.errors);
		/* Suppress repeated lines while waiting for the next init attempt. */
		next_print = now + 2000;
		return;
	}
	if (state.samples == 0 || state.samples == last_sample) {
		return;
	}
	last_sample = state.samples;
	printk("SC7 t=%lld n=%u raw=%d,%d,%d mg=%d,%d,%d st=%02x over=%u\n",
	       (long long)state.timestamp, state.samples,
	       state.sample.raw[0], state.sample.raw[1], state.sample.raw[2],
	       state.sample.mg[0], state.sample.mg[1], state.sample.mg[2],
	       state.sample.status, state.overruns);
}
