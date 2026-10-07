#ifndef ESP_LINK_H
#define ESP_LINK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ESP_LINK_BUFFER_SIZE 256U

enum {
    ESP_LINK_TYPE_IMU = 0x01,
    ESP_LINK_TYPE_STATUS = 0x02,
    ESP_LINK_TYPE_INFO = 0x03,
    ESP_LINK_TYPE_LOG = 0x04,
    ESP_LINK_TYPE_REQ = 0x10,
};

enum {
    ESP_LINK_INFO_BATTERY_SOC = 0x01,
    ESP_LINK_INFO_ERROR = 0x02,
    ESP_LINK_INFO_PID_PARAMS = 0x03,
    ESP_LINK_INFO_MOTOR = 0x04,
};

typedef struct {
    uint8_t type;
    uint8_t seq;
    const uint8_t *payload;
    size_t payload_len;
} esp_link_frame_t;

typedef struct {
    uint32_t frames_ok;
    uint32_t sequence_gaps;
    uint32_t crc_errors;
    uint32_t decode_errors;
    uint32_t length_errors;
    uint32_t receive_overflows;
} esp_link_stats_t;

typedef void (*esp_link_frame_callback_t)(const esp_link_frame_t *frame, void *context);

typedef struct {
    uint8_t encoded[ESP_LINK_BUFFER_SIZE];
    uint8_t decoded[ESP_LINK_BUFFER_SIZE];
    size_t encoded_len;
    bool discarding;
    bool have_sequence;
    uint8_t last_sequence;
    esp_link_stats_t stats;
} esp_link_decoder_t;

void esp_link_decoder_init(esp_link_decoder_t *decoder);

/* Drop a partial frame while preserving sequence state and diagnostics. */
void esp_link_decoder_resync(esp_link_decoder_t *decoder);

/* Returns the number of validated, known frames delivered to callback. */
size_t esp_link_decoder_feed(esp_link_decoder_t *decoder,
                             const uint8_t *data,
                             size_t len,
                             esp_link_frame_callback_t callback,
                             void *context);

size_t esp_link_cobs_decode(const uint8_t *input, size_t len, uint8_t *output);
uint16_t esp_link_crc16(const uint8_t *data, size_t len);

#endif
