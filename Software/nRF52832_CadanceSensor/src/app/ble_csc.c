#include "ble_csc.h"

#include <errno.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/byteorder.h>

#define CSC_REPORT_INTERVAL_MS 100
#define CSC_EVENT_TICKS_PER_SECOND 1024U
#define CSC_CRANK_DATA_PRESENT BIT(1)
#define CSC_CRANK_FEATURE BIT(1)

static struct k_spinlock event_lock;
static uint16_t crank_revolutions;
static uint16_t last_crank_event;
static atomic_t subscribed;

static void report_measurement(struct k_work *work);
K_WORK_DELAYABLE_DEFINE(report_work, report_measurement);

static const struct bt_data advertising_data[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL, BT_UUID_16_ENCODE(BT_UUID_CSC_VAL),
		      BT_UUID_16_ENCODE(BT_UUID_BAS_VAL)),
};

static const struct bt_data scan_response[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
	BT_DATA_BYTES(BT_DATA_GAP_APPEARANCE, BT_BYTES_LIST_LE16(CONFIG_BT_DEVICE_APPEARANCE)),
};

static ssize_t read_features(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			     void *buf, uint16_t len, uint16_t offset)
{
	uint16_t features = sys_cpu_to_le16(CSC_CRANK_FEATURE);

	return bt_gatt_attr_read(conn, attr, buf, len, offset, &features, sizeof(features));
}

static void measurement_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	ARG_UNUSED(attr);

	if (value == BT_GATT_CCC_NOTIFY) {
		atomic_set(&subscribed, 1);
		k_work_reschedule(&report_work, K_NO_WAIT);
	} else {
		atomic_clear(&subscribed);
		k_work_cancel_delayable(&report_work);
	}
}

BT_GATT_SERVICE_DEFINE(csc_service,
	BT_GATT_PRIMARY_SERVICE(BT_UUID_CSC),
	BT_GATT_CHARACTERISTIC(BT_UUID_CSC_MEASUREMENT, BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_NONE,
			       NULL, NULL, NULL),
	BT_GATT_CCC(measurement_ccc_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
	BT_GATT_CHARACTERISTIC(BT_UUID_CSC_FEATURE, BT_GATT_CHRC_READ, BT_GATT_PERM_READ,
			       read_features, NULL, NULL)
);

static void report_measurement(struct k_work *work)
{
	ARG_UNUSED(work);

	if (!atomic_get(&subscribed)) {
		return;
	}

	uint8_t measurement[5] = {CSC_CRANK_DATA_PRESENT};
	k_spinlock_key_t key = k_spin_lock(&event_lock);

	sys_put_le16(crank_revolutions, &measurement[1]);
	sys_put_le16(last_crank_event, &measurement[3]);
	k_spin_unlock(&event_lock, key);

	/* Each notification is a snapshot. Retry with fresh data on the next tick. */
	(void)bt_gatt_notify_uuid(NULL, BT_UUID_CSC_MEASUREMENT, csc_service.attrs,
				  measurement, sizeof(measurement));
	if (atomic_get(&subscribed)) {
		k_work_schedule(&report_work, K_MSEC(CSC_REPORT_INTERVAL_MS));
	}
}

static void start_advertising(struct k_work *work);
K_WORK_DELAYABLE_DEFINE(advertising_work, start_advertising);

static void start_advertising(struct k_work *work)
{
	ARG_UNUSED(work);

	int err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_2,
				  advertising_data, ARRAY_SIZE(advertising_data),
				  scan_response, ARRAY_SIZE(scan_response));

	if (err != 0 && err != -EALREADY) {
		k_work_schedule(&advertising_work, K_SECONDS(1));
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	ARG_UNUSED(conn);
	ARG_UNUSED(reason);

	atomic_clear(&subscribed);
	k_work_cancel_delayable(&report_work);
}

static void recycled(void)
{
	/* A new connectable advertisement can use the released connection slot. */
	k_work_reschedule(&advertising_work, K_NO_WAIT);
}

BT_CONN_CB_DEFINE(csc_connection_callbacks) = {
	.disconnected = disconnected,
	.recycled = recycled,
};

int ble_csc_start(void)
{
	int err = bt_enable(NULL);

	if (err == 0) {
		k_work_schedule(&advertising_work, K_NO_WAIT);
	}
	return err;
}

void ble_csc_record_revolution(uint64_t time_ms)
{
	uint16_t event_time = (uint16_t)(time_ms * CSC_EVENT_TICKS_PER_SECOND / MSEC_PER_SEC);
	k_spinlock_key_t key = k_spin_lock(&event_lock);

	crank_revolutions++;
	last_crank_event = event_time;
	k_spin_unlock(&event_lock, key);
}
