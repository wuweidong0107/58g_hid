#include <stdio.h>
#include <string.h>
#include "codec.h"

enum {
    PREAMBLE_AW5808_SERIAL = 0x55,
    DELIMITER_AW5808_SERIAL = 0xAA,
};

struct protocol_head_aw5080_serial {
    uint8_t preamble;
    uint8_t delimiter;
    uint8_t payload_length;
    uint8_t data[0];
};

static size_t aw5808_serial_encode(uint8_t *frame, size_t data_len)
{
    if(data_len < 1)
        return 0;
    
    struct protocol_head_aw5080_serial *header = (struct protocol_head_aw5080_serial *)frame;
    header->preamble = PREAMBLE_AW5808_SERIAL;
    header->delimiter = DELIMITER_AW5808_SERIAL;
    header->payload_length = data_len - 1;
    
    size_t cmd_len = 1;         /* 1 byte CommandID */
    size_t checksum_len = 1;    /* 1 byte CheckSum */
    size_t frame_len = sizeof(struct protocol_head_aw5080_serial) + cmd_len + header->payload_length + checksum_len;
    uint8_t *checksum = (uint8_t*)header + frame_len -1;
    *checksum = 0x0;
    return frame_len;
}

/* 0x55 0xAA [1 byte datalen] [1 byte cmd] [ n byte pyaload] [1 byte checksum] */
static size_t aw5808_serial_decode(const uint8_t *frame, size_t length, const uint8_t **data, size_t *data_len)
{
    *data = NULL;
    if (length < 4)
        return 0;

    const struct protocol_head_aw5080_serial *header = (struct protocol_head_aw5080_serial *)frame;
    if (header->preamble != PREAMBLE_AW5808_SERIAL || header->delimiter != DELIMITER_AW5808_SERIAL)
        return 2;

    size_t cmd_len = 1;         /* 1 byte CommandID */
    size_t checksum_len = 1;    /* 1 byte CheckSum */
    size_t frame_len = sizeof(struct protocol_head_aw5080_serial) + cmd_len + header->payload_length  + checksum_len;
    if (length < frame_len)
        return 0;
    
    *data = header->data;
    *data_len = cmd_len + header->payload_length;
    return frame_len;
}

codec_t codec_aw5808_serial = {
   .ident = "aw5808_serial",
   .encode = aw5808_serial_encode,
   .decode = aw5808_serial_decode,
};
