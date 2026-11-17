#ifndef BCCPM_BOARD_IO_H_
#define BCCPM_BOARD_IO_H_

#include <stdbool.h>
#include <stdint.h>

/* Thread context, serialized calls. Initialize before IMU/bridge ADC modules:
 * enables the bridge, turns the buzzer off and initializes power. */
int board_io_init(void);
/* Sample twice 20 ms apart. Returns -EAGAIN on a changing input. */
int board_io_button_pressed(bool *pressed);
/* 4 kHz, 50% duty when on; off drives the PWM output inactive. */
int board_io_buzzer_set(bool on);
/* Set frequency in Hz and duty 0..100%; hardware may reject unsupported periods. */
int board_io_buzzer_configure(uint32_t frequency_hz, uint8_t duty_percent);

#endif
