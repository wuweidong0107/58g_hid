#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <pthread.h>
#include <libudev.h>
#include <ev.h>

#include "list.h"
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
} __attribute__((packed)) hidraw_packet_t;

struct aw5808_handle {
    char ident[128];
    /* io */
    struct ev_loop *loop;
    serial_t *serial;
    const codec_t *codec_serial;
    char usb_name[128];
    hidraw_t *hidraw;
    const codec_t *codec_hidraw;
    /* device config */
    aw5808_mode_t mode;                 /* i2s or usb */
    aw5808_i2s_mode_t i2s_mode;         /* master or slave */
    aw5808_connect_mode_t conn_mode;    /* multi or single */
    uint8_t rf_channel;
    uint8_t rf_power;
    /* hotplug */
    int udev_fd;
    struct io_channel udev_io;
    struct udev *udev;
    struct udev_monitor *mon;
    
    struct list_head clients;
    /* error handle */
    struct {
        int c_errno;
        char errmsg[96];
    } error;
};

static int _error(aw5808_t *aw, int code, int c_errno, const char *fmt, ...)
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

static void udev_read_cb(struct ev_loop *loop, struct ev_io *w, int revents)
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
                if(hidraw_open(aw->hidraw, NULL, AW5808_USB_VID, AW5808_USB_PID, aw->usb_name, aw->loop) != 0)
                    log_error("Opening hidraw in udev");
                aw->mode = AW5808_MODE_USB;
            } else if (!strcmp(udev_device_get_action(dev), "remove")) {
                hidraw_close(aw->hidraw);
                aw->mode = AW5808_MODE_I2S;
            }
        }
        udev_device_unref(dev);
    }
}

static void handle_get_config(aw5808_t *aw, const uint8_t *data, uint32_t data_len)
{
    uint16_t firmware_version = (data[0]<<8) + data[1];
    uint8_t mcu_verison = data[2];
    aw5808_mode_t mode = data[3];
    uint8_t rf_channel = data[4];
    uint8_t rf_power = data[5];
    struct aw5808_client *client;

    list_for_each_entry(client, &aw->clients, list) {
        if (client->ops->on_get_config)
            client->ops->on_get_config(aw, firmware_version, mcu_verison, mode, rf_channel, rf_power);
    }
}

static void handl_get_rfstatus(aw5808_t *aw, const uint8_t *data, uint32_t data_len)
{
    struct aw5808_client *client;
    list_for_each_entry(client, &aw->clients, list) {
        if (client->ops->on_get_rfstatus && data_len ==1)
            client->ops->on_get_rfstatus(aw, data[0]&0x1, (data[0]>>1 & 0x3));
    }
}

static void handle_notify_rfstatus(aw5808_t *aw, const uint8_t *data, uint32_t data_len)
{
    struct aw5808_client *client;
    list_for_each_entry(client, &aw->clients, list) {
        if (client->ops->on_notify_rfstatus && data_len == 1)
            client->ops->on_notify_rfstatus(aw, data[0]&0x1, (data[0]>>1 & 0x3));
    }
}

static void handle_pair(aw5808_t *aw)
{
    struct aw5808_client *client;
    list_for_each_entry(client, &aw->clients, list) {
        if (client->ops->on_pair)
            client->ops->on_pair(aw);
    }
}

static void handle_set_mode(aw5808_t *aw, const uint8_t *data, uint32_t data_len)
{
    struct aw5808_client *client;
    list_for_each_entry(client, &aw->clients, list) {
        if (client->ops->on_set_mode && data_len == 1)
            client->ops->on_set_mode(aw, data[0]);
    }
}

static void handle_set_i2s_mode(aw5808_t *aw, const uint8_t *data, uint32_t data_len)
{
    struct aw5808_client *client;
    list_for_each_entry(client, &aw->clients, list) {
        if (client->ops->on_set_mode && data_len == 1)
            client->ops->on_set_mode(aw, data[0]);
    }
}

static void handle_set_connect_mode(aw5808_t *aw, const uint8_t *data, uint32_t data_len)
{
    struct aw5808_client *client;
    list_for_each_entry(client, &aw->clients, list) {
        if (client->ops->on_set_mode && data_len == 1)
            client->ops->on_set_mode(aw, data[0]);
    }
}

static void handle_set_rfchannel(aw5808_t *aw, const uint8_t *data, uint32_t data_len)
{
    struct aw5808_client *client;
    list_for_each_entry(client, &aw->clients, list) {
        if (client->ops->on_set_mode && data_len == 1)
            client->ops->on_set_mode(aw, data[0]);
    }
}

