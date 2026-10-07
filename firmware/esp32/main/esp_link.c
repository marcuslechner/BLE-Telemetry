#include "esp_link.h"

#include <string.h>

typedef enum {
    FRAME_LAYOUT_VALID,
    FRAME_LAYOUT_UNKNOWN,
    FRAME_LAYOUT_INVALID,
} frame_layout_result_t;

size_t esp_link_cobs_decode(const uint8_t *input, size_t len, uint8_t *output)
{
    size_t read_index = 0;
    size_t write_index = 0;

    while (read_index < len) {
        const uint8_t code = input[read_index++];

        if ((code == 0U) || ((read_index + code - 1U) > len)) {
            return 0;
        }

        for (uint8_t i = 1; i < code; ++i) {
            output[write_index++] = input[read_index++];
        }

        if ((code != 0xFFU) && (read_index < len)) {
            output[write_index++] = 0;
        }
    }

    return write_index;
}

uint16_t esp_link_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFFU;

    for (size_t i = 0; i < len; ++i) {
        crc ^= (uint16_t)data[i] << 8;
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x8000U) != 0U
                      ? (uint16_t)((crc << 1) ^ 0x1021U)
                      : (uint16_t)(crc << 1);
        }
    }

    return crc;
}

static frame_layout_result_t validate_layout(uint8_t type,
                                              const uint8_t *payload,
                                              size_t payload_len)
{
    switch (type) {
    case ESP_LINK_TYPE_IMU:
        return payload_len == 16U ? FRAME_LAYOUT_VALID : FRAME_LAYOUT_INVALID;

    case ESP_LINK_TYPE_STATUS:
        return payload_len == 14U ? FRAME_LAYOUT_VALID : FRAME_LAYOUT_INVALID;

    case ESP_LINK_TYPE_INFO:
        if (payload_len < 1U) {
            return FRAME_LAYOUT_INVALID;
        }
        switch (payload[0]) {
        case ESP_LINK_INFO_BATTERY_SOC:
            return payload_len == 2U ? FRAME_LAYOUT_VALID : FRAME_LAYOUT_INVALID;
        case ESP_LINK_INFO_ERROR:
            return payload_len == 4U ? FRAME_LAYOUT_VALID : FRAME_LAYOUT_INVALID;
        case ESP_LINK_INFO_PID_PARAMS:
            return payload_len == 14U ? FRAME_LAYOUT_VALID : FRAME_LAYOUT_INVALID;
        case ESP_LINK_INFO_MOTOR:
            return payload_len == 8U ? FRAME_LAYOUT_VALID : FRAME_LAYOUT_INVALID;
        default:
            return FRAME_LAYOUT_UNKNOWN;
        }

    case ESP_LINK_TYPE_LOG:
        return (payload_len >= 1U) && (payload_len <= 65U)
                   ? FRAME_LAYOUT_VALID
                   : FRAME_LAYOUT_INVALID;

    case ESP_LINK_TYPE_REQ:
        return payload_len == 4U ? FRAME_LAYOUT_VALID : FRAME_LAYOUT_INVALID;

    default:
        return FRAME_LAYOUT_UNKNOWN;
    }
}

static void update_sequence(esp_link_decoder_t *decoder, uint8_t sequence)
{
    if (decoder->have_sequence) {
        const uint8_t expected = (uint8_t)(decoder->last_sequence + 1U);

        /* A jump back to zero indicates an STM32 restart. */
        if ((sequence != 0U) && (sequence != expected)) {
            decoder->stats.sequence_gaps += (uint8_t)(sequence - expected);
        }
    }

    decoder->last_sequence = sequence;
    decoder->have_sequence = true;
}

static bool process_encoded_frame(esp_link_decoder_t *decoder,
                                  esp_link_frame_callback_t callback,
                                  void *context)
{
    const size_t decoded_len = esp_link_cobs_decode(
        decoder->encoded, decoder->encoded_len, decoder->decoded);

    if (decoded_len == 0U) {
        ++decoder->stats.decode_errors;
        return false;
    }

    if (decoded_len < 4U) {
        ++decoder->stats.length_errors;
        return false;
    }

    const size_t data_len = decoded_len - 2U;
    const uint16_t received_crc = (uint16_t)decoder->decoded[data_len]
                                  | ((uint16_t)decoder->decoded[data_len + 1U] << 8);

    if (esp_link_crc16(decoder->decoded, data_len) != received_crc) {
        ++decoder->stats.crc_errors;
        return false;
    }

    const uint8_t type = decoder->decoded[0];
    const uint8_t sequence = decoder->decoded[1];
    const uint8_t *payload = &decoder->decoded[2];
    const size_t payload_len = data_len - 2U;

    update_sequence(decoder, sequence);

    const frame_layout_result_t layout = validate_layout(type, payload, payload_len);
    if (layout == FRAME_LAYOUT_UNKNOWN) {
        return false;
    }
    if (layout == FRAME_LAYOUT_INVALID) {
        ++decoder->stats.length_errors;
        return false;
    }

    ++decoder->stats.frames_ok;
    if (callback != NULL) {
        const esp_link_frame_t frame = {
            .type = type,
            .seq = sequence,
            .payload = payload,
            .payload_len = payload_len,
        };
        callback(&frame, context);
    }

    return true;
}

void esp_link_decoder_init(esp_link_decoder_t *decoder)
{
    memset(decoder, 0, sizeof(*decoder));
}

void esp_link_decoder_resync(esp_link_decoder_t *decoder)
{
    decoder->encoded_len = 0;
    decoder->discarding = false;
}

size_t esp_link_decoder_feed(esp_link_decoder_t *decoder,
                             const uint8_t *data,
                             size_t len,
                             esp_link_frame_callback_t callback,
                             void *context)
{
    size_t delivered = 0;

    for (size_t i = 0; i < len; ++i) {
        const uint8_t byte = data[i];

        if (byte == 0U) {
            if (!decoder->discarding && (decoder->encoded_len > 0U)) {
                delivered += process_encoded_frame(decoder, callback, context) ? 1U : 0U;
            }
            decoder->encoded_len = 0;
            decoder->discarding = false;
            continue;
        }

        if (decoder->discarding) {
            continue;
        }

        if (decoder->encoded_len == sizeof(decoder->encoded)) {
            ++decoder->stats.receive_overflows;
            decoder->encoded_len = 0;
            decoder->discarding = true;
            continue;
        }

        decoder->encoded[decoder->encoded_len++] = byte;
    }

    return delivered;
}
