#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "app/ant_app.h"
#include "app/bluetooth_app.h"
#include "app/sc7a20h_app.h"

int main(void)
{
	/* Official multiprotocol example initializes ANT before enabling BLE. */
	int ant_error = ant_app_init(&ant_app_default_channel_config);

	if (bluetooth_app_init() != 0) {
		return 0;
	}
	if (ant_error != 0) {
		printk("ANT initialization failed / unavailable (%d); BLE remains active\n", ant_error);
	} else {
		printk("ANT Bicycle Power started: fixed %u W, %u rpm; BLE active\n",
		       ANT_APP_FIXED_POWER_WATTS, ANT_APP_FIXED_CADENCE_RPM);
	}
	sc7a20h_app_start();

	while (true) {
		sc7a20h_app_process();
		bluetooth_app_process();
		ant_app_process();
		k_sleep(K_MSEC(20));
	}
}
