/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>

#define MODULE button_state
#include <caf/events/button_event.h>
#include <caf/events/led_event.h>
#include <caf/events/module_state_event.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(MODULE, CONFIG_CAF_SAMPLE_BUTTON_STATE_LOG_LEVEL);

#include "led_state_def.h"
#include "modules/button_state.h"

#include <inttypes.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

#include <caf/events/click_event.h>

#define BLINKYTHREAD_PRIORITY 3
#define STACKSIZE 256

#define BLINKY_SLEEP_FAST 100
#define BLINKY_SLEEP_SLOW 200

volatile bool adv_enable = true;  // true = forward, false = reverse
volatile bool is_bond = false;    // true = fast, false = slow.
volatile bool is_connected = false;
volatile bool is_reset = false;
volatile bool led2_red_on = false;
volatile bool led2_blue_on = false;
volatile bool led2_green_on = false;

static uint8_t cnt = 0;  // led position

static struct gpio_dt_spec led1 =
    GPIO_DT_SPEC_GET_OR(DT_ALIAS(led0_green), gpios, {0});

static struct gpio_dt_spec led2_red =
    GPIO_DT_SPEC_GET_OR(DT_ALIAS(led1_red), gpios, {0});

static struct gpio_dt_spec led2_blue =
    GPIO_DT_SPEC_GET_OR(DT_ALIAS(led1_blue), gpios, {0});

static struct gpio_dt_spec led2_green =
    GPIO_DT_SPEC_GET_OR(DT_ALIAS(led1_green), gpios, {0});

enum button_id {
  BUTTON_ID_NEXT_EFFECT,
  BUTTON_ID_NEXT_LED,

  BUTTON_ID_COUNT
};

int get_status(struct button_state* out_state) {
  out_state->is_adv_enable = adv_enable;
  out_state->is_bond = is_bond;

  return 0;
}

int set_is_connected(bool enable) {
  is_connected = enable;

  return 0;
}

static bool handle_click_event(const struct click_event* evt) {
  // LOG_INF("CLICK HANDLER %d", evt->key_id);
  if (evt->key_id == 0x00) {
    switch (evt->click) {
      case CLICK_SHORT: {
        if (adv_enable) {
          LOG_INF("Disable adv");
        } else {
          LOG_INF("Enable adv");
        }
        adv_enable = !adv_enable;
      } break;
      case CLICK_LONG: {
        LOG_INF("Disable adv and bond (reset)");
        adv_enable = false;
        is_bond = false;
        is_reset = true;
      } break;
      case CLICK_DOUBLE: {
        if (is_bond) {
          LOG_INF("Disable bond");
        } else {
          LOG_INF("Enable bond");
        }
        led2_blue_on = true;
        is_bond = !is_bond;
      } break;
      default:
        break;
    }
  }

  return false;
}

static bool app_event_handler(const struct app_event_header* aeh) {
  LOG_INF("EVENT HANDLER");
  if (is_click_event(aeh)) {
    return handle_click_event(cast_click_event(aeh));
  }

  if (is_module_state_event(aeh)) {
    const struct module_state_event* event = cast_module_state_event(aeh);

    if (check_state(event, MODULE_ID(leds), MODULE_STATE_READY)) {
      LOG_INF("Ready steady");
      printk("ready steady\n");
    }

    return false;
  }

  /* Event not handled but subscribed. */
  __ASSERT_NO_MSG(false);

  return false;
}

void blinkythread(void) {
  uint32_t time_point_led1 = k_cycle_get_32();
  /* If we have an LED, match its state to the button's. */
  while (1) {
    uint32_t time_point = k_cycle_get_32();
    switch (cnt & 0x1) {
      case 0: {
        if (adv_enable && (time_point - time_point_led1 > 10000)) {
          gpio_pin_toggle_dt(&led1);
          if (is_bond && !is_connected) {
            gpio_pin_toggle_dt(&led2_blue);
          }
          time_point_led1 = k_cycle_get_32();
        } else if (time_point - time_point_led1 > 50000) {
          gpio_pin_toggle_dt(&led1);
          if (is_bond && !is_connected) {
            gpio_pin_toggle_dt(&led2_blue);
          }
          time_point_led1 = k_cycle_get_32();
        }
        if (adv_enable && !is_connected && !led2_blue_on && !is_bond) {
          led2_blue_on = true;
          gpio_pin_set_dt(&led2_blue, 1);
        } else if (!adv_enable && !is_bond && led2_blue_on) {
          led2_blue_on = false;
          gpio_pin_set_dt(&led2_blue, 0);
        }
      } break;
      case 1: {
        if (is_connected && !led2_red_on) {
          led2_red_on = true;
          gpio_pin_set_dt(&led2_blue, 0);
          gpio_pin_set_dt(&led2_red, 1);

        } else if (!is_connected && led2_red_on) {
          led2_red_on = false;
          gpio_pin_set_dt(&led2_red, 0);
        }

        if (is_reset) {
          is_reset = false;
          gpio_pin_set_dt(&led1, 1);
          gpio_pin_set_dt(&led2_red, 1);
          gpio_pin_set_dt(&led2_blue, 1);
          gpio_pin_set_dt(&led2_green, 1);
          k_msleep(2000);
          gpio_pin_set_dt(&led2_red, 0);
          gpio_pin_set_dt(&led2_blue, 0);
          gpio_pin_set_dt(&led2_green, 0);
        }
      } break;
    }

    cnt = (cnt + 1) & 0x1;
    k_msleep(BLINKY_SLEEP_FAST);
  }
}

APP_EVENT_LISTENER(MODULE, app_event_handler);
APP_EVENT_SUBSCRIBE(MODULE, module_state_event);
APP_EVENT_SUBSCRIBE(MODULE, click_event);
K_THREAD_DEFINE(blinkythread_id, STACKSIZE, blinkythread, NULL, NULL, NULL,
                BLINKYTHREAD_PRIORITY, 0, 0);
