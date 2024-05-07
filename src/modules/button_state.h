
#ifndef ST_BLE_BUTTON_STATE_H_
#define ST_BLE_BUTTON_STATE_H_

#include <zephyr/drivers/gpio.h>

#ifdef __cplusplus
extern "C" {
#endif

enum device_status_enum {
  // ADV control flags for button
  ADV_ENABLE,
  // ADV is running or not
  ADV_IS_ENABLED,

  // Bonded control flags for button
  BONDED,
  // Device is bonded or not
  IS_BONDED,

  // Connected control flags for LED
  CONNECTED,
  // Reset control flags for button
  RESET,
};

struct device_status {
  atomic_t status_bits;
};

// Singleton instance of device status, used to control LED and receive button events
struct device_status* get_status(void);

#ifdef __cplusplus
}
#endif

#endif /* ST_BLE_BUTTON_STATE_H_ */
