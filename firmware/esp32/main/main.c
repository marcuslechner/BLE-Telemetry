#include <inttypes.h>
#include <stdlib.h>

#include "ble_battery.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_link.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#define ESP_LINK_UART UART_NUM_1
#define UART_RX_BUFFER_SIZE 2048
#define UART_TX_BUFFER_SIZE 256
#define UART_EVENT_QUEUE_SIZE 20
#define UART_READ_CHUNK_SIZE 128

static const char *TAG = "ble_telemetry";
static QueueHandle_t s_uart_queue;
static esp_link_decoder_t s_decoder;
static uint32_t s_uart_errors;
static uint32_t s_uart_overruns;

static void handle_esp_link_frame(const esp_link_frame_t *frame, void *context)
{
    (void)context;

    if ((frame->type == ESP_LINK_TYPE_INFO)
        && (frame->payload[0] == ESP_LINK_INFO_BATTERY_SOC)) {
        ble_battery_set_level(frame->payload[1]);
        ESP_LOGI(TAG, "battery=%u%% seq=%u", frame->payload[1], frame->seq);
    }

    if ((s_decoder.stats.frames_ok % 10U) == 0U) {
        ESP_LOGI(TAG,
                 "link: ok=%" PRIu32 " gaps=%" PRIu32 " crc=%" PRIu32
                 " decode=%" PRIu32 " length=%" PRIu32 " rx_overflow=%" PRIu32
                 " uart_overrun=%" PRIu32 " uart_error=%" PRIu32,
                 s_decoder.stats.frames_ok,
                 s_decoder.stats.sequence_gaps,
                 s_decoder.stats.crc_errors,
                 s_decoder.stats.decode_errors,
                 s_decoder.stats.length_errors,
                 s_decoder.stats.receive_overflows,
                 s_uart_overruns,
                 s_uart_errors);
    }
}

static void uart_event_task(void *argument)
{
    (void)argument;
    uart_event_t event;
    uint8_t bytes[UART_READ_CHUNK_SIZE];

    for (;;) {
        if (xQueueReceive(s_uart_queue, &event, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        switch (event.type) {
        case UART_DATA: {
            size_t remaining = event.size;
            while (remaining > 0U) {
                const size_t requested = remaining < sizeof(bytes) ? remaining : sizeof(bytes);
                const int received = uart_read_bytes(
                    ESP_LINK_UART, bytes, requested, pdMS_TO_TICKS(100));
                if (received <= 0) {
                    break;
                }
                esp_link_decoder_feed(
                    &s_decoder, bytes, (size_t)received, handle_esp_link_frame, NULL);
                remaining -= (size_t)received;
            }
            break;
        }

        case UART_FIFO_OVF:
        case UART_BUFFER_FULL:
            ++s_uart_overruns;
            uart_flush_input(ESP_LINK_UART);
            xQueueReset(s_uart_queue);
            esp_link_decoder_resync(&s_decoder);
            ESP_LOGW(TAG, "UART receive overrun; discarded partial frame");
            break;

        case UART_PARITY_ERR:
        case UART_FRAME_ERR:
            ++s_uart_errors;
            ESP_LOGW(TAG, "UART line error: event=%d", event.type);
            break;

        default:
            break;
        }
    }
}

void app_main(void)
{
    const uart_config_t uart_config = {
        .baud_rate = CONFIG_BLE_TELEMETRY_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_LOGI(TAG, "BLE-Telemetry starting on ESP32-S3");
    esp_link_decoder_init(&s_decoder);

    ESP_ERROR_CHECK(uart_driver_install(ESP_LINK_UART,
                                        UART_RX_BUFFER_SIZE,
                                        UART_TX_BUFFER_SIZE,
                                        UART_EVENT_QUEUE_SIZE,
                                        &s_uart_queue,
                                        0));
    ESP_ERROR_CHECK(uart_param_config(ESP_LINK_UART, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(ESP_LINK_UART,
                                 CONFIG_BLE_TELEMETRY_UART_TX_GPIO,
                                 CONFIG_BLE_TELEMETRY_UART_RX_GPIO,
                                 UART_PIN_NO_CHANGE,
                                 UART_PIN_NO_CHANGE));

    ESP_LOGI(TAG,
             "ESP Link UART1: TX=GPIO%d RX=GPIO%d baud=%d",
             CONFIG_BLE_TELEMETRY_UART_TX_GPIO,
             CONFIG_BLE_TELEMETRY_UART_RX_GPIO,
             CONFIG_BLE_TELEMETRY_UART_BAUD_RATE);

    ESP_ERROR_CHECK(ble_battery_init());

    const BaseType_t created = xTaskCreate(
        uart_event_task, "esp_link_uart", 4096, NULL, 10, NULL);
    if (created != pdPASS) {
        ESP_LOGE(TAG, "failed to create UART task");
        abort();
    }
}
