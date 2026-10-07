#ifndef BLE_BATTERY_H
#define BLE_BATTERY_H

#include <stdint.h>

#include "esp_err.h"

/** Initialize the BLE peripheral and start advertising after host sync. */
esp_err_t ble_battery_init(void);

/** Update the Battery Level characteristic and notify a subscribed client. */
void ble_battery_set_level(uint8_t percent);

#endif
