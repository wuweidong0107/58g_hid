#ifndef __IO_CHANNEL_H__
#define __IO_CHANNEL_H__

#include <ev.h>
#include "iobuf.h"

struct io_channel {
    int fd;
    struct ev_io ior;
    struct ev_io iow;
    struct iobuf rbuf;
    struct iobuf wbuf;
};

#endif