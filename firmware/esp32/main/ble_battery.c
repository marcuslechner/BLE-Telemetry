#include "ble_battery.h"

#include <stdbool.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "host/util/util.h"
#include "nimble/ble.h"
#include "nimble/nimble_port.h"
#include "nvs_flash.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#define BLE_DEVICE_NAME "BLE-Telemetry"
#define BATTERY_SERVICE_UUID 0x180F
#define BATTERY_LEVEL_UUID 0x2A19

static const char *TAG = "ble_battery";
static const ble_uuid16_t s_battery_service_uuid =
    BLE_UUID16_INIT(BATTERY_SERVICE_UUID);
static const ble_uuid16_t s_battery_level_uuid =
    BLE_UUID16_INIT(BATTERY_LEVEL_UUID);

static portMUX_TYPE s_state_lock = portMUX_INITIALIZER_UNLOCKED;
static uint8_t s_battery_level;
static uint16_t s_battery_level_handle;
static uint16_t s_subscriber_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static bool s_notify_enabled;
static uint8_t s_own_addr_type;

static void start_advertising(void);

static uint8_t battery_level_get(void)
{
    uint8_t level;

    portENTER_CRITICAL(&s_state_lock);
    level = s_battery_level;
    portEXIT_CRITICAL(&s_state_lock);
    return level;
}

static int battery_level_access(uint16_t conn_handle,
                                uint16_t attr_handle,
                                struct ble_gatt_access_ctxt *ctxt,
                                void *argument)
{
    (void)conn_handle;
    (void)argument;

    if ((ctxt->op != BLE_GATT_ACCESS_OP_READ_CHR)
        || (attr_handle != s_battery_level_handle)) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    const uint8_t level = battery_level_get();
    return os_mbuf_append(ctxt->om, &level, sizeof(level)) == 0
               ? 0
               : BLE_ATT_ERR_INSUFFICIENT_RES;
}

static const struct ble_gatt_svc_def s_gatt_services[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &s_battery_service_uuid.u,
        .characteristics =
            (struct ble_gatt_chr_def[]){
                {
                    .uuid = &s_battery_level_uuid.u,
                    .access_cb = battery_level_access,
                    .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                    .val_handle = &s_battery_level_handle,
                },
                {0},
            },
    },
    {0},
};

static void subscription_reset(uint16_t conn_handle)
{
    portENTER_CRITICAL(&s_state_lock);
    if ((conn_handle == BLE_HS_CONN_HANDLE_NONE)
        || (s_subscriber_conn_handle == conn_handle)) {
        s_subscriber_conn_handle = BLE_HS_CONN_HANDLE_NONE;
        s_notify_enabled = false;
    }
    portEXIT_CRITICAL(&s_state_lock);
}

static int gap_event_handler(struct ble_gap_event *event, void *argument)
{
    (void)argument;

    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            ESP_LOGI(TAG,
                     "phone connected; handle=%u",
                     event->connect.conn_handle);
        } else {
            ESP_LOGW(TAG,
                     "connection failed; status=%d",
                     event->connect.status);
            start_advertising();
        }
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG,
                 "phone disconnected; reason=%d",
                 event->disconnect.reason);
        subscription_reset(event->disconnect.conn.conn_handle);
        start_advertising();
        return 0;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        ESP_LOGI(TAG,
                 "advertising completed; reason=%d; restarting",
                 event->adv_complete.reason);
        start_advertising();
        return 0;

    case BLE_GAP_EVENT_SUBSCRIBE:
        if (event->subscribe.attr_handle == s_battery_level_handle) {
            portENTER_CRITICAL(&s_state_lock);
            s_subscriber_conn_handle = event->subscribe.cur_notify
                                           ? event->subscribe.conn_handle
                                           : BLE_HS_CONN_HANDLE_NONE;
            s_notify_enabled = event->subscribe.cur_notify != 0;
            portEXIT_CRITICAL(&s_state_lock);
            ESP_LOGI(TAG,
                     "battery notifications %s; handle=%u",
                     event->subscribe.cur_notify ? "enabled" : "disabled",
                     event->subscribe.conn_handle);
        }
        return 0;

    case BLE_GAP_EVENT_MTU:
        ESP_LOGI(TAG,
                 "MTU updated; handle=%u mtu=%u",
                 event->mtu.conn_handle,
                 event->mtu.value);
        return 0;

    case BLE_GAP_EVENT_NOTIFY_TX:
        if ((event->notify_tx.status != 0)
            && (event->notify_tx.status != BLE_HS_EDONE)) {
            ESP_LOGW(TAG,
                     "battery notification failed; status=%d",
                     event->notify_tx.status);
        }
        return 0;

    default:
        return 0;
    }
}

