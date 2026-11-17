#ifndef BCCPM_ADS1220_H_
#define BCCPM_ADS1220_H_

#include <stdint.h>
#include <stdbool.h>

/* Fixed AIN0-AIN1, gain 16, external REFP0/REFN0, 20 SPS single-shot.
 * Calls must be serialized and made from thread context.
 */
int ads1220_init(void);
/* Bridge GPIO defaults on. Do not switch during a pending conversion. */
int ads1220_bridge_init(void);
int ads1220_bridge_set(bool enabled);
int ads1220_start(void);
/* Returns -EAGAIN until the current conversion is ready. */
int ads1220_read(int32_t *code);

#endif
