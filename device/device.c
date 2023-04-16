#include <stdio.h>
#include <pthread.h>
#include <ev.h>
#include <stdlib.h>

#include "log.h"
#include "utils.h"
#include "device.h"
#include "ini.h"
#include "usb.h"
#include "aw5808.h"
#include "serial.h"
#include "wifi.h"

#define DEVICE_MAX_NUM  (8)

static aw5808_t *aw5808_array[DEVICE_MAX_NUM];
static serial_t *serial_array[DEVICE_MAX_NUM];
static usb_t *usb_array[DEVICE_MAX_NUM];
static wifi_t *wifi_array[DEVICE_MAX_NUM];
static int aw5808_idx, serial_idx, usb_idx, wifi_idx;

static bool device_aw5808_init(struct ev_loop *loop, const char *conf_file, const char *section)
{
    char key[64] = {0};
    int k;
    aw5808_options_t opt;

    memset(&opt, 0, sizeof(opt));
    opt.loop = loop;
    for (k = 0; ini_getkey(section, k, key, sizearray(key), conf_file) > 0; k++) {
        if (!strncmp(key, "serial", strlen("serial"))) {
            ini_gets(section, key, "dummy", opt.serial, sizearray(opt.serial), conf_file);
        } else if (!strncmp(key, "usb", strlen("usb"))) {
            ini_gets(section, key, "dummy", opt.usb, sizearray(opt.usb), conf_file);
        } else if (!strncmp(key, "mode", strlen("mode"))) {
            opt.mode = ini_getl(section, key, 0, conf_file);
        }
    }
    if ((aw5808_array[aw5808_idx] = aw5808_new()) == NULL) {
        log_error("aw5808[%d] new fail", aw5808_idx, opt.usb);
        return false;
    }
    if (aw5808_open(aw5808_array[aw5808_idx], &opt) != 0) {
        log_error("aw5808[%d] open fail: %s", aw5808_idx, aw5808_errmsg(aw5808_array[aw5808_idx]));
        aw5808_free(aw5808_array[aw5808_idx]);
        aw5808_array[aw5808_idx] = NULL;
        return false;
    }
    aw5808_idx++;
    return true;
}

static bool device_serial_init(struct ev_loop *loop, const char *conf_file, const char *section)
{
    char key[64] = {0};
    int k;
    serial_options_t opt;

    memset(&opt, 0, sizeof(opt));
    for (k = 0; ini_getkey(section, k, key, sizearray(key), conf_file) > 0; k++) {
        if (!strncmp(key, "path", strlen("path"))) {
            ini_gets(section, key, "dummy", opt.path, sizearray(opt.path), conf_file);
        } else if (!strncmp(key, "baudrate", strlen("baudrate"))) {
            opt.baudrate = ini_getl(section, key, 0, conf_file);
        }
    }
    if ((serial_array[serial_idx] = serial_new()) == NULL) {
        log_error("serial[%d] new fail", serial_idx);
        return false;
    }
    if (serial_open(serial_array[serial_idx], opt.path, opt.baudrate, loop) != 0) {
        log_error("serial[%d] open fail: %s", serial_idx, serial_errmsg(serial_array[serial_idx]));
        serial_free(serial_array[serial_idx]);
        serial_array[serial_idx] = NULL;
        return false;
    }
    serial_idx++;
    return true;
}

