#include <stdio.h>
#include <string.h>
#include "codec.h"
#include "log.h"

extern codec_t codec_aw5808_serial;
extern codec_t codec_aw5808_hid;
extern codec_t codec_uband;

static codec_t *codecs[] = {
    &codec_aw5808_serial,
    &codec_aw5808_hid,
    &codec_uband,
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