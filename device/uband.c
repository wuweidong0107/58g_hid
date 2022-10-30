#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <pthread.h>
#include <libudev.h>
#include <ev.h>

#include "uband.h"
#include "serial.h"
#include "codec.h"
#include "log.h"
#include "utils.h"
#include "io_channel.h"

struct uband_handle {
    char ident[128];
    /* io */
    struct ev_loop *loop;
    serial_t *serial;
    const codec_t *codec;
    /* error handle */
    struct {
        int c_errno;
        char errmsg[96];
    } error;
};