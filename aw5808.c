#include <stdlib.h>

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

aw5808_t *aw5808_new(void)
{
    aw5808_t *aw = calloc(1, sizeof(aw5808_t));
    if (aw == NULL)
        return NULL;

    aw->serial = serial_new();
    if (aw->serial == NULL)
        goto fail;

    aw->hid = hid_new();
    if (aw->hid == NULL)
        goto fail;

    return aw;
fail:
    aw5808_free(aw);
    return NULL;
}

void aw5808_free(aw5808_t *aw)
{
    if (aw->hid)
        hid_free(aw->hid);
    if (aw->serial)
        serial_free(aw->serial);
    if (aw)
        free(aw);
}

int aw5808_open(aw5808_t *aw, aw5808_options_t *opt)
{
    if (serial_open(aw->serial, opt->serial, 57600) !=0 )
        return _aw5808_error(aw, AW5808_ERROR_OPEN, 0, "Openning aw5808 serial %s", opt->serial);
    
    if (hid_open(aw->hid, opt->usb_vid, opt->usb_pid, opt->usb_name) != 0) {
        serial_close(aw->serial);
        return _aw5808_error(aw, AW5808_ERROR_OPEN, 0, "Opening aw5808 hid %s", opt->usb_name);
    }
    return 0;
}

void aw5808_close(aw5808_t *aw)
{
    hid_close(aw->hid);
    serial_close(aw->serial);
}