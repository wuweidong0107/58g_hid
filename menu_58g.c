#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>

#include "log.h"
#include "menu_58g.h"
#include "aw5808.h"
#include "task.h"
#include "thpool.h"

extern threadpool thpool;

typedef struct {
    int argc;
    char **argv;
} cmd_t;

void callback_58g_read_fw()
{

}
void task_58g_read_fw(void *arg)
{
}

int cmd_58g_read_fw(int argc, char *argv[])
{
    cmd_t cmd = {
        .argc = argc,
        .argv = argv,
    };
    task_t task = {
        .handler = task_58g_read_fw,
        .data = &cmd,
        .callback = callback_58g_read_fw,
        .cb_data = NULL,
    };
    
    thpool_push_task(thpool, &task);
    return 0;
}

int cmd_58g_read_id(int argc, char *argv[])
{
    return 0;
}