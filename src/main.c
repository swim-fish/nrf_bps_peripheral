#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/printk.h>
#include <zephyr/types.h>

#include <zephyr/settings/settings.h>

#include <app_event_manager.h>
#include <dk_buttons_and_leds.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/hci_vs.h>
#include <zephyr/bluetooth/services/bas.h>
#include <zephyr/bluetooth/services/hrs.h>
#include <zephyr/bluetooth/services/ias.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/drivers/bluetooth/hci_driver.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/reboot.h>

#include "modules/button_state.h"

#define MODULE main
#include <caf/events/module_state_event.h>

LOG_MODULE_REGISTER(MODULE);

#define RUN_STATUS_LED DK_LED1
#define CENTRAL_CON_STATUS_LED DK_LED2
#define PERIPHERAL_CONN_STATUS_LED DK_LED3

static struct bt_uuid_16 bps_uuid = BT_UUID_INIT_16(BT_UUID_BPS_VAL);

static struct bt_uuid_16 bpm_uuid = BT_UUID_INIT_16(BT_UUID_GATT_BPM_VAL);

#define VND_MAX_LEN 21

/* BLE connection */
struct bt_conn* ble_conn;

static size_t send_count = 0;
static uint8_t simulate_vnd;
volatile bool is_bonded = false;
volatile bool is_adved = false;
static bt_addr_le_t bond_addr;
static struct bt_le_adv_param adv_param;

static void notify_bps(void);

static uint8_t vnd_value[VND_MAX_LEN] = {
    // 1e 80 00 5c 00 68 00 e8 07 04 12 12 28 00 60 00
    // 01 00 00 00 00
    0x1e, 0x80, 0x00, 0x5c, 0x00, 0x68, 0x00, 0xe8, 0x07, 0x04, 0x12,
    0x12, 0x28, 0x00, 0x60, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00};

static void vnd_ccc_cfg_changed(const struct bt_gatt_attr* attr,
                                uint16_t value) {
  ARG_UNUSED(attr);
  simulate_vnd = (value == BT_GATT_CCC_NOTIFY) ? 1 : 0;
  printk("Notification %s\n", simulate_vnd ? "enabled" : "disabled");

  if (is_bonded && simulate_vnd) {
    notify_bps();
  }
}

/* Vendor Primary Service Declaration */
BT_GATT_SERVICE_DEFINE(
    bps_svc, BT_GATT_PRIMARY_SERVICE(&bps_uuid),
    BT_GATT_CHARACTERISTIC(&bpm_uuid.uuid,
                           BT_GATT_CHRC_READ | BT_GATT_CHRC_INDICATE |
                               BT_GATT_CHRC_NOTIFY,
                           BT_GATT_PERM_READ, NULL, NULL, vnd_value),
    BT_GATT_CCC(vnd_ccc_cfg_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE), );

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA_BYTES(BT_DATA_UUID16_ALL, BT_UUID_16_ENCODE(BT_UUID_BPS_VAL),
                  BT_UUID_16_ENCODE(BT_UUID_BAS_VAL),
                  BT_UUID_16_ENCODE(BT_UUID_CTS_VAL)),
};

static void notify_bps(void) {
  printk("Sending notification\n");
  struct bt_gatt_attr* notify_ccc =
      bt_gatt_find_by_uuid(bps_svc.attrs, bps_svc.attr_count, &bpm_uuid.uuid);
  bt_gatt_notify(NULL, notify_ccc, &vnd_value, sizeof(vnd_value));
  send_count++;
  printk("Notification sent count: %d\n", send_count);
}

void mtu_updated(struct bt_conn* conn, uint16_t tx, uint16_t rx) {
  printk("Updated MTU: TX: %d RX: %d bytes\n", tx, rx);
}

static struct bt_gatt_cb gatt_callbacks = {.att_mtu_updated = mtu_updated};

