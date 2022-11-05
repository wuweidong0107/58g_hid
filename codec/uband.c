#include <stdio.h>
#include <string.h>
#include "codec.h"

static size_t uband_encode(uint8_t *frame, size_t data_len)
{
    return 0;
}

static size_t uband_decode(const uint8_t *frame, size_t length, const uint8_t **data, size_t *data_len)
{

    return 0;
}

codec_t codec_uband = {
   .ident = "uband",
   .encode = uband_encode,
   .decode = uband_decode,
};
