#ifndef __HID_H__
#define __HID_H__

#include <stdint.h>

typedef struct hid_handle hid_t;

enum serial_error_code {
    HID_ERROR_ARG            = -1, /* Invalid arguments */
    HID_ERROR_OPEN           = -2, /* Opening serial port */
    HID_ERROR_QUERY          = -3, /* Querying serial port attributes */
    HID_ERROR_CONFIGURE      = -4, /* Configuring serial port attributes */
    HID_ERROR_IO             = -5, /* Reading/writing serial port */
    HID_ERROR_CLOSE          = -6, /* Closing serial port */
};

hid_t *hid_new(void);
int hid_open(hid_t *hid, unsigned short vendor_id, unsigned short product_id, const char *name);
const char *hid_errmsg(hid_t *hid);
ssize_t hid_write(hid_t *hid, const uint8_t *buf, size_t len);
ssize_t hid_read(hid_t *hid, uint8_t *buf, size_t len, int timeout_ms);
#endif