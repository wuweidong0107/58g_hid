#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <pthread.h>

#include "aw5808.h"
#include "hid.h"
#include "serial.h"

struct aw5808_handle {
    serial_t *serial;
    hid_t *hid;
    int mode;    // USB / I2S
    pthread_mutex_t mutex;

    struct {
        int c_errno;
        char errmsg[96];
    } error;
};

enum hid_58g_rw {
    HID_58G_WRITE = 0x01,
    HID_58G_READ = 0x02,
};

typedef struct {
    uint8_t rw;     // 1: write, 2: read
    uint8_t reg;    // reg address
    uint8_t len;    // data length
    uint8_t data[61];
} __attribute__((packed)) hid_packet_t;

static int _aw5808_error(aw5808_t *aw, int code, int c_errno, const char *fmt, ...)
{
    va_list ap;

    aw->error.c_errno = c_errno;

    va_start(ap, fmt);
    vsnprintf(aw->error.errmsg, sizeof(aw->error.errmsg), fmt, ap);
    va_end(ap);

    if (c_errno) {
        char buf[64];
        strerror_r(c_errno, buf, sizeof(buf));
        snprintf(aw->error.errmsg+strlen(aw->error.errmsg), sizeof(aw->error.errmsg)-strlen(aw->error.errmsg), ": %s [errno %d]", buf, c_errno);
    }

    return code;
}

const char *aw5808_errmsg(aw5808_t *aw)
{
    return aw->error.errmsg;
}

int aw5808_errno(aw5808_t *aw)
{
    return aw->error.c_errno;
}

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

    pthread_mutex_init(&aw->mutex, NULL);

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
    if (serial_open(aw->serial, opt->serial, 57600) !=0) {
        return _aw5808_error(aw, AW5808_ERROR_OPEN, 0, "Openning aw serial %s", opt->serial);
    }

    /* TODO: read mode (USB / I2S) */
    
    if (hid_open(aw->hid, opt->usb_vid, opt->usb_pid, opt->usb_name) != 0) {
        serial_close(aw->serial);
        return _aw5808_error(aw, AW5808_ERROR_OPEN, 0, "Opening aw hid %s", opt->usb_name);
    }
    return 0;
}

void aw5808_close(aw5808_t *aw)
{
    hid_close(aw->hid);
    serial_close(aw->serial);
}

int aw5808_read_fw(aw5808_t *aw, uint8_t *buf, size_t len, int timeout_ms)
{
    if (len < 2) {
        return _aw5808_error(aw, AW5808_ERROR_ARG, 0, "Firmware version len too short");
    }

    hid_packet_t pkt = {
        .rw = HID_58G_READ,
        .reg = 0x0,
        .len = 0x02,
        .data = {0},
    };
    
    if (hid_write(aw->hid, (uint8_t *) &pkt, sizeof(pkt)) != sizeof(pkt)) {
        return _aw5808_error(aw, AW5808_ERROR_ARG, 0, "aw5808 hid writing");
    }

    if (hid_read(aw->hid, (uint8_t *) &pkt, sizeof(pkt), timeout_ms) != sizeof(pkt)) {
        return _aw5808_error(aw, AW5808_ERROR_ARG, 0, "aw5808 hid reading");
    }
    buf[0] = pkt.data[0];
    buf[1] = pkt.data[1];
    return 2;
}

void aw5808_lock(aw5808_t *aw)
{
    pthread_mutex_lock(&aw->mutex);
}

void aw5808_unlock(aw5808_t *aw)
{
    pthread_mutex_unlock(&aw->mutex);
}