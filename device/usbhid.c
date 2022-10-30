#include <stdlib.h>
#include <stdarg.h>
#include "usb.h"
#include "usbhid.h"

struct usbhid_handle {
    char ident[64];
    usb_t *usb;
    struct usbhid_device_info* device_info;

    struct {
        int c_errno;
        char errmsg[256];
    } error;
};

static int _error(usbhid_t *usbhid, int code, int c_errno, const char *fmt, ...)
{
    va_list ap;

    usbhid->error.c_errno = c_errno;

    va_start(ap, fmt);
    vsnprintf(usbhid->error.errmsg, sizeof(usbhid->error.errmsg), fmt, ap);
    va_end(ap);

    if (c_errno) {
        char buf[64];
        strerror_r(c_errno, buf, sizeof(buf));
        snprintf(usbhid->error.errmsg+strlen(usbhid->error.errmsg), sizeof(usbhid->error.errmsg)-strlen(usbhid->error.errmsg), ": %s [errno %d]", buf, c_errno);
    }

    return code;
}

usbhid_t *usbhid_new(void)
{
    usbhid_t *usbhid = calloc(1, sizeof(usbhid_t));
    if (usbhid == NULL)
        return NULL;

    usbhid->usb = usb_new();
    if (!usbhid->usb)
        goto fail;

    return usbhid;
fail:
    free(usbhid);
    return NULL;
}

void usbhid_free(usbhid_t *usbhid)
{
    free(usbhid);
}

int usbhid_open(usbhid_t *usbhid, const char *path)
{
    if (usb_init() < 0)
        return _hidraw_error(usbhid, USBHID_ERROR_OPEN, 0, "Openging usbhid device %s", path);


    return 0;
}

int usbhid_close(usbhid_t *usbhid);
{

}

const char *usbhid_errmsg(usbhid_t *usbhid)
{
    return usbhid->error.errmsg;
}

int usbhid_errno(usbhid_t *usbhid)
{
    return usbhid->error.c_errno;
}