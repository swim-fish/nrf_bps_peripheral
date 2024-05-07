
#ifndef ST_BLE_BUTTON_STATE_H_
#define ST_BLE_BUTTON_STATE_H_

#include <zephyr/drivers/gpio.h>

#ifdef __cplusplus
extern "C" {
#endif

enum device_status_enum {
  ADV_ENABLE,
  ADV_IS_ENABLED,
  BONDED,
  IS_BONDED,
  CONNECTED,
  RESET,
};

struct device_status {
  atomic_t status_bits;
};

struct device_status* get_status(void);

#ifdef __cplusplus
}
#endif

#endif /* ST_BLE_BUTTON_STATE_H_ */
