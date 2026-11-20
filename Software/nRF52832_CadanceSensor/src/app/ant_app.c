#include "ant_app.h"

#include <ant_host_init.h>
#include <ant_key_manager.h>
#include <ant_profiles/bsc/ant_bsc.h>
#include <ant_profiles/bsc/ant_bsc_utils.h>
#include <zephyr/kernel.h>

#define CHANNEL_NUMBER 0U
#define NETWORK_NUMBER 0U
#define TRANSMISSION_TYPE 0x05U
#define DEVICE_NUMBER 1U

static ant_bsc_profile_t profile;
static struct k_spinlock event_lock;
static ant_bsc_page0_data_t latest_event;

static void prepare_next_message(ant_bsc_profile_t *bsc, ant_bsc_evt_t event)
{
	ARG_UNUSED(event);

	/* The SDK has encoded this frame. Supply a coherent pair for the next one. */
	k_spinlock_key_t key = k_spin_lock(&event_lock);

	bsc->page_0 = latest_event;
	k_spin_unlock(&event_lock, key);
}

BSC_SENS_CHANNEL_CONFIG_DEF(cadence, CHANNEL_NUMBER, TRANSMISSION_TYPE,
			    BSC_CADENCE_DEVICE_TYPE, DEVICE_NUMBER, NETWORK_NUMBER);
BSC_SENS_PROFILE_CONFIG_DEF(cadence, false, false, ANT_BSC_PAGE_0, prepare_next_message);

static void ant_event(ant_evt_t *event)
{
	ant_bsc_sens_evt_handler(event, &profile);
}

int ant_app_init(void)
{
	int err = ant_init();

	if (err == 0) {
		err = ant_plus_key_set(NETWORK_NUMBER);
	}
	if (err == 0) {
		err = ant_bsc_sens_init(&profile, BSC_SENS_CHANNEL_CONFIG(cadence),
				       BSC_SENS_PROFILE_CONFIG(cadence));
	}
	if (err == 0) {
		err = ant_cb_register(ant_event);
	}
	if (err == 0) {
		err = ant_bsc_sens_open(&profile);
	}
	return err;
}

void ant_app_record_revolution(uint64_t time_ms)
{
	uint16_t event_ticks = (uint16_t)(time_ms * ANT_BSC_EVENT_TIME_COUNTS_PER_SEC / 1000U);
	k_spinlock_key_t key = k_spin_lock(&event_lock);

	latest_event.event_time = event_ticks;
	latest_event.rev_count++;
	k_spin_unlock(&event_lock, key);
}
