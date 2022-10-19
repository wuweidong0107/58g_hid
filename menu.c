#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <ev.h>

#include "main.h"
#include "log.h"
#include "menu.h"
#include "aw5808.h"
#include "thpool.h"
#include "device.h"

extern struct ev_loop *loop;

/*
void task_58g_readfw_handler(void *arg)
{
    aw5808_t *aw = (aw5808_t *) arg;
    uint8_t buf[2];
    
    aw5808_lock(aw);
    if (aw5808_read_fw(aw, buf, sizeof(buf)/sizeof(buf[0]), 10) == 2)
        printf("fimware version: %02x %02x\n",buf[0], buf[1]);
    aw5808_unlock(aw);
}
*/

void on_aw5808_set_mode(aw5808_t *aw, const uint8_t *data, int len)
{
    switch(data[1]) {
        case AW5808_MODE_I2S:
            printf("i2s mode now.\n");
            break;
        case AW5808_MODE_USB:
            printf("usb mode now.\n");
            break;
        default:
            fprintf(stderr, "unknown mode(%x).\n", data[1]);
    }
}

static struct aw5808_cbs menu_cbs = {
    .on_set_mode = on_aw5808_set_mode,
};

int cmd_aw5808_get_fwver(int argc, char *argv[])
{
    int index = 0;
    if (argc == 2)
        index = strtoul(argv[1], NULL, 10);

    aw5808_t *aw = get_aw5808(index);
    if (aw == NULL)
        return -1;
    
    uint8_t buf[2];
    return aw5808_read_fw(aw, buf, sizeof(buf)/sizeof(buf[0]));
/*
    thpool_add_work(thpool, task_58g_readfw_handler, aw);
    return 0;
*/
}

int cmd_aw5808_set_mode(int argc, char *argv[])
{
    int index, mode;
    if (argc == 2) {
        index = 0;
        mode = strtoul(argv[1], NULL, 10);
    } else if (argc == 3) {
        index = strtoul(argv[1], NULL, 10);
        mode = strtoul(argv[2], NULL, 10);
    } else {
        return -1;
    }

    aw5808_t *aw = get_aw5808(index);
    if (aw == NULL)
        return -1;
    return aw5808_set_mode(aw, mode);
}

void menu_init(void)
{
    int i;
    aw5808_t *aw;

    for (i=0; (aw=get_aw5808(i)) !=NULL; i++) {
        aw5808_set_cbs(aw, &menu_cbs);
    }
}