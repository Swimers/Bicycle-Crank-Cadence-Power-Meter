#include "ant_decode.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
	struct ant_decode_state a = {0}, b = {0};
	struct ant_measurement out;
	uint8_t hr[8] = {0x80, 0, 0, 0, 0, 0, 1, 72};
	assert(ant_decode_page(&a, 120, hr, 0, &out));
	assert(out.has_heart_rate && out.heart_rate_bpm == 72);
	hr[7] = 255;
	assert(ant_decode_page(&a, 120, hr, 0, &out) && !out.has_heart_rate);

	uint8_t power[8] = {0x10, 1, 0xff, 88, 0, 0, 0xfa, 0};
	assert(ant_decode_page(&a, 11, power, 0, &out));
	assert(out.has_power && out.power_w == 250);
	assert(out.has_cadence && out.cadence_rpm_x10 == 880);
	power[0] = 0x50;
	assert(!ant_decode_page(&a, 11, power, 0, &out));

	uint8_t cadence[8] = {0, 0, 0, 0, 0xe8, 0xfd, 0xff, 0xff};
	assert(ant_decode_page(&a, 122, cadence, 100, &out) && !out.has_cadence);
	assert(ant_decode_page(&b, 122, cadence, 100, &out) && !out.has_cadence);
	/* 65000 -> 488 ticks and 65535 -> 0 revolutions: 60 rpm. */
	cadence[4] = 0xe8; cadence[5] = 0x01; cadence[6] = 0; cadence[7] = 0;
	assert(ant_decode_page(&a, 122, cadence, 1100, &out));
	assert(out.has_cadence && out.cadence_rpm_x10 == 600);
	assert(ant_decode_page(&a, 122, cadence, 2000, &out) && !out.has_cadence);
	assert(ant_decode_page(&a, 122, cadence, 4100, &out));
	assert(out.has_cadence && out.cadence_rpm_x10 == 0);
	/* Independent device state and old baseline expiry do not fabricate rpm. */
	assert(ant_decode_page(&b, 122, cadence, 60100, &out) && !out.has_cadence);
	assert(!ant_decode_page(NULL, 122, cadence, 0, &out));
	puts("ANT decoder tests passed");
	return 0;
}
