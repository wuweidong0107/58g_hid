#include <stdio.h>
#include <pthread.h>
#include <ev.h>
#include <stdlib.h>

#include "device.h"
#include "ini.h"
#include "aw5808.h"
#include "serial.h"
#include "log.h"
#include "utils.h"

#define DEVICE_MAX_NUM  (8)
static aw5808_t *aw_array[DEVICE_MAX_NUM];
static serial_t *serial_array[DEVICE_MAX_NUM];

int devices_init(struct ev_loop *loop, const char *conf_file)
{
    char section[64] = {0};
    char key[64] = {0};
    char val[64]={0};
    int s, k;
    int aw_idx = 0;
    int serial_idx = 0;

   if (access(conf_file, R_OK) < 0)
      return -1;

    for (s = 0; ini_getsection(s, section, sizearray(section), conf_file) > 0; s++) {
        if (!strncmp(section, "aw5808", strlen("aw5808") && aw_idx < DEVICE_MAX_NUM)) {
            aw5808_options_t opt;
            memset(&opt, 0, sizeof(opt));
            opt.loop = loop;
            for (k = 0; ini_getkey(section, k, key, sizearray(key), conf_file) > 0; k++) {
                if (!strncmp(key, "serial", strlen(key))) {
                    ini_gets(section, key, "dummy", opt.serial, sizearray(opt.serial), conf_file);
                } else if (!strncmp(key, "usb", strlen("usb"))) {
                    ini_gets(section, key, "dummy", opt.usb, sizearray(opt.usb), conf_file);
                } else if (!strncmp(key, "mode", strlen("mode"))) {
                    opt.mode = ini_getl(section, key, 0, conf_file);
                }
            }
            if ((aw_array[aw_idx] = aw5808_new()) == NULL) {
                log_error("aw5808[%d] new fail", aw_idx, opt.usb);
                continue ;
            }
            if (aw5808_open(aw_array[aw_idx], &opt) != 0) {
                log_error("aw5808[%d] open fail: %s", aw_idx, aw5808_errmsg(aw_array[aw_idx]));
                aw5808_free(aw_array[aw_idx]);
                aw_array[aw_idx] = NULL;
                continue;
            }
            aw_idx++;
        } else if (!strncmp(section, "serial", strlen("serial") && serial_idx < DEVICE_MAX_NUM)) {
            serial_options_t opt;
            memset(&opt, 0, sizeof(opt));

            for (k = 0; ini_getkey(section, k, key, sizearray(key), conf_file) > 0; k++) {
                memset(val, 0, sizeof(val));
                if (!strncmp(key, "path", strlen("path"))) {
                    ini_gets(section, key, "dummy", opt.path, sizearray(opt.path), conf_file);
                } else if (!strncmp(key, "baudrate", strlen("baudrate"))) {
                    opt.baudrate = ini_getl(section, key, 0, conf_file);
                }
            }
            if ((serial_array[serial_idx] = serial_new()) == NULL) {
                log_error("serial[%d] new fail", serial_idx);
                continue ;
            }
            if (serial_open(serial_array[serial_idx], opt.path, opt.baudrate, loop) != 0) {
                log_error("serial[%d] open fail: %s", serial_idx, serial_errmsg(serial_array[serial_idx]));
                serial_free(serial_array[serial_idx]);
                serial_array[serial_idx] = NULL;
                continue;
            }
            serial_idx++;
        }
    }
    return 0;
}

void devices_exit(void)
{
    int i;
    for (i=0; i<DEVICE_MAX_NUM; i++) {
        if (aw_array[i]) {
            aw5808_close(aw_array[i]);
            aw5808_free(aw_array[i]);
        }
    }

    for (i=0; i<DEVICE_MAX_NUM; i++) {
        if (serial_array[i]) {
            serial_close(serial_array[i]);
            serial_free(serial_array[i]);
        }
    }
}

aw5808_t *get_aw5808(int index)
{
    if(index >= DEVICE_MAX_NUM)
        return NULL;

    return aw_array[index];
}

serial_t *get_serial(int index)
{
    if(index >= DEVICE_MAX_NUM)
        return NULL;

    return serial_array[index];
}