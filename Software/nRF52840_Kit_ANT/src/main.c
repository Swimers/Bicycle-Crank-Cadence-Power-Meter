#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/reboot.h>
#include <hal/nrf_power.h>
#include <string.h>

#if defined(CONFIG_ANT)
#include "ant_receiver.h"
#include <zephyr/sys/printk.h>
#endif

/* Adafruit nRF52 bootloader v0.6.0: enter USB MSC + CDC UF2 mode. */
#define ADAFRUIT_UF2_MAGIC 0x57U
#define COMMAND_CAPACITY 32U

static void reply(const struct device *uart, const char *message)
{
	while (*message != '\0') {
		uart_poll_out(uart, (unsigned char)*message++);
	}
}

int main(void)
{
	const struct device *const uart = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
	char command[COMMAND_CAPACITY];
	size_t len = 0;
	bool invalid = false;
	unsigned char byte;

	if (!device_is_ready(uart)) {
		return 0;
	}

	/* Zephyr 4.2 CDC ACM queues its first OUT transfer on RX enable.
	 * uart_poll_in() alone does not arm the endpoint on that version.
	 */
#if defined(CONFIG_UART_INTERRUPT_DRIVEN)
	uart_irq_rx_enable(uart);
#endif

#if defined(CONFIG_ANT)
	int ant_err = ant_receiver_start();
	char ant_status[96];
	snprintk(ant_status, sizeof(ant_status),
		 "{\"v\":1,\"type\":\"status\",\"ant_start_error\":%d}\n", ant_err);
	reply(uart, ant_status);
#endif

	for (;;) {
#if defined(CONFIG_ANT)
		ant_receiver_process(uart);
#endif
		if (uart_poll_in(uart, &byte) != 0) {
			k_sleep(K_MSEC(10));
			continue;
		}

		if (byte == '\n') {
			/* Accept LF and CRLF, but not embedded CR or NUL. */
			if (len > 0 && command[len - 1] == '\r') {
				len--;
			}
			command[len] = '\0';
			if (!invalid && strcmp(command, "PING") == 0) {
				reply(uart, "PONG\r\n");
#if defined(CONFIG_ANT)
			} else if (!invalid && strcmp(command, "STATUS") == 0) {
				reply(uart, ant_status);
#endif
			} else if (!invalid && strcmp(command, "BOOTLOADER") == 0) {
				reply(uart, "OK BOOTLOADER\r\n");
				/* Best effort only: CDC does not confirm host receipt. */
				k_sleep(K_MSEC(300));
				NRF_POWER->GPREGRET = ADAFRUIT_UF2_MAGIC;
				sys_reboot(SYS_REBOOT_COLD);
			} else if (invalid || len != 0) {
				reply(uart, "ERR COMMAND\r\n");
			}
			len = 0;
			invalid = false;
		} else if (byte == '\0' || (len > 0 && command[len - 1] == '\r')) {
			invalid = true;
		} else if (len < sizeof(command) - 1) {
			command[len++] = (char)byte;
		} else {
			/* Discard the entire overlong line until its newline. */
			invalid = true;
		}
	}
}
