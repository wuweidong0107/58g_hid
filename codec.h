#ifndef __CODEC_H__
#define __CODEC_H__

#include <stdio.h>
#include <stdint.h>

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

typedef struct codec
{
    const char *ident;
    size_t (*encode)(uint8_t *header, size_t data_length);
    size_t (*decode)(const uint8_t *header, size_t length, const uint8_t **data, size_t *data_length);
} codec_t;

const codec_t *get_codec(const char *name);
#endif