static void handle_set_rfpower(aw5808_t *aw, const uint8_t *data, uint32_t data_len)
{
    struct aw5808_client *client;
    list_for_each_entry(client, &aw->clients, list) {
        if (client->ops->on_set_mode && data_len == 1)
            client->ops->on_set_mode(aw, data[0]);
    }
}

static int serial_sendframe(aw5808_t *aw, const uint8_t *data, size_t data_len, bool sync)
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

static int on_serial_receive(serial_t *serial, const uint8_t *buf, size_t len)
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
#if 0
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
                handle_get_config(aw, data+1, data_len-1);
                break;
            case 0xD1:
                handl_get_rfstatus(aw, data+1, data_len-1);
                break;
            case 0x52:
                handle_notify_rfstatus(aw, data+1, data_len-1);
                break;
            case 0xD3:
                handle_pair(aw);
                break;
            case 0xD4:
                handle_set_mode(aw, data+1, data_len-1);
                break;
            case 0xD5:
                handle_set_i2s_mode(aw, data+1, data_len-1);
                break;
            case 0xD6:
                handle_set_connect_mode(aw, data+1, data_len-1);
                break;
            case 0xD7:
                handle_set_rfchannel(aw, data+1, data_len-1);
                break;
            case 0xD8:
                handle_set_rfpower(aw, data+1, data_len-1);
                break;
            default:
                break;
        }
    }
    return used;
}

static struct serial_client_ops serial_client_aw5808_ops = {
    .on_receive = on_serial_receive,
};

static struct serial_client serial_client_aw5808 = {
    .name = "aw5808 serial",
    .ops = &serial_client_aw5808_ops,
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

    aw->hidraw = hidraw_new();
    if (!aw->hidraw)
        goto fail;

	/* create udev object */
    aw->udev = udev_new();
    if (!aw->udev)
        goto fail;
    INIT_LIST_HEAD(&aw->clients);
    return aw;
fail:
    aw5808_free(aw);
    return NULL;
}

void aw5808_free(aw5808_t *aw)
{
    udev_unref(aw->udev);

    if (aw->hidraw)
        hidraw_free(aw->hidraw);
    if (aw->serial)
        serial_free(aw->serial);
    if (aw)
        free(aw);
}

int aw5808_open(aw5808_t *aw, aw5808_options_t *opt)
{   
    if(!opt->serial && !opt->usb)
        return _error(aw, AW5808_ERROR_OPEN, 0, "No serial or usb specifed", opt->serial);

    bool is_serial_init = false;
    if (opt->serial) {
        if (serial_open(aw->serial, opt->serial, 57600, opt->loop) !=0) {
            return _error(aw, AW5808_ERROR_OPEN, 0, "Openning aw5808 serial %s", opt->serial);
        }

        serial_add_client(aw->serial, &serial_client_aw5808);
        serial_set_userdata(aw->serial, aw);
        aw->codec_serial = get_codec("aw5808_serial");
        if (!aw->codec_serial)
            return _error(aw, AW5808_ERROR_OPEN, 0, "Openning aw5808 get serial codec");

        if(aw5808_set_mode_sync(aw, opt->mode, 100))
            return _error(aw, AW5808_ERROR_OPEN, 0, "Openning aw5808 set mode");
        is_serial_init = true;
    }

    if (opt->usb) {
        strncpy(aw->usb_name, opt->usb, sizeof(aw->usb_name)-1);
        aw->mon = udev_monitor_new_from_netlink(aw->udev, "udev");
        udev_monitor_filter_add_match_subsystem_devtype(aw->mon, "hidraw", NULL);
        udev_monitor_enable_receiving(aw->mon);
        aw->udev_fd = udev_monitor_get_fd(aw->mon);
        aw->loop = opt->loop;
        ev_io_init(&aw->udev_io.ior, udev_read_cb, aw->udev_fd, EV_READ);
        ev_io_start(aw->loop, &aw->udev_io.ior);
    
        /* if fail as hidraw not ready yet, will reopen it at udev callback. */
        if (hidraw_open(aw->hidraw, NULL, AW5808_USB_VID, AW5808_USB_PID, aw->usb_name, aw->loop)) {
            log_warn("hidraw not ready yet");
            if (is_serial_init == false)
                return _error(aw, AW5808_ERROR_OPEN, 0, "Neither usb / uart is wokring");
        }
    }
    
    aw->i2s_mode = AW5808_MODE_I2S_UNKNOWN;
    aw->conn_mode = AW5808_MODE_CONN_UNKNOWN;
    return 0;
}

