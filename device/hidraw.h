#ifndef __HIDRAW_H__
#define __HIDRAW_H__

#include <stdint.h>
#include <ev.h>
#include "list.h"

typedef struct hidraw_handle hidraw_t;

enum hidraw_error_code {
    HID_ERROR_ARG            = -1, /* Invalid arguments */
    HID_ERROR_OPEN           = -2, /* Opening hidraw device */
    HID_ERROR_QUERY          = -3, /* Querying hidraw device attributes */
    HID_ERROR_CONFIGURE      = -4, /* Configuring hidraw device attributes */
    HID_ERROR_IO             = -5, /* Reading/writing hidraw device */
    HID_ERROR_CLOSE          = -6, /* Closing hidraw device */
};

struct hidraw_client_ops {
    int (*on_receive)(hidraw_t *hidraw, const uint8_t *buf, size_t len);
};

struct hidraw_client {
    char name[64];
    struct hidraw_client_ops *ops;
    struct list_head list;
};

hidraw_t *hidraw_new();
int hidraw_open(hidraw_t *hidraw, const char *path, uint16_t vendor_id, uint16_t product_id, const char *name, struct ev_loop *loop);
int hidraw_close(hidraw_t *hidraw);
ssize_t hidraw_write(hidraw_t *hidraw, const uint8_t *buf, size_t len);
ssize_t hidraw_read(hidraw_t *hidraw, uint8_t *buf, size_t len, int timeout_ms);
void hidraw_free(hidraw_t *hidraw);

/* Error Handling */
const char *hidraw_errmsg(hidraw_t *hidraw);
int hidraw_errno(hidraw_t *hidraw);

int hidraw_fd(hidraw_t *hidraw);
const char* hidraw_id(hidraw_t *hidraw);
#endif