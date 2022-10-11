#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "aw5808.h"
#include "hid.h"
#include "serial.h"

struct aw5808_handle {
    serial_t *serial;
    hid_t *hid;

    struct {
        int c_errno;
        char errmsg[96];
    } error;
};

static int _aw5808_error(aw5808_t *aw5808, int code, int c_errno, const char *fmt, ...)
{
    va_list ap;

    aw5808->error.c_errno = c_errno;

    va_start(ap, fmt);
    vsnprintf(aw5808->error.errmsg, sizeof(aw5808->error.errmsg), fmt, ap);
    va_end(ap);

    if (c_errno) {
        char buf[64];
        strerror_r(c_errno, buf, sizeof(buf));
        snprintf(aw5808->error.errmsg+strlen(aw5808->error.errmsg), sizeof(aw5808->error.errmsg)-strlen(aw5808->error.errmsg), ": %s [errno %d]", buf, c_errno);
    }

    return code;
}

const char *aw5808_errmsg(aw5808_t *aw5808)
{
    return aw5808->error.errmsg;
}

int aw5808_errno(aw5808_t *aw5808)
{
    return aw5808->error.c_errno;
}

aw5808_t *aw5808_new(void)
{
    aw5808_t *aw5808 = calloc(1, sizeof(aw5808_t));
    if (aw5808 == NULL)
        return NULL;

    aw5808->serial = serial_new();
    if (aw5808->serial == NULL)
        goto fail;

    aw5808->hid = hid_new();
    if (aw5808->hid == NULL)
        goto fail;

    return aw5808;
fail:
    aw5808_free(aw5808);
    return NULL;
}

void aw5808_free(aw5808_t *aw5808)
{
    if (aw5808->hid)
        hid_free(aw5808->hid);
    if (aw5808->serial)
        serial_free(aw5808->serial);
    if (aw5808)
        free(aw5808);
}

int aw5808_open(aw5808_t *aw5808, aw5808_options_t *opt)
{
    if (serial_open(aw5808->serial, opt->serial, 57600) !=0) {
        return _aw5808_error(aw5808, AW5808_ERROR_OPEN, 0, "Openning aw5808 serial %s", opt->serial);
    }
    if (hid_open(aw5808->hid, opt->usb_vid, opt->usb_pid, opt->usb_name) != 0) {
        serial_close(aw5808->serial);
        return _aw5808_error(aw5808, AW5808_ERROR_OPEN, 0, "Opening aw5808 hid %s", opt->usb_name);
    }
    return 0;
}

void aw5808_close(aw5808_t *aw5808)
{
    hid_close(aw5808->hid);
    serial_close(aw5808->serial);
}