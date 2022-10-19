#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <pthread.h>

#include "aw5808.h"
#include "codec.h"
#include "log.h"
#include "utils.h"

struct aw5808_handle {
    serial_t *serial;
    const codec_t *codec_serial;
    hid_t *hid;
    const codec_t *codec_hid;

    int mode;    // USB / I2S
    struct aw5808_cbs *cbs;

    struct {
        int c_errno;
        char errmsg[96];
    } error;
};

enum aw5808_58g_rw {
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

aw5808_t *aw5808_new(struct ev_loop *loop)
{
    aw5808_t *aw = calloc(1, sizeof(aw5808_t));
    if (aw == NULL)
        return NULL;

    aw->serial = serial_new(loop);
    if (aw->serial == NULL)
        goto fail;

    aw->hid = hid_new(loop);
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

static int on_serial_read(serial_t *serial, const uint8_t *buf, int len)
{
    aw5808_t *aw = serial_get_userdata(serial);
    const codec_t *codec_serial = aw->codec_serial;
    size_t used = 0;
	const uint8_t *payload = NULL;
	size_t payload_len, ret;

    for(;;) {
        ret = codec_serial->decode(buf, len, &payload, &payload_len);
        if (!payload)
            break;
        used += ret;
        buf += ret;
        len -= ret;
        switch(payload[0]) {
            case 0xD4:
                aw->mode = payload[0] == 1 ? AW5808_MODE_I2S:AW5808_MODE_USB;
                if (aw->cbs->on_set_mode)
                    aw->cbs->on_set_mode(aw, payload, payload_len);
                break;
            default:
                break;
        }
    }
    return used;
}

static struct serial_cbs cbs = {
    .on_read = on_serial_read,
};

int aw5808_open(aw5808_t *aw, aw5808_options_t *opt)
{   
    if (serial_open(aw->serial, opt->serial, 57600, &cbs) !=0) {
        return _aw5808_error(aw, AW5808_ERROR_OPEN, 0, "Openning aw5808 serial %s", opt->serial);
    }

    serial_set_userdata(aw->serial, aw);
    aw->codec_serial = get_codec("aw5808_serial");
    if (!aw->codec_serial)
        return _aw5808_error(aw, AW5808_ERROR_OPEN, 0, "Getting aw5808 serial codec");

    // if (aw5808_set_mode(aw, opt->mode))
    //     return _aw5808_error(aw, AW5808_ERROR_OPEN, 0, "Setting aw5808 mode");
/*
    if (aw->mode == AW5808_MODE_USB) {
        if (hid_open(aw->hid, opt->usb_vid, opt->usb_pid, opt->usb_name) != 0) {
            serial_close(aw->serial);
            return _aw5808_error(aw, AW5808_ERROR_OPEN, 0, "Opening aw5808 hid %s", opt->usb_name);
        }
    }
*/
    return 0;
}

void aw5808_close(aw5808_t *aw)
{
    hid_close(aw->hid);
    serial_close(aw->serial);
}

int aw5808_set_mode(aw5808_t *aw, aw5808_mode_t mode)
{
    uint8_t buf[] = {0x55, 0xaa, 0x01, 0x54, 0x01, 0x00};
    size_t len = sizeof(buf) / sizeof(buf[0]);

    if (mode != AW5808_MODE_USB && mode != AW5808_MODE_I2S)
        return -1;

    buf[4] = mode;
    if (serial_write(aw->serial, buf, len) != len)
        return _aw5808_error(aw, AW5808_ERROR_CONFIGURE, 0, "Setting mode");

    return 0;
}

int aw5808_read_fw(aw5808_t *aw, uint8_t *buf, size_t len)
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
    return 0;

/*
    if (hid_read(aw->hid, (uint8_t *) &pkt, sizeof(pkt), timeout_ms) != sizeof(pkt)) {
        return _aw5808_error(aw, AW5808_ERROR_ARG, 0, "aw5808 hid reading");
    }
    buf[0] = pkt.data[0];
    buf[1] = pkt.data[1];
*/
    return 2;
}

int aw5808_hid_fd(aw5808_t *aw)
{
    return hid_fd(aw->hid);
}

int aw5808_serial_fd(aw5808_t *aw)
{
    return serial_fd(aw->serial);
}

void aw5808_set_cbs(aw5808_t *aw, struct aw5808_cbs *cbs)
{
    aw->cbs = cbs;
}