void aw5808_close(aw5808_t *aw)
{
    hidraw_close(aw->hidraw);
    serial_close(aw->serial);
}

int aw5808_get_config(aw5808_t *aw)
{
    if (aw->serial) {
        uint8_t data[2] = {0x50, 0x0};
        size_t data_len = 2;
        if (serial_sendframe(aw, data, data_len, 0) == 0)
            return 0;
    } else if (aw->hidraw) {
        // TODO
    }
    return _error(aw, AW5808_ERROR_QUERY, 0, "Getting config");
}

int aw5808_get_rfstatus(aw5808_t *aw)
{
    if (aw->serial) {
        uint8_t data[2] = {0x51, 0x0};
        size_t data_len = 2;
        if (serial_sendframe(aw, data, data_len, 0) == 0)
            return 0;
    } else if (aw->hidraw) {
        // TODO
    }
    return _error(aw, AW5808_ERROR_QUERY, 0, "Getting RF status");
}

int aw5808_reply_rfstatus_notify(aw5808_t *aw)
{
    if (aw->serial) {
        uint8_t data[2] = {0xD2, 0xFF};
        size_t data_len = 2;
        if (serial_sendframe(aw, data, data_len, 0) == 0)
            return 0;
    } else if (aw->hidraw) {
        // TODO
    }
    return _error(aw, AW5808_ERROR_QUERY, 0, "Getting RF status");
}

int aw5808_pair(aw5808_t *aw)
{
    if (aw->serial) {
        uint8_t data[2] = {0x53, 0xFF};
        size_t data_len = 2;
        if (serial_sendframe(aw, data, data_len, 0) == 0)
            return 0;
    } else if (aw->hidraw) {
        // TODO
    }
    return _error(aw, AW5808_ERROR_CONFIGURE, 0, "Pairing");
}

int aw5808_set_mode_sync(aw5808_t *aw, aw5808_mode_t mode, int timeout_us)
{
    uint8_t data[2] = {0x54, mode};
    size_t data_len = 2;
    uint8_t frame[64] = {0};
    size_t frame_len = 6;

    if (!aw->serial)
        return _error(aw, AW5808_ERROR_CONFIGURE, 0, "Not support serial");

    if (serial_sendframe(aw, data, data_len, 1) != 0)
        return _error(aw, AW5808_ERROR_CONFIGURE, 0, "Setting mode write requeste");

    if (serial_read(aw->serial, frame, frame_len, timeout_us) != frame_len)
        return _error(aw, AW5808_ERROR_CONFIGURE, 0, "Setting mode but no reply");

    if (frame[4] != mode)
        return _error(aw, AW5808_ERROR_CONFIGURE, 0, "Setting mode not work");

    aw->mode = mode;
    return 0;
}

int aw5808_set_mode(aw5808_t *aw, aw5808_mode_t mode)
{
    uint8_t data[2] = {0x54, mode};
    size_t data_len = 2;

    if (mode != AW5808_MODE_USB && mode != AW5808_MODE_I2S)
        return _error(aw, AW5808_ERROR_CONFIGURE, 0, "Invalid mode");

    /* Already in request mode ? */
    if (mode == aw->mode) {
        struct aw5808_client *client;
        list_for_each_entry(client, &aw->clients, list) {
            if (client->ops->on_set_mode)
                client->ops->on_set_mode(aw, mode);
        }
        return 0;
    }
    
    if (aw->serial) {
        if (mode == AW5808_MODE_I2S && aw->mode == AW5808_MODE_USB)
            hidraw_close(aw->hidraw);
        
        if (serial_sendframe(aw, data, data_len, 0) == 0)
            return 0;
    }

    return _error(aw, AW5808_ERROR_CONFIGURE, 0, "Setting mode");
}

int aw5808_set_i2s_mode(aw5808_t *aw, aw5808_i2s_mode_t mode)
{
    uint8_t data[2] = {0x55, mode};
    size_t data_len = 2;

    if (mode != AW5808_MODE_I2S_MASTER && mode != AW5808_MODE_I2S_SLAVE)
        return _error(aw, AW5808_ERROR_CONFIGURE, 0, "Invalid i2s mode");

    if (aw->mode != AW5808_MODE_I2S)
        return _error(aw, AW5808_ERROR_CONFIGURE, 0, "Not working in i2s mode");

    /* Already in request mode ? */
    if (mode == aw->i2s_mode) {
        struct aw5808_client *client;
        list_for_each_entry(client, &aw->clients, list) {
            if (client->ops->on_set_i2s_mode)
                client->ops->on_set_i2s_mode(aw, mode);
        }
        return 0;
    }

    if (aw->serial) {
        if (serial_sendframe(aw, data, data_len, 0) == 0)
            return 0;
    }
    return _error(aw, AW5808_ERROR_QUERY, 0, "Setting i2s mode");
}

