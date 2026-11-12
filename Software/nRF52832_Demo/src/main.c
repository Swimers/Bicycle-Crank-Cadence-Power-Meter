#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

static const struct device *const i2c_bus = DEVICE_DT_GET(DT_NODELABEL(i2c0));

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
};

static const struct bt_data sd[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME,
		sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

static void start_advertising(struct k_work *work)
{
	int err;

	ARG_UNUSED(work);
	err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1,
			      ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
	if (err) {
		printk("Bluetooth advertising failed (%d)\n", err);
		return;
	}
	printk("Bluetooth advertising as %s\n", CONFIG_BT_DEVICE_NAME);
}

K_WORK_DEFINE(advertising_work, start_advertising);

static void recycled(void)
{
	/* Restart only after the disconnected connection object is released. */
	k_work_submit(&advertising_work);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.recycled = recycled,
};

int main(void)
{
	int err;

	if (!device_is_ready(i2c_bus)) {
		printk("I2C0 controller is not ready\n");
		return 0;
	}
	printk("I2C0 ready: SCL=P0.15, SDA=P0.14, 100 kHz\n");

	err = bt_enable(NULL);
	if (err) {
		printk("Bluetooth initialization failed (%d)\n", err);
		return 0;
	}

	k_work_submit(&advertising_work);

	return 0;
}
