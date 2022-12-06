#ifndef __CODEC_H__
#define __CODEC_H__

#include <stdio.h>
#include <stdint.h>

typedef struct aw5808_codec
{
    const char *ident;
    size_t (*encode)(uint8_t *header, size_t data_length);
    size_t (*decode)(const uint8_t *header, size_t length, const uint8_t **data, size_t *data_length);
} aw5808_codec_t;

extern aw5808_codec_t codec_aw5808_serial;
extern aw5808_codec_t codec_aw5808_hid;
#endif