static void connected(struct bt_conn* conn, uint8_t err) {
  if (err) {
    printk("Connection failed (err 0x%02x)\n", err);
  } else {
    // show connection source mac address
    char addr[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
    printk("Connected %s\n", addr);

    set_is_connected(true);

    if (!ble_conn) {
      ble_conn = bt_conn_ref(conn);
    }
    // auth requested by peer
    if (bt_conn_set_security(conn, BT_SECURITY_L2)) {
      printk("Failed to set security\n");
    }
  }
}

static void disconnected(struct bt_conn* conn, uint8_t reason) {
  printk("Disconnected (reason 0x%02x)\n", reason);
  // dk_set_led_off(CENTRAL_CON_STATUS_LED);
  if (ble_conn) {
    bt_conn_unref(ble_conn);
    ble_conn = NULL;
  }
  set_is_connected(false);
}

static void alert_stop(void) {
  printk("Alert stopped\n");
}

static void alert_start(void) {
  printk("Mild alert started\n");
}

static void alert_high_start(void) {
  printk("High alert started\n");
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
};

BT_IAS_CB_DEFINE(ias_callbacks) = {
    .no_alert = alert_stop,
    .mild_alert = alert_start,
    .high_alert = alert_high_start,
};

static void copy_last_bonded_addr(const struct bt_bond_info* info, void* data) {
  bt_addr_le_copy(&bond_addr, &info->addr);
}

static void bt_ready(void) {
  int err;
  bt_addr_le_t addr_le = {0};
  char addr[BT_ADDR_LE_STR_LEN];
  size_t count = 1;

  printk("Bluetooth initialized\n");

  // Load settings: includes bonded devices
  if (IS_ENABLED(CONFIG_SETTINGS)) {
    settings_load();
  }

  bt_addr_le_copy(&bond_addr, BT_ADDR_LE_NONE);
  bt_foreach_bond(BT_ID_DEFAULT, copy_last_bonded_addr, NULL);

  /* Address is equal to BT_ADDR_LE_NONE if compare returns 0.
	 * This means there is no bond yet.
	 */
  if (bt_addr_le_cmp(&bond_addr, BT_ADDR_LE_NONE) != 0) {
    bt_addr_le_to_str(&bond_addr, addr, sizeof(addr));
    printk("Bonded by %s\n", addr);

    adv_param = *BT_LE_ADV_CONN_DIR_LOW_DUTY(&bond_addr);
    adv_param.options |= BT_LE_ADV_OPT_DIR_ADDR_RPA;
    // err = bt_le_adv_start(&adv_param, NULL, 0, NULL, 0);
    is_bonded = true;
    is_adved = true;
    err = bt_le_adv_start(BT_LE_ADV_CONN_NAME, ad, ARRAY_SIZE(ad), NULL, 0);
  } else {
    is_adved = true;
    err = bt_le_adv_start(BT_LE_ADV_CONN_NAME, ad, ARRAY_SIZE(ad), NULL, 0);
  }

  // Get the Bluetooth device address
  bt_id_get(&addr_le, &count);
  bt_addr_le_to_str(&addr_le, addr, sizeof(addr));
  printk("Bluetooth Device Address: %s\n", addr);
  struct bt_le_oob oob;
  if (bt_le_oob_get_local(BT_ID_DEFAULT, &oob) == 0) {
    addr_le = oob.addr;
    bt_addr_le_to_str(&addr_le, addr, sizeof(addr));
    printk("OOB advertising as %s\n", addr);
  }

  if (err) {
    printk("Advertising failed to start (err %d)\n", err);
  } else {
    printk("Advertising successfully started\n");
  }
}

void pairing_complete(struct bt_conn* conn, bool bonded) {
  printk("Pairing complete\n");
  is_bonded = true;
  bt_addr_le_copy(&bond_addr, bt_conn_get_dst(conn));

  // printk("Pairing completed. Rebooting in 3 seconds...\n");
  // k_sleep(K_SECONDS(3));
  // sys_reboot(SYS_REBOOT_WARM);
}

static struct bt_conn_auth_info_cb bt_conn_auth_info = {.pairing_complete =
                                                            pairing_complete};

int main(void) {

  struct bt_gatt_attr* vnd_ind_attr;
  char str[BT_UUID_STR_LEN];
  int err;

  if (app_event_manager_init()) {
    printk("Application Event Manager not initialized\n");
  } else {
    module_set_state(MODULE_STATE_READY);
  }

  err = bt_enable(NULL);
  if (err) {
    // print error no and text
    printk("Bluetooth init failed (err %d - %s)\n", err, strerror(-err));
    return 0;
  }

  bt_ready();

  bt_conn_auth_info_cb_register(&bt_conn_auth_info);

  bt_gatt_cb_register(&gatt_callbacks);

  vnd_ind_attr =
      bt_gatt_find_by_uuid(bps_svc.attrs, bps_svc.attr_count, &bpm_uuid.uuid);
  bt_uuid_to_str(&bpm_uuid.uuid, str, sizeof(str));
  printk("Indicate BPS attr %p (UUID %s)\n", vnd_ind_attr, str);

  while (1) {
    k_sleep(K_MSEC(10));

    struct button_state state;
    if (get_status(&state) == 0) {
      if (state.is_adv_enable && !is_adved) {
        LOG_INF("Starting advertising");
        is_adved = true;
        err = bt_le_adv_start(BT_LE_ADV_CONN_NAME, ad, ARRAY_SIZE(ad), NULL, 0);
      } else if (!state.is_adv_enable && is_adved) {
        LOG_INF("Stopping advertising");
        is_adved = false;
        err = bt_le_adv_stop();
      }

      if (state.is_bond && !is_bonded) {
        LOG_INF("Set bonding to true");
        is_bonded = true;
      } else if (!state.is_bond && is_bonded) {
        LOG_INF("Set bonding to false and call unpair");
        is_bonded = false;
        err = bt_unpair(BT_ID_DEFAULT, BT_ADDR_LE_ANY);
      }
    }
  }

  return 0;
}
