#include "ant_receiver.h"
#include "ant_decode.h"

#include <ant_host_init.h>
#include <ant_interface.h>
#include <ant_key_manager.h>
#include <ant_parameters.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/printk.h>
#include <string.h>

#define RX_CHANNEL 0U
#define ANT_NETWORK 0U
#define ANT_RF_FREQUENCY 57U
#define DEVICE_CAPACITY 24U
#define DEVICE_STALE_MS 60000U

struct frame {
	uint16_t id;
	uint8_t type;
	uint8_t transmission;
	uint8_t payload[8];
	uint32_t time_ms;
};

struct slot {
	bool used;
	uint16_t id;
	uint8_t type;
	uint8_t transmission;
	uint32_t last_seen;
	struct ant_decode_state decode;
};

K_MSGQ_DEFINE(frames, sizeof(struct frame), 32, 4);
static struct slot slots[DEVICE_CAPACITY];
static atomic_t queue_drops;
static uint32_t slot_drops;

static void on_ant_event(ant_evt_t *event)
{
	ANT_MESSAGE *msg = &event->message;

	if (event->channel != RX_CHANNEL || event->event != EVENT_RX ||
	    (msg->ANT_MESSAGE_ucMesgID != MESG_BROADCAST_DATA_ID &&
	     msg->ANT_MESSAGE_ucMesgID != MESG_ACKNOWLEDGED_DATA_ID) ||
	    msg->ANT_MESSAGE_ucSize < 14U ||
	    !msg->ANT_MESSAGE_stExtMesgBF.bANTDeviceID) {
		return;
	}
	const uint8_t *id = msg->ANT_MESSAGE_aucExtData;
	struct frame f = {
		.id = (uint16_t)id[0] | ((uint16_t)id[1] << 8),
		.type = id[2] & 0x7fU,
		.transmission = id[3],
		.time_ms = k_uptime_get_32(),
	};
	memcpy(f.payload, msg->ANT_MESSAGE_aucPayload, sizeof(f.payload));
	if (k_msgq_put(&frames, &f, K_NO_WAIT) != 0) {
		atomic_inc(&queue_drops);
	}
}

int ant_receiver_start(void)
{
	int err = ant_init();
	if (err) { return err; }
	err = ant_cb_register(on_ant_event);
	if (err) { return err; }
	err = ant_plus_key_set(ANT_NETWORK);
	if (err) { return err; }
	err = ant_lib_config_set(ANT_LIB_CONFIG_MESG_OUT_INC_DEVICE_ID);
	if (err) { return err; }
	err = ant_channel_assign(RX_CHANNEL, CHANNEL_TYPE_SLAVE, ANT_NETWORK,
				 EXT_PARAM_ALWAYS_SEARCH);
	if (err) { return err; }
	err = ant_channel_id_set(RX_CHANNEL, 0, 0, 0);
	if (err) { return err; }
	err = ant_channel_radio_freq_set(RX_CHANNEL, ANT_RF_FREQUENCY);
	if (err) { return err; }
	err = ant_channel_low_priority_rx_search_timeout_set(RX_CHANNEL, 0xff);
	if (err) { return err; }
	err = ant_channel_search_timeout_set(RX_CHANNEL, 0);
	if (err) { return err; }
	return ant_channel_open(RX_CHANNEL);
}

static struct slot *get_slot(const struct frame *f)
{
	struct slot *free_slot = NULL;

	for (size_t i = 0; i < ARRAY_SIZE(slots); ++i) {
		struct slot *s = &slots[i];
		if (s->used && s->id == f->id && s->type == f->type &&
		    s->transmission == f->transmission) {
			return s;
		}
		if ((!s->used || (uint32_t)(f->time_ms - s->last_seen) > DEVICE_STALE_MS) &&
		    free_slot == NULL) {
			free_slot = s;
		}
	}
	if (free_slot != NULL) {
		memset(free_slot, 0, sizeof(*free_slot));
		free_slot->used = true;
		free_slot->id = f->id;
		free_slot->type = f->type;
		free_slot->transmission = f->transmission;
	}
	return free_slot;
}

static void send_text(const struct device *uart, const char *text)
{
	while (*text != '\0') {
		uart_poll_out(uart, (unsigned char)*text++);
	}
}

void ant_receiver_process(const struct device *uart)
{
	struct frame f;
	if (k_msgq_get(&frames, &f, K_NO_WAIT) != 0) {
		return;
	}
	struct slot *slot = get_slot(&f);
	if (slot == NULL) {
		slot_drops++;
		return;
	}
	slot->last_seen = f.time_ms;
	struct ant_measurement m;
	bool supported = ant_decode_page(&slot->decode, f.type, f.payload, f.time_ms, &m);
	char hr[12] = "null", cadence[16] = "null", power[12] = "null";
	if (supported && m.has_heart_rate) { snprintk(hr, sizeof(hr), "%u", m.heart_rate_bpm); }
	if (supported && m.has_cadence) {
		snprintk(cadence, sizeof(cadence), "%u.%u", m.cadence_rpm_x10 / 10U,
			 m.cadence_rpm_x10 % 10U);
	}
	if (supported && m.has_power) { snprintk(power, sizeof(power), "%u", m.power_w); }
	char line[256];
	snprintk(line, sizeof(line),
		 "{\"v\":1,\"type\":\"measurement\",\"device_id\":%u,\"device_type\":%u,"
		 "\"transmission_type\":%u,\"uptime_ms\":%u,\"heart_rate_bpm\":%s,"
		 "\"cadence_rpm\":%s,\"power_w\":%s,\"raw\":\"%02x%02x%02x%02x%02x%02x%02x%02x\","
		 "\"queue_drops\":%u,\"slot_drops\":%u}\n",
		 f.id, f.type, f.transmission, f.time_ms, hr, cadence, power,
		 f.payload[0], f.payload[1], f.payload[2], f.payload[3], f.payload[4],
		 f.payload[5], f.payload[6], f.payload[7],
		 (unsigned int)atomic_get(&queue_drops), slot_drops);
	send_text(uart, line);
}
