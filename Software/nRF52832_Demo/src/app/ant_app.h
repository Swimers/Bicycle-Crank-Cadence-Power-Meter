#ifndef CADENCE_ANT_APP_H_
#define CADENCE_ANT_APP_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ANT_APP_PAYLOAD_SIZE             8U
#define ANT_APP_PAGE_POWER_ONLY          0x10U
#define ANT_APP_PAGE_CRANK_TORQUE        0x12U
#define ANT_APP_CADENCE_UNAVAILABLE      0xffU
#define ANT_APP_PEDAL_POWER_UNAVAILABLE  0xffU

#define ANT_APP_CHANNEL_BIDIR_MASTER    0x10U
#define ANT_APP_DEVICE_TYPE_BICYCLE_POWER 0x0bU
#define ANT_APP_TRANSMISSION_TYPE        0x05U
#define ANT_APP_RF_CHANNEL               57U   /* 2400 + 57 = 2457 MHz. */
#define ANT_APP_CHANNEL_PERIOD          8182U /* Units: 1/32768 second. */

struct ant_app_channel_config {
	uint8_t channel_number;
	uint8_t network_number; /* Local stack network index, not a network key. */
	uint8_t channel_type;
	uint8_t device_type;
	uint8_t transmission_type;
	uint8_t rf_channel;
	uint16_t device_number; /* Must be nonzero and unique for each product. */
	uint16_t channel_period;
};

/* Profile defaults only. Device number 1 is a development placeholder;
 * replace it with a per-device identifier when a real backend is added.
 * The backend must separately configure the ANT+ managed network key.
 */
extern const struct ant_app_channel_config ant_app_default_channel_config;

/* Page 0x10: caller supplies an updated measurement snapshot.
 * Native integer values, NOT a buffer to pass directly to a radio API.
 * A future serializer must explicitly write 16-bit values little endian.
 */
struct ant_app_power_only_data {
	uint8_t page_number;            /* ANT_APP_PAGE_POWER_ONLY. */
	uint8_t update_event_count;     /* Wraps at 256 measurement updates. */
	uint8_t pedal_power;            /* Encoded percentage/side, or 0xff. */
	uint8_t instantaneous_cadence;  /* RPM: 0..254, 0xff = unavailable. */
	uint16_t accumulated_power;     /* Sum of updated power values, wraps at 65536. */
	uint16_t instantaneous_power;   /* Watts. */
};

/* Page 0x12: cumulative quantities, not a single revolution's duration/torque.
 * Native integer values; future wire encoding must be little endian.
 */
struct ant_app_crank_torque_data {
	uint8_t page_number;            /* ANT_APP_PAGE_CRANK_TORQUE. */
	uint8_t update_event_count;     /* Wraps at 256 measurement updates. */
	uint8_t crank_ticks;            /* Cumulative revolutions, wraps at 256. */
	uint8_t instantaneous_cadence;  /* RPM: 0..254, 0xff = unavailable. */
	uint16_t accumulated_crank_period; /* Units: 1/2048 s, wraps at 65536. */
	uint16_t accumulated_torque;    /* Units: 1/32 Nm, wraps at 65536. */
};

/* INTERFACE SCAFFOLD ONLY: no ANT stack/channel is opened in this revision.
 * init: -EINVAL for NULL config / zero device number; otherwise -ENOTSUP.
 * send: -EINVAL for NULL data / incorrect page number; otherwise -ENOTSUP.
 * No measurement is queued, encoded or transmitted, and no counter is updated.
 * Future return 0 must mean accepted by the backend, not received by a display.
 */
int ant_app_init(const struct ant_app_channel_config *config);
void ant_app_process(void); /* Reserved event processing hook; currently a no-op. */
int ant_app_send_power_only(const struct ant_app_power_only_data *data);
int ant_app_send_crank_torque(const struct ant_app_crank_torque_data *data);

#ifdef __cplusplus
}
#endif

#endif
