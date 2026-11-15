#include "ant_app.h"

#include <errno.h>
#include <stddef.h>

#if defined(CONFIG_ANT)
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/printk.h>
#include <ant_init.h>
#include <ant_interface.h>
#include <ant_parameters.h>
#include <ant_channel_config.h>
#include <ant_key_manager.h>
#endif

/* Check declared field positions without relying on C bitfields or packing.
 * These checks do not replace an explicit little-endian wire serializer.
 */
_Static_assert(sizeof(struct ant_app_power_only_data) == ANT_APP_PAYLOAD_SIZE,
	       "Power-only page layout must contain eight bytes");
_Static_assert(offsetof(struct ant_app_power_only_data, accumulated_power) == 4,
	       "Power accumulation field offset");
_Static_assert(offsetof(struct ant_app_power_only_data, instantaneous_power) == 6,
	       "Instantaneous power field offset");
_Static_assert(sizeof(struct ant_app_crank_torque_data) == ANT_APP_PAYLOAD_SIZE,
	       "Crank-torque page layout must contain eight bytes");
_Static_assert(offsetof(struct ant_app_crank_torque_data, accumulated_crank_period) == 4,
	       "Crank period field offset");
_Static_assert(offsetof(struct ant_app_crank_torque_data, accumulated_torque) == 6,
	       "Torque accumulation field offset");

const struct ant_app_channel_config ant_app_default_channel_config = {
	.channel_number = 0,
	.network_number = 0,
	.channel_type = ANT_APP_CHANNEL_BIDIR_MASTER,
	.device_type = ANT_APP_DEVICE_TYPE_BICYCLE_POWER,
	.transmission_type = ANT_APP_TRANSMISSION_TYPE,
	.rf_channel = ANT_APP_RF_CHANNEL,
	.device_number = 1,
	.channel_period = ANT_APP_CHANNEL_PERIOD,
};

#if defined(CONFIG_ANT)
static struct k_spinlock payload_lock;
static uint8_t pending_payload[ANT_APP_PAYLOAD_SIZE];
static struct ant_app_power_only_data fixed_power;
static struct ant_app_channel_config channel;
static bool fixed_mode;
static bool init_attempted;
static atomic_t channel_open;
static atomic_t tx_events;
static atomic_t tx_errors;
static atomic_t last_stack_error;

static void put_le16(uint8_t *out, uint16_t value)
{
	out[0] = (uint8_t)value;
	out[1] = (uint8_t)(value >> 8);
}

static void encode_power(uint8_t out[ANT_APP_PAYLOAD_SIZE],
			 const struct ant_app_power_only_data *data)
{
	out[0] = ANT_APP_PAGE_POWER_ONLY;
	out[1] = data->update_event_count;
	out[2] = data->pedal_power;
	out[3] = data->instantaneous_cadence;
	put_le16(&out[4], data->accumulated_power);
	put_le16(&out[6], data->instantaneous_power);
}

static void encode_crank(uint8_t out[ANT_APP_PAYLOAD_SIZE],
			 const struct ant_app_crank_torque_data *data)
{
	out[0] = ANT_APP_PAGE_CRANK_TORQUE;
	out[1] = data->update_event_count;
	out[2] = data->crank_ticks;
	out[3] = data->instantaneous_cadence;
	put_le16(&out[4], data->accumulated_crank_period);
	put_le16(&out[6], data->accumulated_torque);
}

static int stack_result(ant_err_t err)
{
	if (err != 0) {
		atomic_set(&last_stack_error, (atomic_val_t)err);
		return -EIO;
	}
	return 0;
}

static void ant_event(ant_evt_t *event)
{
	if (event->channel != channel.channel_number) {
		return;
	}
	if (event->event == EVENT_CHANNEL_CLOSED) {
		atomic_clear(&channel_open);
		return;
	}
	if (event->event != EVENT_TX || !atomic_get(&channel_open)) {
		return;
	}
	atomic_inc(&tx_events);
	uint8_t payload[ANT_APP_PAYLOAD_SIZE];
	k_spinlock_key_t key = k_spin_lock(&payload_lock);

	if (fixed_mode) {
		/* Values stay fixed; a new time-synchronous measurement is made
		 * at each channel event. Protocol counters must still progress.
		 */
		fixed_power.update_event_count++;
		fixed_power.accumulated_power += ANT_APP_FIXED_POWER_WATTS;
		encode_power(pending_payload, &fixed_power);
	}
	memcpy(payload, pending_payload, sizeof(payload));
	k_spin_unlock(&payload_lock, key);
	/* ANT's callback context supports filling the next TX buffer. No
	 * printk, BLE calls, sleeping, or sampling work runs in this callback.
	 */
	if (stack_result(ant_broadcast_message_tx(channel.channel_number,
					 sizeof(payload), payload)) != 0) {
		atomic_inc(&tx_errors);
	}
}
#endif

