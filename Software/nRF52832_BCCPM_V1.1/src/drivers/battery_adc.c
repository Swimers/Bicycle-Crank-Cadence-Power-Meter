#include "battery_adc.h"

#include <errno.h>
#include <stdbool.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

#define BOARD_IO DT_NODELABEL(bccpm_io)
#define SETTLE_MS 20
#define SAMPLE_COUNT 4
#define DIVIDER_TOP_OHMS 1500000LL
#define DIVIDER_BOTTOM_OHMS 220000LL

static const struct adc_dt_spec battery = ADC_DT_SPEC_GET_BY_IDX(BOARD_IO, 0);
static const struct gpio_dt_spec enable = GPIO_DT_SPEC_GET(BOARD_IO, bat_mon_en_gpios);
static bool initialized;

int battery_adc_init(void)
{
	int ret;

	initialized = false;
	if (!adc_is_ready_dt(&battery) || !gpio_is_ready_dt(&enable)) {
		return -ENODEV;
	}
	ret = gpio_pin_configure_dt(&enable, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		return ret;
	}
	ret = adc_channel_setup_dt(&battery);
	if (ret < 0) {
		return ret;
	}
	initialized = true;
	return 0;
}

int battery_adc_read_mv(uint32_t *battery_mv)
{
	int16_t raw;
	int64_t total = 0;
	struct adc_sequence sequence = { .buffer = &raw, .buffer_size = sizeof(raw), .calibrate = true };
	int ret, off_ret;

	if (!battery_mv) {
		return -EINVAL;
	}
	if (!initialized) {
		return -EACCES;
	}
	ret = adc_sequence_init_dt(&battery, &sequence);
	if (ret < 0) {
		return ret;
	}
	ret = gpio_pin_set_dt(&enable, 1);
	if (ret < 0) {
		return ret;
	}
	k_sleep(K_MSEC(SETTLE_MS));
	/* Discard the first acquisition to help settle the SAADC input. */
	ret = adc_read_dt(&battery, &sequence);
	sequence.calibrate = false;
	for (int i = 0; ret == 0 && i < SAMPLE_COUNT; ++i) {
		int32_t mv;
		ret = adc_read_dt(&battery, &sequence);
		if (ret < 0) {
			break;
		}
		mv = raw;
		ret = adc_raw_to_millivolts_dt(&battery, &mv);
		if (ret == 0) {
			total += mv;
		}
	}
	off_ret = gpio_pin_set_dt(&enable, 0);
	if (ret < 0) {
		return ret;
	}
	if (off_ret < 0) {
		return off_ret;
	}
	if (total < 0) {
		return -ERANGE;
	}
	/* Round once after averaging and reversing the divider ratio. */
	*battery_mv = (uint32_t)((total * (DIVIDER_TOP_OHMS + DIVIDER_BOTTOM_OHMS) +
		(SAMPLE_COUNT * DIVIDER_BOTTOM_OHMS / 2)) /
		(SAMPLE_COUNT * DIVIDER_BOTTOM_OHMS));
	return 0;
}
