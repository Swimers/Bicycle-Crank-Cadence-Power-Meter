#ifndef CADENCE_BLUETOOTH_APP_H_
#define CADENCE_BLUETOOTH_APP_H_

/* Call once from thread context. Checks the NUS console, enables Bluetooth,
 * and schedules connectable advertising. Returns 0 or a negative errno.
 * Advertising failures are reported separately through printk.
 */
int bluetooth_app_init(void);

/* Call periodically after successful init. Prints connection events and
 * echoes received NUS text. May wait for BLE console TX buffer space;
 * call from the application thread, never from an ISR or BT RX callback.
 */
void bluetooth_app_process(void);

#endif
