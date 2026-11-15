#include "ota_entry.h"

#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/retention/bootmode.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/sys/printk.h>
#include <zephyr/app_version.h>

static void reboot_to_loader(struct k_work *work)
{
	ARG_UNUSED(work);
	int err = bootmode_set(BOOT_MODE_TYPE_BOOTLOADER);
	if (err != 0) {
		printk("OTA error: bootmode_set=%d\n", err);
		return;
	}
	sys_reboot(SYS_REBOOT_COLD);
}
K_WORK_DELAYABLE_DEFINE(ota_reboot_work, reboot_to_loader);

void ota_entry_feed(unsigned char c)
{
	static char line[32];
	static size_t used;
	static bool overflow;

	if (c == '\r' || c == '\n') {
		line[used] = '\0';
		if (!overflow && strcmp(line, "OTA ENTER") == 0) {
			printk("OTA: entering CadenceDFU in 500 ms\n");
			k_work_reschedule(&ota_reboot_work, K_MSEC(500));
		} else if (!overflow && strcmp(line, "VERSION") == 0) {
			printk("CadenceSensor firmware %s\n", APP_VERSION_STRING);
		}
		used = 0;
		overflow = false;
	} else if (used < sizeof(line) - 1) {
		line[used++] = (char)c;
	} else {
		overflow = true;
	}
}
