#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/services/nus.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

static const struct device *const i2c_bus = DEVICE_DT_GET(DT_NODELABEL(i2c0));
static const struct device *const ble_console = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));

struct connection_event {
	bool connected;
	uint8_t status;
};

K_MSGQ_DEFINE(connection_events, sizeof(struct connection_event), 4, 1);

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_NUS_SRV_VAL),
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

static void connected(struct bt_conn *conn, uint8_t err)
{
	const struct connection_event event = {.connected = true, .status = err};

	ARG_UNUSED(conn);
	/* The BLE UART can block when full, so print from main, not BT RX. */
	(void)k_msgq_put(&connection_events, &event, K_NO_WAIT);
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	const struct connection_event event = {.connected = false, .status = reason};

	ARG_UNUSED(conn);
	(void)k_msgq_put(&connection_events, &event, K_NO_WAIT);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
	.recycled = recycled,
};

int main(void)
{
	int err;
	struct connection_event event;
	unsigned char c;

	if (!device_is_ready(ble_console)) {
		return 0;
	}

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

	/* Subscribe to NUS TX notifications to receive buffered printk text.
	 * NUS RX writes are echoed here to verify bidirectional text transport.
	 */
	while (true) {
		while (k_msgq_get(&connection_events, &event, K_NO_WAIT) == 0) {
			if (event.connected) {
				printk("BLE connection status: 0x%02x\n", event.status);
			} else {
				printk("BLE disconnected: 0x%02x\n", event.status);
			}
		}

		for (int i = 0; i < 64 && uart_poll_in(ble_console, &c) == 0; i++) {
			printk("%c", c);
		}
		k_sleep(K_MSEC(20));
	}
}
