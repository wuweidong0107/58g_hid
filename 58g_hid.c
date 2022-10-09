#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "log.h"
#include "hid.h"
#include "58g_hid.h"

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

extern hid_t *hid[2];
int cmd_read_fw(int argc, char *argv[])
{
    hid_58g_data_t buf = {
        .rw = HID_58G_READ,
        .reg = 0x0,
        .len = 0x02,
        .data = {0},
    };
    
    int idx = 0;
    if (argc == 2)
        idx = atoi(argv[1]);
    
    if (idx >= (sizeof(hid) / sizeof(hid[0]) || hid[idx] == NULL)) {
        log_error("hid device not exist");
        return -1;
    }

    if (hid_write(hid[idx], (uint8_t *) &buf, sizeof(buf)) != sizeof(buf)) {
        log_error("hid_write() fail: %s", hid_errmsg(hid[idx]));
        return -1;
    }

    if (hid_read(hid[idx], (uint8_t *) &buf, sizeof(buf), 10) != sizeof(buf)) {
        log_error("hid_read() fail: %s", hid_errmsg(hid[idx]));
        return -1;
    }
    printf("fw version: %x%x\n", buf.data[0], buf.data[1]);
    return 0;
}

int cmd_read_id(int argc, char *argv[])
{
    return 0;
}