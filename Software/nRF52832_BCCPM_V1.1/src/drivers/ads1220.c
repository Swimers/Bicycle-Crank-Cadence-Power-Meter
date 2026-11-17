#include "ads1220.h"

#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/kernel.h>

#define ADC_NODE DT_NODELABEL(ads1220)
#define CMD_RESET 0x06
#define CMD_START 0x08
#define CMD_WRITE_ALL 0x43
#define CMD_READ_ALL 0x23

static const struct spi_dt_spec adc = SPI_DT_SPEC_GET(ADC_NODE,
	SPI_WORD_SET(8) | SPI_TRANSFER_MSB | SPI_MODE_CPHA, 0);
static const struct gpio_dt_spec drdy = GPIO_DT_SPEC_GET(ADC_NODE, drdy_gpios);
static const struct gpio_dt_spec bridge = GPIO_DT_SPEC_GET(DT_NODELABEL(bccpm_io), bridge_en_gpios);
static bool initialized;
static bool pending;
static bool bridge_ready;
static bool bridge_enabled;

int ads1220_bridge_init(void)
{
	int ret;
	if (bridge_ready) { return 0; }
	if (!gpio_is_ready_dt(&bridge)) { return -ENODEV; }
	ret = gpio_pin_configure_dt(&bridge, GPIO_OUTPUT_ACTIVE);
	if (ret == 0) { bridge_ready = true; bridge_enabled = true; }
	return ret;
}

int ads1220_bridge_set(bool enabled)
{
	int ret;
	if (!bridge_ready) { return -EACCES; }
	if (pending) { return -EBUSY; }
	ret = gpio_pin_set_dt(&bridge, enabled ? 1 : 0);
	if (ret == 0) { bridge_enabled = enabled; }
	return ret;
}

static int command(uint8_t cmd)
{
	const struct spi_buf buf = { .buf = &cmd, .len = 1 };
	const struct spi_buf_set tx = { .buffers = &buf, .count = 1 };
	return spi_write_dt(&adc, &tx);
}

int ads1220_init(void)
{
	/* External REFP0, no IDAC or internal low-side switch. */
	uint8_t write_bytes[] = { CMD_WRITE_ALL, 0x08, 0x00, 0x40, 0x00 };
	uint8_t read_bytes[] = { CMD_READ_ALL, 0, 0, 0, 0 };
	uint8_t received[sizeof(read_bytes)] = { 0 };
	struct spi_buf write_buf = { .buf = write_bytes, .len = sizeof(write_bytes) };
	struct spi_buf read_buf = { .buf = read_bytes, .len = sizeof(read_bytes) };
	struct spi_buf rx_buf = { .buf = received, .len = sizeof(received) };
	struct spi_buf_set tx = { .buffers = &write_buf, .count = 1 };
	struct spi_buf_set rx = { .buffers = &rx_buf, .count = 1 };
	int ret;

	initialized = false;
	pending = false;
	ret = ads1220_bridge_init();
	if (ret < 0) { return ret; }
	if (!spi_is_ready_dt(&adc) || !gpio_is_ready_dt(&drdy)) {
		return -ENODEV;
	}
	ret = gpio_pin_configure_dt(&drdy, GPIO_INPUT);
	if (ret < 0) {
		return ret;
	}
	/* Power-on/reset recovery, including 50 us + 32 internal clock periods. */
	k_sleep(K_MSEC(1));
	ret = command(CMD_RESET);
	if (ret < 0) {
		return ret;
	}
	k_sleep(K_MSEC(1));
	ret = spi_write_dt(&adc, &tx);
	if (ret < 0) {
		return ret;
	}
	tx.buffers = &read_buf;
	ret = spi_transceive_dt(&adc, &tx, &rx);
	if (ret < 0) {
		return ret;
	}
	if (memcmp(&received[1], &write_bytes[1], 4) != 0) {
		return -EIO;
	}
	initialized = true;
	return 0;
}

int ads1220_start(void)
{
	int ret;

	if (!initialized) {
		return -EACCES;
	}
	if (!bridge_enabled) { return -EACCES; }
	if (pending) {
		return -EBUSY;
	}
	ret = command(CMD_START);
	if (ret == 0) {
		pending = true;
	}
	return ret;
}

int ads1220_read(int32_t *code)
{
	uint8_t zeros[3] = { 0 }, data[3];
	struct spi_buf tx_buf = { .buf = zeros, .len = sizeof(zeros) };
	struct spi_buf rx_buf = { .buf = data, .len = sizeof(data) };
	struct spi_buf_set tx = { .buffers = &tx_buf, .count = 1 };
	struct spi_buf_set rx = { .buffers = &rx_buf, .count = 1 };
	uint32_t raw;
	int ret;

	if (!code) {
		return -EINVAL;
	}
	if (!initialized || !pending) {
		return -EACCES;
	}
	ret = gpio_pin_get_dt(&drdy);
	if (ret < 0) {
		pending = false;
		return ret;
	}
	if (!ret) {
		return -EAGAIN;
	}
	ret = spi_transceive_dt(&adc, &tx, &rx);
	/* A failed transaction may have consumed data. Start anew on next call. */
	pending = false;
	if (ret < 0) {
		return ret;
	}
	raw = ((uint32_t)data[0] << 16) | ((uint32_t)data[1] << 8) | data[2];
	*code = (raw & 0x800000u) ? (int32_t)raw - 0x1000000 : (int32_t)raw;
	return 0;
}