int aw5808_set_connect_mode(aw5808_t *aw, aw5808_connect_mode_t mode)
{
    uint8_t data[2] = {0x56, mode};
    size_t data_len = 2;

    if (mode != AW5808_MODE_CONN_MULTI && mode != AW5808_MODE_CONN_SINGLE)
        return _error(aw, AW5808_ERROR_CONFIGURE, 0, "Invalid connect mode");

    /* Already in request mode ? */
    if (mode == aw->conn_mode) {
        struct aw5808_client *client;
        list_for_each_entry(client, &aw->clients, list) {
            if (client->ops->on_set_connect_mode)
                client->ops->on_set_connect_mode(aw, mode);
        }
        return 0;
    }
    
    if (aw->serial) {
        if (serial_sendframe(aw, data, data_len, 0) == 0)
            return 0;
    }

    return _error(aw, AW5808_ERROR_CONFIGURE, 0, "Setting connect mode");
}

/* Support serial and hidraw */
int aw5808_set_rfchannel(aw5808_t *aw, uint8_t channel)
{
    uint8_t data[2] = {0x57, channel};
    size_t data_len = 2;

    if (channel < 1 || channel > 8)
        return _error(aw, AW5808_ERROR_CONFIGURE, 0, "Invalid channel");

    /* Already in request channel ? */
    if (channel == aw->rf_channel) {
        struct aw5808_client *client;
        list_for_each_entry(client, &aw->clients, list) {
            if (client->ops->on_set_rfchannel)
                client->ops->on_set_rfchannel(aw, channel);
        }
        return 0;
    }

    if (aw->serial) {
        if (serial_sendframe(aw, data, data_len, 0) == 0)
            return 0;
    } else if (aw->hidraw) {
        //TODO
    }
    return _error(aw, AW5808_ERROR_CONFIGURE, 0, "Setting RF channel");
}

int aw5808_set_rfpower(aw5808_t *aw, uint8_t power)
{
    uint8_t data[2] = {0x58, power};
    size_t data_len = 2;

    if (power < 1 || power > 16)
        return _error(aw, AW5808_ERROR_CONFIGURE, 0, "Invalid power");

    /* Already in request power ? */
    if (power == aw->rf_power) {
        struct aw5808_client *client;
        list_for_each_entry(client, &aw->clients, list) {
            if (client->ops->on_set_rfpower)
                client->ops->on_set_rfpower(aw, power);
        }
        return 0;
    }

    if (aw->serial) {
        if (serial_sendframe(aw, data, data_len, 0) == 0)
            return 0;
    } else if (aw->hidraw) {
        //TODO
    }
    return _error(aw, AW5808_ERROR_CONFIGURE, 0, "Setting RF power");
}

int aw5808_read_fw(aw5808_t *aw, uint8_t *buf, size_t len)
{
    if (len < 2) {
        return _error(aw, AW5808_ERROR_ARG, 0, "Firmware version len too short");
    }

    hidraw_packet_t pkt = {
        .rw = HID_58G_READ,
        .reg = 0x0,
        .len = 0x02,
        .data = {0},
    };
    
    if (hidraw_write(aw->hidraw, (uint8_t *) &pkt, sizeof(pkt)) != sizeof(pkt)) {
        return _error(aw, AW5808_ERROR_ARG, 0, "aw5808 hidraw writing");
    }
    return 0;
}

int aw5808_add_client(aw5808_t *aw, struct aw5808_client *client)
{
    if (!client || !client->ops)
        return -1;
    printf("%s: %s\n", __func__, client->name);
    list_add_tail(&client->list, &aw->clients);
    return 0;
}

void aw5808_remove_client(aw5808_t *aw, struct aw5808_client *client)
{
    printf("%s: %s\n", __func__, client->name);
    if (!client)
        return;
    list_del(&client->list);
}

int aw5808_mode(aw5808_t *aw)
{
    return aw->mode;
}

int aw5808_hidraw_fd(aw5808_t *aw)
{
    return hidraw_fd(aw->hidraw);
}

int aw5808_serial_fd(aw5808_t *aw)
{
    return serial_fd(aw->serial);
}

const char *aw5808_id(aw5808_t *aw)
{
    snprintf(aw->ident, sizeof(aw->ident)-1, "%s %s", serial_id(aw->serial), hidraw_id(aw->hidraw));
    return aw->ident;
}