#include <zephyr/drivers/pwm.h>
#include <zephyr/kernel.h>

#include "app/ble_csc.h"
#include "app/sc7a20h_app.h"

static const struct pwm_dt_spec buzzer = PWM_DT_SPEC_GET(DT_PATH(zephyr_user));

static void indicate_power_on(void)
{
	if (!pwm_is_ready_dt(&buzzer)) {
		return;
	}

	for (int beep = 0; beep < 2; beep++) {
		if (beep != 0) {
			k_msleep(100);
		}
		if (pwm_set_pulse_dt(&buzzer, buzzer.period / 2U) != 0) {
			return;
		}
		k_msleep(100);
		if (pwm_set_pulse_dt(&buzzer, 0) != 0) {
			return;
		}
	}
}

int main(void)
{
	int err = ble_csc_start();

	if (err == 0) {
		sc7a20h_app_start();
	}
	indicate_power_on();
	return err;
}
