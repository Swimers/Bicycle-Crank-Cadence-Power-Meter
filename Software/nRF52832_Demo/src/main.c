#include <zephyr/kernel.h>

#include "bluetooth_app.h"
#include "sc7a20h_app.h"

int main(void)
{
	if (bluetooth_app_init() != 0) {
		return 0;
	}
	sc7a20h_app_start();

	while (true) {
		sc7a20h_app_process();
		bluetooth_app_process();
		k_sleep(K_MSEC(20));
	}
}
