#include "power.h"
#include "drivers/battery_adc.h"

#include <errno.h>
#include <stddef.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>

#define BOARD_IO DT_NODELABEL(bccpm_io)
static const struct gpio_dt_spec mode = GPIO_DT_SPEC_GET(BOARD_IO, pmic_mode_gpios);
static const struct gpio_dt_spec iset = GPIO_DT_SPEC_GET(BOARD_IO, pmic_iset_gpios);
static const struct gpio_dt_spec power_off = GPIO_DT_SPEC_GET(BOARD_IO, power_off_gpios);
static bool initialized;

/* Approximate unloaded single-cell curve; replace with measured cell data. */
static const struct { uint16_t mv; uint8_t percent; } curve[] = {
	{3000, 0}, {3300, 5}, {3500, 10}, {3600, 20}, {3700, 40},
	{3800, 60}, {3900, 75}, {4000, 85}, {4100, 95}, {4200, 100},
};

static uint8_t estimate_percent(uint32_t mv)
{
	if (mv <= curve[0].mv) {
		return 0;
	}
	for (size_t i = 1; i < sizeof(curve) / sizeof(curve[0]); ++i) {
		if (mv <= curve[i].mv) {
			uint32_t span = curve[i].mv - curve[i - 1].mv;
			uint32_t delta = (mv - curve[i - 1].mv) *
				(curve[i].percent - curve[i - 1].percent);
			return curve[i - 1].percent + (delta + span / 2) / span;
		}
	}
	return 100;
}

int power_init(void)
{
	int ret;

	initialized = false;
	if (!gpio_is_ready_dt(&mode) || !gpio_is_ready_dt(&iset) ||
	    !gpio_is_ready_dt(&power_off)) {
		return -ENODEV;
	}
	ret = gpio_pin_configure_dt(&power_off, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) { return ret; }
	ret = gpio_pin_configure_dt(&mode, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) { return ret; }
	ret = gpio_pin_configure_dt(&iset, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) { return ret; }
	ret = battery_adc_init();
	if (ret < 0) { return ret; }
	initialized = true;
	return 0;
}

static int set_level(const struct gpio_dt_spec *pin, bool high)
{
	return initialized ? gpio_pin_set_dt(pin, high ? 1 : 0) : -EACCES;
}

int power_set_mode(bool high) { return set_level(&mode, high); }
int power_set_iset(bool high) { return set_level(&iset, high); }
int power_set_power_off(bool high) { return set_level(&power_off, high); }

int power_battery_read(struct power_battery_sample *sample)
{
	struct power_battery_sample result;
	int ret;

	if (!sample) {
		return -EINVAL;
	}
	if (!initialized) {
		return -EACCES;
	}
	ret = battery_adc_read_mv(&result.voltage_mv);
	if (ret < 0) {
		return ret;
	}
	result.percent = estimate_percent(result.voltage_mv);
	*sample = result;
	return 0;
}
