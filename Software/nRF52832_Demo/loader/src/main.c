#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/mgmt/mcumgr/transport/smp_bt.h>
#include <zephyr/mgmt/mcumgr/mgmt/callbacks.h>
#include <zephyr/devicetree.h>
#include <fprotect.h>

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR),
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, SMP_BT_SVC_UUID_VAL),
};
static const struct bt_data sd[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME,
		sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

static void advertise(struct k_work *work)
{
	ARG_UNUSED(work);
	(void)bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad),
			      sd, ARRAY_SIZE(sd));
}
K_WORK_DEFINE(advertising_work, advertise);

static void recycled(void)
{
	k_work_submit(&advertising_work);
}

BT_CONN_CB_DEFINE(callbacks) = {
	.recycled = recycled,
};

static enum mgmt_cb_return command_guard(uint32_t event,
		enum mgmt_cb_return previous, int32_t *rc, uint16_t *group,
		bool *abort_more, void *data, size_t size)
{
	const struct mgmt_evt_op_cmd_arg *command = data;

	ARG_UNUSED(event);
	ARG_UNUSED(group);
	if (previous != MGMT_CB_OK || size != sizeof(*command)) {
		return previous;
	}
	/* Upload already erases progressively. Disable the standalone image
	 * erase command (ID 5), including explicit requests targeting slot1.
	 */
	if (command->group == MGMT_GROUP_ID_IMAGE && command->id == 5) {
		*rc = MGMT_ERR_ENOTSUP;
		*abort_more = true;
		return MGMT_CB_ERROR_RC;
	}
	return MGMT_CB_OK;
}

static struct mgmt_callback command_callback = {
	.callback = command_guard,
	.event_id = MGMT_EVT_OP_CMD_RECV,
};

int main(void)
{
	/* BPROT survives until reset: malformed SMP commands cannot erase Loader,
	 * bootloader or settings. Only the business slot remains writable.
	 */
	/* Internal flash starts at zero on this board. Read the fixed DTS map
	 * directly so the protection code works across both supported SDKs.
	 */
	if (fprotect_area(DT_REG_ADDR(DT_NODELABEL(boot_partition)),
			  DT_REG_SIZE(DT_NODELABEL(boot_partition))) != 0 ||
	    fprotect_area(DT_REG_ADDR(DT_NODELABEL(slot1_partition)),
			  DT_REG_SIZE(DT_NODELABEL(slot1_partition))) != 0 ||
	    fprotect_area(DT_REG_ADDR(DT_NODELABEL(storage_partition)),
			  DT_REG_SIZE(DT_NODELABEL(storage_partition))) != 0) {
		return 0;
	}
	if (bt_enable(NULL) != 0) {
		return 0;
	}
	mgmt_callback_register(&command_callback);
	k_work_submit(&advertising_work);
	return 0;
}
