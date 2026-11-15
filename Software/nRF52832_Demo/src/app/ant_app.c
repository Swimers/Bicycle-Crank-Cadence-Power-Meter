#include "ant_app.h"

#include <errno.h>
#include <stddef.h>

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

int ant_app_init(const struct ant_app_channel_config *config)
{
	if (config == NULL || config->device_number == 0U) {
		return -EINVAL;
	}
	/* NCS 3.4.1 in this workspace has no ANT stack. Do not reconfigure
	 * RADIO, install S332 over SDC, or report a channel as opened.
	 */
	return -ENOTSUP;
}

void ant_app_process(void)
{
	/* Reserved for an actual ANT backend and channel-event scheduler. */
}

int ant_app_send_power_only(const struct ant_app_power_only_data *data)
{
	if (data == NULL || data->page_number != ANT_APP_PAGE_POWER_ONLY) {
		return -EINVAL;
	}
	return -ENOTSUP;
}

int ant_app_send_crank_torque(const struct ant_app_crank_torque_data *data)
{
	if (data == NULL || data->page_number != ANT_APP_PAGE_CRANK_TORQUE) {
		return -EINVAL;
	}
	return -ENOTSUP;
}
