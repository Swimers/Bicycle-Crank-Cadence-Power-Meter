#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/sys/printk.h>
#include <errno.h>

static const struct i2c_dt_spec acc = I2C_DT_SPEC_GET(DT_NODELABEL(sc7a20h));
static const struct spi_dt_spec ads = SPI_DT_SPEC_GET(DT_NODELABEL(ads1220),
                                                    SPI_WORD_SET(8) | SPI_TRANSFER_MSB);
static const struct gpio_dt_spec inputs[] = {
    GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios),
    GPIO_DT_SPEC_GET(DT_NODELABEL(sc7a20h), int1_gpios),
    GPIO_DT_SPEC_GET(DT_NODELABEL(ads1220), drdy_gpios),
};

static int board_init(void)
{
    int err;
    for (size_t i = 0; i < ARRAY_SIZE(inputs); ++i) {
        if (!gpio_is_ready_dt(&inputs[i])) { return -ENODEV; }
        err = gpio_pin_configure_dt(&inputs[i], GPIO_INPUT);
        if (err) { return err; }
        err = gpio_pin_interrupt_configure_dt(&inputs[i], GPIO_INT_DISABLE);
        if (err) { return err; }
    }
    if (!i2c_is_ready_dt(&acc) || !spi_is_ready_dt(&ads)) { return -ENODEV; }
    /* Explicitly deassert software CS before the first transaction. */
    return gpio_pin_configure_dt(&ads.config.cs.gpio, GPIO_OUTPUT_INACTIVE);
}

int main(void)
{
    int err = board_init();
    printk("Board: %s; init: %d\n", CONFIG_BOARD_TARGET, err);
    if (err) { return err; }
    printk("I2C0: addr=0x%02x; SPI1: %u Hz, mode 1; GPIO inputs ready\n",
           acc.addr, ads.config.frequency);
    printk("Controller initialization only; sensor communication not yet verified.\n");
    /* Sensor configuration and acquisition are implemented separately. */
    k_sleep(K_FOREVER);
    return 0;
}
