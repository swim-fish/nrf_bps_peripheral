
#ifndef ST_BLE_BUTTON_STATE_H_
#define ST_BLE_BUTTON_STATE_H_

#include <zephyr/drivers/gpio.h>

#ifdef __cplusplus
extern "C" {
#endif


struct button_state
{
    bool is_adv_enable;
    bool is_bond;
};


int get_status(struct button_state* out_state);

int set_is_connected(bool enable);


#ifdef __cplusplus
}
#endif

#endif /* ST_BLE_BUTTON_STATE_H_ */
