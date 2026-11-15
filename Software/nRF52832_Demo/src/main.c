#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "app/ant_app.h"
#include "app/bluetooth_app.h"
#include "app/sc7a20h_app.h"

int main(void)
{
	if (bluetooth_app_init() != 0) {
		return 0;
	}
	int ant_error = ant_app_init(&ant_app_default_channel_config);

	if (ant_error != 0) {
		printk("ANT interface scaffold: backend not started (%d)\n", ant_error);
	}
	sc7a20h_app_start();

	while (true) {
		sc7a20h_app_process();
		bluetooth_app_process();
		ant_app_process();
		k_sleep(K_MSEC(20));
	}
}
