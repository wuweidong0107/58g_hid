#ifndef __DEVICE_H__
#define __DEVICE_H__

#include "aw5808.h"
#include "serial.h"
#include "usb.h"

int devices_init(struct ev_loop *loop, const char *conf_file);
void devices_exit(void);
aw5808_t *get_aw5808(int index);
serial_t *get_serial(int index);
usb_t *get_usb(int index);

#endif