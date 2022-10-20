#ifndef __AW5808_H__
#define __AW5808_H__

#include <stdint.h>
#include <ev.h>

#include "hid.h"
#include "serial.h"

enum aw5808_error_code {
    AW5808_ERROR_ARG            = -1, /* Invalid arguments */
    AW5808_ERROR_OPEN           = -2, /* Opening aw5808 device */
    AW5808_ERROR_QUERY          = -3, /* Querying aw5808 device attributes */
    AW5808_ERROR_CONFIGURE      = -4, /* Configuring aw5808 device attributes */
    AW5808_ERROR_IO_SERIAL      = -5, /* Reading/writing aw5808 device via serial */
    AW5808_ERROR_IO_HID         = -6, /* Reading/writing aw5808 device via hid */
    AW5808_ERROR_CLOSE          = -7, /* Closing aw5808 device */
};

typedef enum aw5808_mode {
    AW5808_MODE_I2S = 0,
    AW5808_MODE_USB = 1,
} aw5808_mode_t;

typedef struct aw5808_handle aw5808_t;
struct aw5808_cbs {
    void (*on_set_mode)(aw5808_t *aw, const uint8_t *data, int len);
    void (*on_get_config)(aw5808_t *aw, const uint8_t *data, int len);
};

typedef struct aw5808_options {
    const char *serial;
    uint16_t usb_vid;
    uint16_t usb_pid;
    const char *usb_name;
    aw5808_mode_t mode;
    struct aw5808_cbs *cbs;
    struct ev_loop *loop;
} aw5808_options_t;

aw5808_t *aw5808_new();
void aw5808_free(aw5808_t *aw);
int aw5808_open(aw5808_t *aw, aw5808_options_t *opt);
void aw5808_close(aw5808_t *aw);
int aw5808_set_mode(aw5808_t *aw, aw5808_mode_t mode);
int aw5808_read_fw(aw5808_t *aw, uint8_t *buf, size_t len);

const char *aw5808_id(aw5808_t *aw);
const char *aw5808_tostring(aw5808_t *aw);
void aw5808_set_cbs(aw5808_t *aw, struct aw5808_cbs *cbs);

/* Error Handling */
int aw5808_errno(aw5808_t *aw);
const char *aw5808_errmsg(aw5808_t *aw);

#endif