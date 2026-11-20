#ifndef CADENCE_SC7A20H_APP_H_
#define CADENCE_SC7A20H_APP_H_

/* Call once after the BLE stack starts. Sampling runs in its own thread. */
void sc7a20h_app_start(void);

/* Call from main: prints latest data at most 10 times/second. */
void sc7a20h_app_process(void);

#endif
