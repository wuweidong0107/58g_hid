#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <pthread.h>
#include <libudev.h>
#include <ev.h>

#include "aw5808.h"
#include "codec.h"
#include "log.h"
#include "utils.h"
#include "io_channel.h"

struct aw5808_handle {
    char ident[32];

    struct ev_loop *loop;
    serial_t *serial;
    const codec_t *codec_serial;
    char usb_name[64];
    hid_t *hid;
    const codec_t *codec_hid;
    int mode;

    int udev_fd;
    struct io_channel udev_io;
    struct udev *udev;
    struct udev_monitor *mon;

    struct aw5808_cbs *cbs;
    struct {
        int c_errno;
        char errmsg[96];
    } error;
};

enum aw5808_usbid {
    AW5808_USB_VID = 0x25a7,
    AW5808_USB_PID = 0x5804,
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

static void _aw5808_udev_read_cb(struct ev_loop *loop, struct ev_io *w, int revents)
{
    aw5808_t *aw = container_of(w, aw5808_t, udev_io.ior);
    struct udev_device *dev;
    log_debug("");
    dev = udev_monitor_receive_device(aw->mon);
    if (dev && (strstr(udev_device_get_devpath(dev), "25A7:5804") != NULL)) {
        log_debug("");
        if (!strcmp(udev_device_get_action(dev), "add")) {
            hid_open(aw->hid, udev_device_get_devnode(dev), 0, 0, NULL, aw->loop);
        } else if (!strcmp(udev_device_get_action(dev), "remove")) {
            hid_close(aw->hid);
        }
        udev_device_unref(dev);
    }
}

const char *aw5808_errmsg(aw5808_t *aw)
{
    return aw->error.errmsg;
}

int aw5808_errno(aw5808_t *aw)
{
    return aw->error.c_errno;
}

aw5808_t *aw5808_new()
{
    aw5808_t *aw = calloc(1, sizeof(aw5808_t));
    if (!aw)
        return NULL;

    aw->serial = serial_new();
    if (!aw->serial)
        goto fail;

    aw->hid = hid_new();
    if (!aw->hid)
        goto fail;

	/* create udev object */
    aw->udev = udev_new();
    if (!aw->udev)
        goto fail;

    return aw;
fail:
    aw5808_free(aw);
    return NULL;
}

void aw5808_free(aw5808_t *aw)
{
    udev_unref(aw->udev);

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
    int new_mode;

    for(;;) {
        ret = codec_serial->decode(buf, len, &payload, &payload_len);
        if (!payload)
            break;
        used += ret;
        buf += ret;
        len -= ret;
        switch(payload[0]) {
            case 0xD4:
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
    if (serial_open(aw->serial, opt->serial, 57600, &cbs, opt->loop) !=0) {
        return _aw5808_error(aw, AW5808_ERROR_OPEN, 0, "Openning aw5808 serial %s", opt->serial);
    }

    serial_set_userdata(aw->serial, aw);
    aw->codec_serial = get_codec("aw5808_serial");
    if (!aw->codec_serial)
        return _aw5808_error(aw, AW5808_ERROR_OPEN, 0, "Getting aw5808 serial codec");

    if (opt->usb_name)
        strncpy(aw->usb_name, opt->usb_name, sizeof(aw->usb_name)-1);

    aw->mon = udev_monitor_new_from_netlink(aw->udev, "udev");
	udev_monitor_filter_add_match_subsystem_devtype(aw->mon, "hidraw", NULL);
	udev_monitor_enable_receiving(aw->mon);
	aw->udev_fd = udev_monitor_get_fd(aw->mon);
    aw->loop = opt->loop;
    ev_io_init(&aw->udev_io.ior, _aw5808_udev_read_cb, aw->udev_fd, EV_READ);
    ev_io_start(aw->loop, &aw->udev_io.ior);
    return 0;
}

void aw5808_close(aw5808_t *aw)
{
    hid_close(aw->hid);
    serial_close(aw->serial);
}

int aw5808_get_config(aw5808_t *aw)
{
     uint8_t buf[] = {0x55, 0xaa, 0x01, 0x50, 0x00, 0x00};
     size_t len = sizeof(buf) / sizeof(buf[0]);

    if (serial_write(aw->serial, buf, len) != len)
        return _aw5808_error(aw, AW5808_ERROR_QUERY, 0, "Getting config");

    return 0;
}

int aw5808_set_mode(aw5808_t *aw, aw5808_mode_t mode)
{
    uint8_t buf[] = {0x55, 0xaa, 0x01, 0x54, 0x01, 0x00};
    size_t len = sizeof(buf) / sizeof(buf[0]);

    if (mode != AW5808_MODE_USB && mode != AW5808_MODE_I2S)
        return -1;

    buf[4] = mode;
    if (mode == AW5808_MODE_I2S && aw->mode == AW5808_MODE_USB)
        hid_close(aw->hid);
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
}

int aw5808_hid_fd(aw5808_t *aw)
{
    return hid_fd(aw->hid);
}

int aw5808_serial_fd(aw5808_t *aw)
{
    return serial_fd(aw->serial);
}

const char *aw5808_id(aw5808_t *aw)
{
    snprintf(aw->ident, sizeof(aw->ident), "%s %s", serial_id(aw->serial), hid_id(aw->hid));
    return aw->ident;
}

void aw5808_set_cbs(aw5808_t *aw, struct aw5808_cbs *cbs)
{
    aw->cbs = cbs;
}