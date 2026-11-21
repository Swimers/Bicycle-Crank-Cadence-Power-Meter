#ifndef CADENCE_BLE_CSC_H_
#define CADENCE_BLE_CSC_H_

#include <stdint.h>

int ble_csc_start(void);

/* Record one confirmed crank revolution at the given uptime. */
void ble_csc_record_revolution(uint64_t time_ms);

#endif