static bool device_usb_init(struct ev_loop *loop, const char *conf_file, const char *section)
{
    char key[64] = {0};
    int k;

    usb_options_t opt;
    memset(&opt, 0, sizeof(opt));
    for (k = 0; ini_getkey(section, k, key, sizearray(key), conf_file) > 0; k++) {
        if (!strncmp(key, "path", strlen("path"))) {
            ini_gets(section, key, "dummy", opt.path, sizearray(opt.path), conf_file);
        } else if (!strncmp(key, "vid", strlen("vid"))) {
            opt.vid = ini_getl(section, key, 0, conf_file);
        } else if (!strncmp(key, "pid", strlen("pid"))) {
            opt.pid = ini_getl(section, key, 0, conf_file);
        }
    }
    if ((usb_array[usb_idx] = usb_new()) == NULL) {
        log_error("usb[%d] new fail", usb_idx);
        return false;
    }
    if (usb_open(usb_array[usb_idx], opt.vid, opt.pid, opt.path) != 0) {
        log_error("usb[%d] open fail: %s", usb_idx, usb_errmsg(usb_array[usb_idx]));
        usb_free(usb_array[usb_idx]);
        usb_array[usb_idx] = NULL;
        return false;
    }
    usb_idx++;
    return true;
}

static bool device_wifi_init(struct ev_loop *loop, const char *conf_file, const char *section)
{
    if ((wifi_array[wifi_idx] = wifi_new()) == NULL) {
        log_error("wifi[%d] new fail", wifi_idx);
        return false;
    }
    if (wifi_open(wifi_array[wifi_idx], NULL) != 0) {
        log_error("wifi[%d] open fail: %s", wifi_idx, wifi_errmsg(wifi_array[wifi_idx]));
        wifi_free(wifi_array[wifi_idx]);
        wifi_array[wifi_idx] = NULL;
        return false;
    }
    wifi_idx++;
    return true;
}

int devices_init(struct ev_loop *loop, const char *conf_file)
{
    char section[64] = {0};
    int s;

    if (access(conf_file, R_OK) < 0) {
        log_error("config file not exist");
        return -1;
    }

    if (usb_init()) {
        log_error("usb init fail");
        return -1;
    }

    for (s = 0; ini_getsection(s, section, sizearray(section), conf_file) > 0; s++) {
        char *end = strchr(section, '/');
        int section_len = strlen(section);
        if (end != NULL)
            section_len = end - section;
        if (!strncmp(section, "aw5808", section_len) && aw5808_idx < DEVICE_MAX_NUM) {
            device_aw5808_init(loop, conf_file, section);
        } else if (!strncmp(section, "serial", section_len) && serial_idx < DEVICE_MAX_NUM) {
            device_serial_init(loop, conf_file, section);
        } else if (!strncmp(section, "usb", section_len) && usb_idx < DEVICE_MAX_NUM) {
            device_usb_init(loop, conf_file, section);
        } else if (!strncmp(section, "wifi", section_len) && wifi_idx < DEVICE_MAX_NUM) {
            device_wifi_init(loop, conf_file, section);
        }
    }
    return 0;
}

void devices_exit(void)
{
    int i;
    for (i=0; i<aw5808_idx; i++) {
        if (aw5808_array[i]) {
            aw5808_close(aw5808_array[i]);
            aw5808_free(aw5808_array[i]);
        }
    }

    for (i=0; i<serial_idx; i++) {
        if (serial_array[i]) {
            serial_close(serial_array[i]);
            serial_free(serial_array[i]);
        }
    }

    for (i=0; i<usb_idx; i++) {
        if (usb_array[i]) {
            usb_close(usb_array[i]);
            usb_free(usb_array[i]);
        }
    }

    for (i=0; i<wifi_idx; i++) {
        if (wifi_array[i]) {
            wifi_close(wifi_array[i]);
            wifi_free(wifi_array[i]);
        }
    }

    usb_exit();
}

aw5808_t *get_aw5808(int index)
{
    if(index >= aw5808_idx)
        return NULL;

    return aw5808_array[index];
}

serial_t *get_serial(int index)
{
    if(index >= serial_idx)
        return NULL;

    return serial_array[index];
}

usb_t *get_usb(int index)
{
    if(index >= usb_idx)
        return NULL;

    return usb_array[index];
}

wifi_t *get_wifi(int index)
{
    if(index >= wifi_idx)
        return NULL;

    return wifi_array[index];
}