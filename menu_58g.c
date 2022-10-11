#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "log.h"
#include "menu_58g.h"
#include "aw5808.h"

enum hid_58g_rw {
    HID_58G_WRITE = 0x01,
    HID_58G_READ = 0x02,
};

typedef struct {
    uint8_t rw;     // 1: write, 2: read
    uint8_t reg;    // reg address
    uint8_t len;    // data length
    uint8_t data[61];
} __attribute__((packed)) hid_58g_data_t;

static aw5808_t *aw = NULL;

int cmd_58g_open(int argc, char *argv[])
{
    if (argc != 3)
        return -1;

    if (hid != NULL)
        hid_free(hid);

    hid = hid_new();
    if (hid == NULL)
        return -1;
    
    int i=0;
    for(i=0; i<argc; i++)
        printf("%s\n", argv[i]);
    
    char *end;
    unsigned short vid, pid;
    vid = strtoul(argv[1], &end, 16);
    pid = strtoul(argv[2], &end, 16);

    aw5808_options_t opt = {
        .serial = "/dev/ttyS1",
        .usb_vid = ,
        .usb_pid = ,
        .usb_name = NULL,
    };
}

int cmd_58g_read_fw(int argc, char *argv[])
{
    hid_58g_data_t buf = {
        .rw = HID_58G_READ,
        .reg = 0x0,
        .len = 0x02,
        .data = {0},
    };
    
    if (hid_write(hid, (uint8_t *) &buf, sizeof(buf)) != sizeof(buf)) {
        log_error("hid_write() fail: %s", hid_errmsg(hid));
        return -1;
    }

    if (hid_read(hid, (uint8_t *) &buf, sizeof(buf), 10) != sizeof(buf)) {
        log_error("hid_read() fail: %s", hid_errmsg(hid));
        return -1;
    }
    printf("fw version: %x%x\n", buf.data[0], buf.data[1]);
    return 0;
}

int cmd_58g_read_id(int argc, char *argv[])
{
    return 0;
}