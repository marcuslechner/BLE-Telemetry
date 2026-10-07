#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "esp_link.h"

typedef struct {
    size_t count;
    uint8_t last_type;
    uint8_t last_seq;
    uint8_t last_payload[32];
    size_t last_payload_len;
} capture_t;

static void capture_frame(const esp_link_frame_t *frame, void *context)
{
    capture_t *capture = context;
    ++capture->count;
    capture->last_type = frame->type;
    capture->last_seq = frame->seq;
    capture->last_payload_len = frame->payload_len;
    assert(frame->payload_len <= sizeof(capture->last_payload));
    memcpy(capture->last_payload, frame->payload, frame->payload_len);
}

static size_t cobs_encode(const uint8_t *input, size_t len, uint8_t *output)
{
    size_t read_index = 0;
    size_t write_index = 1;
    size_t code_index = 0;
    uint8_t code = 1;

    while (read_index < len) {
        if (input[read_index] == 0U) {
            output[code_index] = code;
            code = 1;
            code_index = write_index++;
            ++read_index;
        } else {
            output[write_index++] = input[read_index++];
            ++code;
            if (code == 0xFFU) {
                output[code_index] = code;
                code = 1;
                code_index = write_index++;
            }
        }
    }

    output[code_index] = code;
    return write_index;
}

static size_t make_wire_frame(uint8_t type,
                              uint8_t sequence,
                              const uint8_t *payload,
                              size_t payload_len,
                              uint8_t *wire)
{
    uint8_t decoded[128];
    decoded[0] = type;
    decoded[1] = sequence;
    memcpy(&decoded[2], payload, payload_len);
    const size_t crc_offset = payload_len + 2U;
    const uint16_t crc = esp_link_crc16(decoded, crc_offset);
    decoded[crc_offset] = (uint8_t)crc;
    decoded[crc_offset + 1U] = (uint8_t)(crc >> 8);

    const size_t encoded_len = cobs_encode(decoded, crc_offset + 2U, wire);
    wire[encoded_len] = 0;
    return encoded_len + 1U;
}

static void test_crc_check_value(void)
{
    static const uint8_t check[] = "123456789";
    assert(esp_link_crc16(check, sizeof(check) - 1U) == 0x29B1U);
}

static void test_protocol_golden_vectors(void)
{
    static const uint8_t wire[] = {
        0x02, 0x03, 0x05, 0x01, 0x4E, 0x27, 0x85, 0x00,
        0x07, 0x03, 0x01, 0x01, 0x4E, 0x17, 0xB2, 0x00,
        0x07, 0x03, 0x02, 0x01, 0x4E, 0x47, 0xEB, 0x00,
    };
    esp_link_decoder_t decoder;
    capture_t capture = {0};
    esp_link_decoder_init(&decoder);

    /* Feed deliberately awkward chunks to exercise stream reassembly. */
    assert(esp_link_decoder_feed(&decoder, wire, 3, capture_frame, &capture) == 0U);
    assert(esp_link_decoder_feed(&decoder, &wire[3], 9, capture_frame, &capture) == 1U);
    assert(esp_link_decoder_feed(
               &decoder, &wire[12], sizeof(wire) - 12U, capture_frame, &capture)
           == 2U);

    assert(capture.count == 3U);
    assert(capture.last_type == ESP_LINK_TYPE_INFO);
    assert(capture.last_seq == 2U);
    assert(capture.last_payload_len == 2U);
    assert(capture.last_payload[0] == ESP_LINK_INFO_BATTERY_SOC);
    assert(capture.last_payload[1] == 78U);
    assert(decoder.stats.frames_ok == 3U);
    assert(decoder.stats.sequence_gaps == 0U);
}

static void test_crc_error(void)
{
    static const uint8_t bad_wire[] = {
        0x07, 0x03, 0x01, 0x01, 0x4E, 0x16, 0xB2, 0x00,
    };
    esp_link_decoder_t decoder;
    capture_t capture = {0};
    esp_link_decoder_init(&decoder);

    assert(esp_link_decoder_feed(
               &decoder, bad_wire, sizeof(bad_wire), capture_frame, &capture)
           == 0U);
    assert(capture.count == 0U);
    assert(decoder.stats.crc_errors == 1U);
}

static void test_sequence_gap_and_restart(void)
{
    uint8_t wire[64];
    static const uint8_t payload[] = {ESP_LINK_INFO_BATTERY_SOC, 78};
    esp_link_decoder_t decoder;
    esp_link_decoder_init(&decoder);

    size_t len = make_wire_frame(ESP_LINK_TYPE_INFO, 0, payload, sizeof(payload), wire);
    assert(esp_link_decoder_feed(&decoder, wire, len, NULL, NULL) == 1U);
    len = make_wire_frame(ESP_LINK_TYPE_INFO, 2, payload, sizeof(payload), wire);
    assert(esp_link_decoder_feed(&decoder, wire, len, NULL, NULL) == 1U);
    assert(decoder.stats.sequence_gaps == 1U);

    len = make_wire_frame(ESP_LINK_TYPE_INFO, 0, payload, sizeof(payload), wire);
    assert(esp_link_decoder_feed(&decoder, wire, len, NULL, NULL) == 1U);
    assert(decoder.stats.sequence_gaps == 1U);
}

static void test_length_and_overflow_errors(void)
{
    uint8_t wire[64];
    static const uint8_t short_battery[] = {ESP_LINK_INFO_BATTERY_SOC};
    esp_link_decoder_t decoder;
    esp_link_decoder_init(&decoder);

    size_t len = make_wire_frame(
        ESP_LINK_TYPE_INFO, 1, short_battery, sizeof(short_battery), wire);
    assert(esp_link_decoder_feed(&decoder, wire, len, NULL, NULL) == 0U);
    assert(decoder.stats.length_errors == 1U);

    uint8_t oversized[ESP_LINK_BUFFER_SIZE + 2U];
    memset(oversized, 0x01, sizeof(oversized));
    oversized[sizeof(oversized) - 1U] = 0;
    assert(esp_link_decoder_feed(
               &decoder, oversized, sizeof(oversized), NULL, NULL)
           == 0U);
    assert(decoder.stats.receive_overflows == 1U);
}

int main(void)
{
    test_crc_check_value();
    test_protocol_golden_vectors();
    test_crc_error();
    test_sequence_gap_and_restart();
    test_length_and_overflow_errors();
    puts("esp_link tests passed");
    return 0;
}
