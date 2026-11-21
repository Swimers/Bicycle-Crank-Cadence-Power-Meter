#include <string.h>
#include <zephyr/bluetooth/att.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/kernel.h>
#include <zephyr/retention/bootmode.h>
#include <zephyr/sys/reboot.h>

#define DFU_SERVICE_UUID BT_UUID_128_ENCODE(0x85b8f1a0, 0xc8ed, 0x4be7, 0xa48f, 0x47d72e308066)
#define DFU_CONTROL_UUID BT_UUID_128_ENCODE(0x85b8f1a1, 0xc8ed, 0x4be7, 0xa48f, 0x47d72e308066)
#define DFU_REBOOT_DELAY_MS 500

static const char enter_command[] = "OTA ENTER";

static void reboot_to_loader(struct k_work *work)
{
	ARG_UNUSED(work);
	sys_reboot(SYS_REBOOT_COLD);
}

static K_WORK_DELAYABLE_DEFINE(dfu_reboot_work, reboot_to_loader);

static ssize_t write_dfu_control(struct bt_conn *conn, const struct bt_gatt_attr *attr,
				const void *buf, uint16_t len, uint16_t offset, uint8_t flags)
{
	ARG_UNUSED(conn);
	ARG_UNUSED(attr);

	if (flags != 0) {
		return BT_GATT_ERR(BT_ATT_ERR_WRITE_NOT_PERMITTED);
	}
	if (offset != 0) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}
	if (len != sizeof(enter_command) - 1) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}
	if (memcmp(buf, enter_command, len) != 0) {
		return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
	}
	if (bootmode_set(BOOT_MODE_TYPE_BOOTLOADER) != 0) {
		return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
	}

	/* Leave time for the ATT Write Response before resetting the connection. */
	if (k_work_schedule(&dfu_reboot_work, K_MSEC(DFU_REBOOT_DELAY_MS)) < 0) {
		bootmode_clear();
		return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
	}
	return len;
}

BT_GATT_SERVICE_DEFINE(dfu_service,
	BT_GATT_PRIMARY_SERVICE(BT_UUID_DECLARE_128(DFU_SERVICE_UUID)),
	BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_128(DFU_CONTROL_UUID), BT_GATT_CHRC_WRITE,
			       BT_GATT_PERM_WRITE, NULL, write_dfu_control, NULL)
);
