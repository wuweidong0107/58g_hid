#include <stdio.h>
#include <string.h>
#include "codec.h"

static size_t aw5808_hid_encode(uint8_t *frame, size_t data_len)
{
    return 0;
}

static size_t aw5808_hid_decode(const uint8_t *frame, size_t length, const uint8_t **data, size_t *data_len)
{

    return 0;
}

aw5808_codec_t codec_aw5808_hid = {
   .ident = "aw5808_hid",
   .encode = aw5808_hid_encode,
   .decode = aw5808_hid_decode,
};