static void start_advertising(void)
{
    const char *name = ble_svc_gap_device_name();
    struct ble_hs_adv_fields fields = {
        .flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP,
        .name = (uint8_t *)name,
        .name_len = strlen(name),
        .name_is_complete = 1,
        .uuids16 = (ble_uuid16_t *)&s_battery_service_uuid,
        .num_uuids16 = 1,
        .uuids16_is_complete = 1,
    };
    struct ble_gap_adv_params params = {
        .conn_mode = BLE_GAP_CONN_MODE_UND,
        .disc_mode = BLE_GAP_DISC_MODE_GEN,
    };

    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to set advertising data; rc=%d", rc);
        return;
    }

    rc = ble_gap_adv_start(s_own_addr_type,
                           NULL,
                           BLE_HS_FOREVER,
                           &params,
                           gap_event_handler,
                           NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to start advertising; rc=%d", rc);
        return;
    }

    ESP_LOGI(TAG,
             "advertising as %s with Battery Service 0x%04X",
             BLE_DEVICE_NAME,
             BATTERY_SERVICE_UUID);
}

static void on_stack_reset(int reason)
{
    subscription_reset(BLE_HS_CONN_HANDLE_NONE);
    ESP_LOGW(TAG, "NimBLE stack reset; reason=%d", reason);
}

static void on_stack_sync(void)
{
    int rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        ESP_LOGE(TAG, "no usable BLE address; rc=%d", rc);
        return;
    }

    rc = ble_hs_id_infer_auto(0, &s_own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to select BLE address type; rc=%d", rc);
        return;
    }

    start_advertising();
}

static void nimble_host_task(void *argument)
{
    (void)argument;
    ESP_LOGI(TAG, "NimBLE host started");
    nimble_port_run();
    vTaskDelete(NULL);
}

esp_err_t ble_battery_init(void)
{
    esp_err_t error = nvs_flash_init();
    if ((error == ESP_ERR_NVS_NO_FREE_PAGES)
        || (error == ESP_ERR_NVS_NEW_VERSION_FOUND)) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        error = nvs_flash_init();
    }
    if (error != ESP_OK) {
        return error;
    }

    error = nimble_port_init();
    if (error != ESP_OK) {
        return error;
    }

    ble_svc_gap_init();
    ble_svc_gatt_init();

    int rc = ble_svc_gap_device_name_set(BLE_DEVICE_NAME);
    if (rc == 0) {
        rc = ble_gatts_count_cfg(s_gatt_services);
    }
    if (rc == 0) {
        rc = ble_gatts_add_svcs(s_gatt_services);
    }
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to configure GATT server; rc=%d", rc);
        nimble_port_deinit();
        return ESP_FAIL;
    }

    ble_hs_cfg.reset_cb = on_stack_reset;
    ble_hs_cfg.sync_cb = on_stack_sync;

    if (xTaskCreate(nimble_host_task,
                    "nimble_host",
                    4096,
                    NULL,
                    5,
                    NULL)
        != pdPASS) {
        nimble_port_deinit();
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

void ble_battery_set_level(uint8_t percent)
{
    if (percent > 100U) {
        ESP_LOGW(TAG, "ignored invalid battery level: %u", percent);
        return;
    }

    uint16_t conn_handle;
    bool notify;
    bool changed;

    portENTER_CRITICAL(&s_state_lock);
    changed = s_battery_level != percent;
    s_battery_level = percent;
    conn_handle = s_subscriber_conn_handle;
    notify = s_notify_enabled;
    portEXIT_CRITICAL(&s_state_lock);

    if (!changed || !notify || (conn_handle == BLE_HS_CONN_HANDLE_NONE)) {
        return;
    }

    const int rc = ble_gatts_notify(conn_handle, s_battery_level_handle);
    if (rc != 0) {
        ESP_LOGW(TAG, "failed to queue battery notification; rc=%d", rc);
    }
}
