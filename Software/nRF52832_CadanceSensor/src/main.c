#include <errno.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/kernel.h>

#include "app/battery.h"
#include "app/ble_csc.h"
#include "app/sc7a20h_app.h"

#define LOW_BATTERY_PERCENT 20
#define SHUTDOWN_BATTERY_PERCENT 10

static const struct pwm_dt_spec buzzer = PWM_DT_SPEC_GET(DT_PATH(zephyr_user));
K_TIMER_DEFINE(battery_timer, NULL, NULL);

static void beep(unsigned int count, int duration_ms)
{
	if (!pwm_is_ready_dt(&buzzer)) {
		return;
	}

	for (unsigned int i = 0; i < count; i++) {
		if (i != 0) {
			k_msleep(100);
		}
		if (pwm_set_pulse_dt(&buzzer, buzzer.period / 2U) != 0) {
			return;
		}
		k_msleep(duration_ms);
		if (pwm_set_pulse_dt(&buzzer, 0) != 0) {
			return;
		}
	}
}

static int sample_battery(bool *ready)
{
	if (!*ready) {
		*ready = battery_init() == 0;
	}
	return *ready ? battery_sample() : -ENODEV;
}

static void indicate_battery(int percent, bool power_on)
{
	if (percent > 0 && percent < SHUTDOWN_BATTERY_PERCENT) {
		beep(1, 400);
		(void)battery_request_power_off();
	} else if (percent > 0 && percent < LOW_BATTERY_PERCENT) {
		beep(3, 100);
	} else if (power_on) {
		beep(2, 100);
	}
}

int main(void)
{
	bool battery_ready = false;

	k_timer_start(&battery_timer, K_MINUTES(1), K_MINUTES(1));
	int percent = sample_battery(&battery_ready);
	int err = ble_csc_start();

	if (err == 0) {
		sc7a20h_app_start();
	}
	indicate_battery(percent, true);

	for (;;) {
		k_timer_status_sync(&battery_timer);
		percent = sample_battery(&battery_ready);
		indicate_battery(percent, false);
	}
}
