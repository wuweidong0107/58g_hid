#include <stdio.h>
#include "codec.h"

codec_t codec_aw5808_serial = {
   .ident = "aw5808_serial",
};

codec_t codec_aw5808_hid = {
   .ident = "aw5808_serial",
};

const codec_t *codecs[] = {
    &codec_aw5808_serial,
    &codec_aw5808_hid,
    NULL,
};