#include <stdio.h>
#include <string.h>
#include "codec.h"

static ssize_t aw5808_serial_encode(int8_t *header, size_t data_length)
{
    return 0;
}

static ssize_t aw5808_serial_decode(const int8_t *header, size_t length, const void **data, size_t *data_length)
{
    return 0;
}

static ssize_t aw5808_hid_encode(int8_t *header, size_t data_length)
{
    return 0;
}

static ssize_t aw5808_hid_decode(const int8_t *header, size_t length, const void **data, size_t *data_length)
{
    return 0;
}

codec_t codec_aw5808_serial = {
   .ident = "aw5808_serial",
   .encode = aw5808_serial_encode,
   .decode = aw5808_serial_decode,
};

codec_t codec_aw5808_hid = {
   .ident = "aw5808_hid",
   .encode = aw5808_hid_encode,
   .decode = aw5808_hid_decode,
};

const codec_t *codecs[] = {
    &codec_aw5808_serial,
    &codec_aw5808_hid,
    NULL,
};

const codec_t *get_codec(const char *name)
{
    int i;

    if (name == NULL)
        return NULL;

    for(i=0; codecs[i]; i++) {
        if (!strncmp(name, codecs[i]->ident, strlen(name))) {
            return codecs[i];
        }
    }
    return NULL;
}