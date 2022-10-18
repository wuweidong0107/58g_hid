#include <stdio.h>
#include <pthread.h>
#include <ev.h>

#include "main.h"
#include "device.h"
#include "log.h"

#define AW5808_MAX_NUM  (8)
static aw5808_t *aws[AW5808_MAX_NUM];

int devices_init(struct ev_loop *loop)
{
    // TODO: load config

    aw5808_options_t opt;
    opt.serial = "/dev/ttyS1";
    opt.usb_vid = 0x25a7;
    opt.usb_pid = 0x5804;
    opt.usb_name = NULL;
    opt.mode = AW5808_MODE_USB;

    if ((aws[0] = aw5808_new(loop)) == NULL) {
        log_error("aw5808_new() fail", opt.usb_name);
        return -1;
    }
    if (aw5808_open(aws[0], &opt) != 0) {
        log_error("aw5808_open() fail: %s", aw5808_errmsg(aws[0]));
        return -1;
    }
    
/*
    opt.serial = "dev/ttyS2";
    opt.usb_vid = 0x1234;
    opt.usb_pid = 0x5678;
    opt.usb_name = "name2";
    if ((aws[1] = aw5808_new()) == NULL) {
        log_error("aw5808_new() %s fail\n", opt.usb_name);
        return -1;
    }
    if (aw5808_open(aws[1], &opt) != 0) {
        log_error("aw5808_open() %s fail: %s", opt.usb_name, aw5808_errmsg(aws[1]));
        return -1;
    }
*/
    return 0;
}

void devices_exit(void)
{
    int i;
    for (i=0; i<AW5808_MAX_NUM; i++) {
        if (aws[i]) {
            aw5808_close(aws[i]);
            aw5808_free(aws[i]);
        }
    }
}

aw5808_t *get_aw5808(int index)
{
    if(index >= AW5808_MAX_NUM)
        return NULL;

    return aws[index];
}