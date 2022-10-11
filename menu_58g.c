#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>

#include "log.h"
#include "menu_58g.h"
#include "aw5808.h"
#include "task.h"

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

typedef struct {
    aw5808_t *aw;
    int argc;
    char **argv;
} aw5808_context_t;

/*
int cmd_58g_open(int argc, char *argv[])
{
    if (argc != 3)
        return -EINVAL;
    
    if (aw5808 != NULL) {
        aw5808_close(aw5808);
        aw5808_free(aw5808);
    }

    if ((aw5808 = aw5808_new()) == NULL) {
        log_error("aw5808_new() fail\n");
        return EXIT_FAILURE;
    }

    char *end;
    uint16_t vid, pid;
    vid = strtoul(argv[1], &end, 16);
    pid = strtoul(argv[2], &end, 16);

    aw5808_options_t opt = {
        .serial = "/dev/ttyS1",
        .usb_vid = vid,
        .usb_pid = pid,
        .usb_name = NULL,
    };
    
    if (aw5808_open(aw5808, &opt) != 0) {
        log_error("aw5808_open() fail: %s", aw5808_errmsg(aw5808));
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
*/

int cmd_58g_read_fw(int argc, char *argv[])
{
    aw5808_context_t ctx = {
        .argc = argc,
        .argv = argv,

    };
/*
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
*/
    return 0;
}

int cmd_58g_read_id(int argc, char *argv[])
{
    return 0;
}