int ant_app_init(const struct ant_app_channel_config *config)
{
	if (config == NULL || config->device_number == 0U) {
		return -EINVAL;
	}
#if defined(CONFIG_ANT)
	if (init_attempted) {
		return -EALREADY;
	}
	if (config->channel_number >= CONFIG_ANT_TOTAL_CHANNELS_ALLOCATED ||
	    config->device_type != ANT_APP_DEVICE_TYPE_BICYCLE_POWER ||
	    config->channel_type != ANT_APP_CHANNEL_BIDIR_MASTER ||
	    config->rf_channel != ANT_APP_RF_CHANNEL ||
	    config->channel_period != ANT_APP_CHANNEL_PERIOD ||
	    config->transmission_type != ANT_APP_TRANSMISSION_TYPE) {
		return -EINVAL;
	}
	init_attempted = true;
	channel = *config;
	int err = stack_result(ant_init());

	if (err == 0) {
		err = stack_result(ant_cb_register(ant_event));
	}
	if (err == 0) {
		err = stack_result(ant_plus_key_set(channel.network_number));
	}
	if (err != 0) {
		return err;
	}
	ant_channel_config_t cfg = {
		.channel_number = channel.channel_number,
		.channel_type = channel.channel_type,
		.ext_assign = 0,
		.rf_freq = channel.rf_channel,
		.transmission_type = channel.transmission_type,
		.device_type = channel.device_type,
		.device_number = channel.device_number,
		.channel_period = channel.channel_period,
		.network_number = channel.network_number,
	};
	err = stack_result(ant_channel_init(&cfg));
	if (err != 0) {
		(void)ant_channel_unassign(channel.channel_number);
		return err;
	}
	fixed_power = (struct ant_app_power_only_data){
		.page_number = ANT_APP_PAGE_POWER_ONLY,
		.pedal_power = ANT_APP_PEDAL_POWER_UNAVAILABLE,
		.instantaneous_cadence = ANT_APP_FIXED_CADENCE_RPM,
		.instantaneous_power = ANT_APP_FIXED_POWER_WATTS,
	};
	fixed_mode = true;
	encode_power(pending_payload, &fixed_power);
	err = stack_result(ant_broadcast_message_tx(channel.channel_number,
						 sizeof(pending_payload), pending_payload));
	if (err == 0) {
		atomic_set(&channel_open, 1);
		err = stack_result(ant_channel_open(channel.channel_number));
	}
	if (err != 0) {
		atomic_clear(&channel_open);
		(void)ant_channel_unassign(channel.channel_number);
	}
	return err;
#else
	return -ENOTSUP;
#endif
}

void ant_app_process(void)
{
#if defined(CONFIG_ANT)
	static int64_t next_report;
	int64_t now = k_uptime_get();

	if (now >= next_report) {
		uint8_t payload[ANT_APP_PAYLOAD_SIZE];
		bool is_fixed;
		k_spinlock_key_t key = k_spin_lock(&payload_lock);

		memcpy(payload, pending_payload, sizeof(payload));
		is_fixed = fixed_mode;
		k_spin_unlock(&payload_lock, key);
		next_report = now + 1000;
		printk("ANT ch=%u open=%d tx=%ld errors=%ld stack=0x%lx page=0x%02x fixed=%d\n",
		       channel.channel_number, (int)atomic_get(&channel_open),
		       (long)atomic_get(&tx_events), (long)atomic_get(&tx_errors),
		       (unsigned long)atomic_get(&last_stack_error),
		       payload[0], (int)is_fixed);
	}
#endif
}

int ant_app_send_power_only(const struct ant_app_power_only_data *data)
{
	if (data == NULL || data->page_number != ANT_APP_PAGE_POWER_ONLY) {
		return -EINVAL;
	}
#if defined(CONFIG_ANT)
	if (!atomic_get(&channel_open)) {
		return -ENODEV;
	}
	uint8_t payload[ANT_APP_PAYLOAD_SIZE];

	encode_power(payload, data);
	k_spinlock_key_t key = k_spin_lock(&payload_lock);
	fixed_mode = false;
	memcpy(pending_payload, payload, sizeof(payload));
	k_spin_unlock(&payload_lock, key);
	return 0;
#else
	return -ENOTSUP;
#endif
}

int ant_app_send_crank_torque(const struct ant_app_crank_torque_data *data)
{
	if (data == NULL || data->page_number != ANT_APP_PAGE_CRANK_TORQUE) {
		return -EINVAL;
	}
#if defined(CONFIG_ANT)
	if (!atomic_get(&channel_open)) {
		return -ENODEV;
	}
	uint8_t payload[ANT_APP_PAYLOAD_SIZE];

	encode_crank(payload, data);
	k_spinlock_key_t key = k_spin_lock(&payload_lock);
	fixed_mode = false;
	memcpy(pending_payload, payload, sizeof(payload));
	k_spin_unlock(&payload_lock, key);
	return 0;
#else
	return -ENOTSUP;
#endif
}
