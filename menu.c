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

static void cmd_write_cb(struct ev_loop *loop, struct ev_io *w, int revents)
{
}

static void cmd_read_cb(struct ev_loop *loop, struct ev_io *w, int revents)
{
}

int cmd_58g_readfw(int argc, char *argv[])
{
    int index = 0;
    if (argc == 2)
        index = strtoul(argv[1], NULL, 10);

    aw5808_t *aw = get_aw5808(index);
    if (aw == NULL)
        return -1;
    ev_io_start(loop, &conni->iow);
/*
    thpool_add_work(thpool, task_58g_readfw_handler, aw);
    return 0;
*/
}

int cmd_58g_read_id(int argc, char *argv[])
{
    return 0;
}