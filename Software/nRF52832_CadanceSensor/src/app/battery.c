#include "battery.h"

#include <errno.h>
#include <zephyr/bluetooth/att.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>

#define BOARD_IO DT_PATH(zephyr_user)
#define DIVIDER_FULL_OHMS DT_PROP(BOARD_IO, bat_mon_full_ohms)
#define DIVIDER_OUTPUT_OHMS DT_PROP(BOARD_IO, bat_mon_output_ohms)
#define DIVIDER_SWITCH_DROP_MV DT_PROP_OR(BOARD_IO, bat_mon_switch_drop_mv, 0)
#define BATTERY_SETTLE_MS 150
#define BATTERY_SAMPLE_COUNT 8
#define BATTERY_SAMPLE_INTERVAL_US 1000
#define MIN_VALID_BATTERY_MV 2000
#define MAX_VALID_BATTERY_MV 4500
#define SHIP_REQUEST_MS 250

static const struct adc_dt_spec voltage = ADC_DT_SPEC_GET_BY_IDX(BOARD_IO, 0);
static const struct gpio_dt_spec divider_enable = GPIO_DT_SPEC_GET(BOARD_IO, bat_mon_en_gpios);
static const struct gpio_dt_spec power_off = GPIO_DT_SPEC_GET(BOARD_IO, pmic_power_off_gpios);
static bool adc_calibrated;
static atomic_t battery_level;

/* Approximate single-cell 4.2 V LiPo discharge curve. Tune with measured cell data. */
static const struct {
	uint16_t mv;
	uint8_t percent;
} discharge_curve[] = {
	{3000, 1}, {3300, 5}, {3500, 10}, {3600, 20}, {3700, 40},
	{3800, 60}, {3900, 75}, {4000, 85}, {4100, 95}, {4200, 100},
};

static uint8_t estimate_percent(int32_t mv)
{
	for (size_t i = 1; i < ARRAY_SIZE(discharge_curve); i++) {
		if (mv <= discharge_curve[i].mv) {
			int32_t lower_mv = discharge_curve[i - 1].mv;
			int32_t lower_percent = discharge_curve[i - 1].percent;
			int32_t rise = MAX(mv - lower_mv, 0);
			int32_t span = discharge_curve[i].mv - lower_mv;

			return (uint8_t)(lower_percent +
					 rise * (discharge_curve[i].percent - lower_percent) / span);
		}
	}
	return 100;
}

static ssize_t read_battery_level(struct bt_conn *conn, const struct bt_gatt_attr *attr,
				  void *buf, uint16_t len, uint16_t offset)
{
	uint8_t level = (uint8_t)atomic_get(&battery_level);

	/* Keep the service readable after recovery without inventing a startup level. */
	if (level == 0) {
		return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
	}
	return bt_gatt_attr_read(conn, attr, buf, len, offset, &level, sizeof(level));
}

/* Zephyr sorts static services by name. Append this after CSC and DFU. */
BT_GATT_SERVICE_DEFINE(gatt_battery_service,
	BT_GATT_PRIMARY_SERVICE(BT_UUID_BAS),
	BT_GATT_CHARACTERISTIC(BT_UUID_BAS_BATTERY_LEVEL, BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
			       BT_GATT_PERM_READ, read_battery_level, NULL, NULL),
	BT_GATT_CCC(NULL, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE)
);

int battery_init(void)
{
	if (!adc_is_ready_dt(&voltage) || !gpio_is_ready_dt(&divider_enable) ||
	    !gpio_is_ready_dt(&power_off)) {
		return -ENODEV;
	}
	int err = gpio_pin_configure_dt(&power_off, GPIO_OUTPUT_INACTIVE);

	if (err == 0) {
		err = gpio_pin_configure_dt(&divider_enable, GPIO_OUTPUT_INACTIVE);
	}
	return err == 0 ? adc_channel_setup_dt(&voltage) : err;
}

int battery_sample(void)
{
	/* One first conversion to discard, followed by a batch to average. */
	int16_t readings[BATTERY_SAMPLE_COUNT + 1];
	const struct adc_sequence_options options = {
		.extra_samplings = BATTERY_SAMPLE_COUNT,
		.interval_us = BATTERY_SAMPLE_INTERVAL_US,
	};
	struct adc_sequence sequence = {
		.options = &options,
		.buffer = readings,
		.buffer_size = sizeof(readings),
		.calibrate = !adc_calibrated,
	};
	int err = adc_sequence_init_dt(&voltage, &sequence);

	if (err != 0) {
		return err;
	}
	err = gpio_pin_set_dt(&divider_enable, 1);
	if (err != 0) {
		return err;
	}
	k_msleep(BATTERY_SETTLE_MS);
	err = adc_read_dt(&voltage, &sequence);
	int disable_err = gpio_pin_set_dt(&divider_enable, 0);
	if (err != 0) {
		return err;
	}
	adc_calibrated = true;
	if (disable_err != 0) {
		return disable_err;
	}

	int32_t sum = 0;
	for (size_t i = 1; i < ARRAY_SIZE(readings); i++) {
		sum += readings[i];
	}
	int32_t input_uv = DIV_ROUND_CLOSEST(sum, BATTERY_SAMPLE_COUNT);

	err = adc_raw_to_microvolts_dt(&voltage, &input_uv);
	if (err != 0) {
		return err;
	}
	int32_t switched_mv = DIV_ROUND_CLOSEST((int64_t)input_uv * DIVIDER_FULL_OHMS,
					       (int64_t)DIVIDER_OUTPUT_OHMS * 1000);
	int32_t battery_mv = switched_mv + DIVIDER_SWITCH_DROP_MV;

	/* Check the physical input as well so compensation cannot mask an open divider. */
	if (switched_mv < MIN_VALID_BATTERY_MV || battery_mv > MAX_VALID_BATTERY_MV) {
		return -ERANGE;
	}

	uint8_t level = estimate_percent(battery_mv);

	atomic_set(&battery_level, level);
	if (bt_is_ready()) {
		(void)bt_gatt_notify_uuid(NULL, BT_UUID_BAS_BATTERY_LEVEL,
					  gatt_battery_service.attrs, &level, sizeof(level));
	}
	return level;
}

int battery_request_power_off(void)
{
	int err = gpio_pin_set_dt(&power_off, 1);

	if (err != 0) {
		return err;
	}
	k_msleep(SHIP_REQUEST_MS);
	/* If VBUS prevented entry to Ship mode, release SHPACT so charging can resume. */
	return gpio_pin_set_dt(&power_off, 0);
}
