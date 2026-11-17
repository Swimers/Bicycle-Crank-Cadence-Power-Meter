#include "board_io.h"
#include "power.h"
#include "drivers/ads1220.h"

#include <errno.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/kernel.h>

#define BOARD_IO DT_NODELABEL(bccpm_io)
#define BUTTON_DEBOUNCE_MS 20

static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(DT_NODELABEL(user_button), gpios);
static const struct pwm_dt_spec buzzer = PWM_DT_SPEC_GET(BOARD_IO);
static bool initialized;

int board_io_init(void)
{
	int ret;

	initialized = false;
	if (!gpio_is_ready_dt(&button) ||
	    !pwm_is_ready_dt(&buzzer)) {
		return -ENODEV;
	}
	ret = ads1220_bridge_init();
	if (ret < 0) { return ret; }
	ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
	if (ret < 0) { return ret; }
	ret = pwm_set_dt(&buzzer, buzzer.period, 0);
	if (ret < 0) { return ret; }
	ret = power_init();
	if (ret < 0) { return ret; }
	initialized = true;
	return 0;
}

int board_io_button_pressed(bool *pressed)
{
	int first, second;

	if (!pressed) { return -EINVAL; }
	if (!initialized) { return -EACCES; }
	first = gpio_pin_get_dt(&button);
	if (first < 0) { return first; }
	k_sleep(K_MSEC(BUTTON_DEBOUNCE_MS));
	second = gpio_pin_get_dt(&button);
	if (second < 0) { return second; }
	if (first != second) { return -EAGAIN; }
	*pressed = second != 0;
	return 0;
}

int board_io_buzzer_set(bool on)
{
	if (!initialized) { return -EACCES; }
	return pwm_set_dt(&buzzer, buzzer.period, on ? buzzer.period / 2 : 0);
}

int board_io_buzzer_configure(uint32_t frequency_hz, uint8_t duty_percent)
{
	uint32_t period, pulse;
	if (!initialized) { return -EACCES; }
	if (frequency_hz == 0 || frequency_hz > 1000000000U || duty_percent > 100) {
		return -EINVAL;
	}
	period = 1000000000U / frequency_hz;
	pulse = (uint32_t)(((uint64_t)period * duty_percent) / 100U);
	return pwm_set_dt(&buzzer, period, pulse);
}
