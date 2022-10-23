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

struct aw5808_handle {
    char ident[64];

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

static void aw5808_udev_read_cb(struct ev_loop *loop, struct ev_io *w, int revents)
{
    aw5808_t *aw = container_of(w, aw5808_t, udev_io.ior);
    struct udev_device *dev;
    log_debug("");
    dev = udev_monitor_receive_device(aw->mon);
    if (dev) {
#if 1
        printf("Subsystem=%s\n", udev_device_get_subsystem(dev));
        printf("ACTION=%s\n", udev_device_get_action(dev));
        printf("DEVNAME=%s\n", udev_device_get_sysname(dev));
        printf("DEVPATH=%s\n", udev_device_get_devpath(dev));
        printf("MACADDR=%s\n", udev_device_get_sysattr_value(dev, "address"));
        printf("DEVNODE=%s\n", udev_device_get_devnode(dev));
#endif
        if (strstr(udev_device_get_devpath(dev), "25A7:5804") != NULL) {
            if (!strcmp(udev_device_get_action(dev), "add")) {
                if(hid_open(aw->hid, NULL, AW5808_USB_VID, AW5808_USB_PID, aw->usb_name, aw->loop) != 0)
                    log_error("Opening hid in udev");
                aw->mode = AW5808_MODE_USB;
            } else if (!strcmp(udev_device_get_action(dev), "remove")) {
                hid_close(aw->hid);
                aw->mode = AW5808_MODE_I2S;
            }
        }
        udev_device_unref(dev);
    }
}

static int aw5808_serial_sendframe(aw5808_t *aw, const uint8_t *data, size_t data_len, bool sync)
{
    uint8_t frame[64]={0};
    size_t frame_len;

    memcpy(frame+3, data, data_len);
    frame_len = aw->codec_serial->encode(frame, data_len);
    if (frame_len <= 0)
        return -1; 

    if (sync) {
        if (serial_write_sync(aw->serial, frame, frame_len) != frame_len)
            return -1;
    } else {
        if (serial_write(aw->serial, frame, frame_len) != frame_len)
            return -1;
    }    
    return 0;
}

static int aw5808_on_serial_read(serial_t *serial, const uint8_t *buf, size_t len)
{
    aw5808_t *aw = serial_get_userdata(serial);
    const codec_t *codec_serial = aw->codec_serial;
    size_t used = 0;
	const uint8_t *data = NULL;
	size_t data_len, ret;

    for(;;) {
        ret = codec_serial->decode(buf, len, &data, &data_len);
        if (!data)
            break;
#if 1
        int i;
        printf("----------buf_len=%ld\n", len);
        for(i=0; i<len; i++) {
            printf("%02x ", buf[i]);
        }
        printf("\n----------data_len=%ld\n", data_len);
        for(i=0; i<data_len; i++) {
            printf("%02x ", data[i]);
        }
        printf("\n");
#endif

        used += ret;
        buf += ret;
        len -= ret;
        switch(data[0]) {
            case 0xD0:
                if (aw->cbs->on_get_config)
                    aw->cbs->on_get_config(aw, data, data_len);
                break;
            case 0xD1:
                if (aw->cbs->on_get_rfstatus && data_len == 0x02)
                    aw->cbs->on_get_rfstatus(aw, data[1]&0x1, (data[1]>>1 & 0x3));
                break;
            case 0xD4:
                if (aw->cbs->on_set_mode && data_len == 0x02)
                    aw->cbs->on_set_mode(aw, data[1]);
                break;
            default:
                break;
        }
    }
    return used;
}

static struct serial_cbs cbs = {
    .on_read = aw5808_on_serial_read,
};

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

int aw5808_open(aw5808_t *aw, aw5808_options_t *opt)
{   
    if(!opt->serial && !opt->usb)
        return _aw5808_error(aw, AW5808_ERROR_OPEN, 0, "No serial or usb specifed", opt->serial);

    if (opt->serial) {
        if (serial_open(aw->serial, opt->serial, 57600, &cbs, opt->loop) !=0) {
            return _aw5808_error(aw, AW5808_ERROR_OPEN, 0, "Openning aw5808 serial %s", opt->serial);
        }

        serial_set_userdata(aw->serial, aw);
        aw->codec_serial = get_codec("aw5808_serial");
        if (!aw->codec_serial)
            return _aw5808_error(aw, AW5808_ERROR_OPEN, 0, "Openning aw5808 get serial codec");

        if(aw5808_set_mode_sync(aw, opt->mode, 100))
            return _aw5808_error(aw, AW5808_ERROR_OPEN, 0, "Openning aw5808 set mode");
    }

    if (opt->usb) {
        strncpy(aw->usb_name, opt->usb, sizeof(aw->usb_name)-1);
        aw->mon = udev_monitor_new_from_netlink(aw->udev, "udev");
        udev_monitor_filter_add_match_subsystem_devtype(aw->mon, "hidraw", NULL);
        udev_monitor_enable_receiving(aw->mon);
        aw->udev_fd = udev_monitor_get_fd(aw->mon);
        aw->loop = opt->loop;
        ev_io_init(&aw->udev_io.ior, aw5808_udev_read_cb, aw->udev_fd, EV_READ);
        ev_io_start(aw->loop, &aw->udev_io.ior);
    
        /* if fail as hid not ready yet, will reopen it at udev callback. */
        if (hid_open(aw->hid, NULL, AW5808_USB_VID, AW5808_USB_PID, aw->usb_name, aw->loop))
            log_warn("hid not ready yet");
    }
    
    return 0;
}

void aw5808_close(aw5808_t *aw)
{
    hid_close(aw->hid);
    serial_close(aw->serial);
}

int aw5808_get_config(aw5808_t *aw)
{
    if (aw->serial) {
        uint8_t data[2] = {0x50, 0x0};
        size_t data_len = 2;
        if (aw5808_serial_sendframe(aw, data, data_len, 0) != 0)
            return _aw5808_error(aw, AW5808_ERROR_QUERY, 0, "Getting config");
        return 0;
    } else if (aw->hid) {
        // TODO
    }
    return -1;
}

int aw5808_get_rfstatus(aw5808_t *aw)
{
    if (aw->serial) {
        uint8_t data[2] = {0x51, 0x0};
        size_t data_len = 2;
        if (aw5808_serial_sendframe(aw, data, data_len, 0) != 0)
            return _aw5808_error(aw, AW5808_ERROR_QUERY, 0, "Getting RF status");
        return 0;
    } else if (aw->hid) {
        // TODO
    }
    return -1;
}

int aw5808_set_mode_sync(aw5808_t *aw, aw5808_mode_t mode, int timeout_us)
{
    uint8_t data[2] = {0x54, mode};
    size_t data_len = 2;
    uint8_t frame[64] = {0};
    size_t frame_len = 6;

    if (!aw->serial)
        return _aw5808_error(aw, AW5808_ERROR_CONFIGURE, 0, "Not support serial");

    if (aw5808_serial_sendframe(aw, data, data_len, 1) != 0)
        return _aw5808_error(aw, AW5808_ERROR_CONFIGURE, 0, "Setting mode write requeste");

    if (serial_read(aw->serial, frame, frame_len, timeout_us) != frame_len)
        return _aw5808_error(aw, AW5808_ERROR_CONFIGURE, 0, "Setting mode but no reply");

    if (frame[4] != mode)
        return _aw5808_error(aw, AW5808_ERROR_CONFIGURE, 0, "Setting mode not work");

    aw->mode = mode;
    return 0;
}

int aw5808_set_mode(aw5808_t *aw, aw5808_mode_t mode)
{
    uint8_t data[2] = {0x54, mode};
    size_t data_len = 2;

    if (!aw->serial)
        return _aw5808_error(aw, AW5808_ERROR_CONFIGURE, 0, "Not support serial");

    if (mode != AW5808_MODE_USB && mode != AW5808_MODE_I2S)
        return _aw5808_error(aw, AW5808_ERROR_CONFIGURE, 0, "Invalid mode");

    /* Already in request mode ? */
    if (mode == aw->mode) {
        aw->cbs->on_set_mode(aw, mode);
        return 0;
    }
    
    if (mode == AW5808_MODE_I2S && aw->mode == AW5808_MODE_USB)
        hid_close(aw->hid);
    
    if (aw5808_serial_sendframe(aw, data, data_len, 0) != 0)
        return _aw5808_error(aw, AW5808_ERROR_QUERY, 0, "Setting mode");

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

int aw5808_mode(aw5808_t *aw)
{
    return aw->mode;
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
    snprintf(aw->ident, sizeof(aw->ident)-1, "%s %s", serial_id(aw->serial), hid_id(aw->hid));
    return aw->ident;
}

void aw5808_set_cbs(aw5808_t *aw, struct aw5808_cbs *cbs)
{
    aw->cbs = cbs;
}