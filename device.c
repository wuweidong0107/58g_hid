#include <stdio.h>
#include <pthread.h>
#include <ev.h>
#include <stdlib.h>

#include "device.h"
#include "ini.h"
#include "log.h"
#include "utils.h"

#define AW5808_MAX_NUM  (8)
static aw5808_t *aws[AW5808_MAX_NUM];

int devices_init(struct ev_loop *loop, const char *conf_file)
{
    char section[64] = {0};
    char key[64] = {0};
    int s, k;
    int aw_idx = 0;

   if (access(conf_file, R_OK) < 0)
      return -1;

    aw5808_options_t opt;
    for (s = 0; ini_getsection(s, section, sizearray(section), conf_file) > 0; s++) {
        if (!strncmp("aw5808", section, strlen("aw5808") && aw_idx < (AW5808_MAX_NUM-1))) {
            memset(&opt, 0, sizeof(opt));
            opt.loop = loop;
            for (k = 0; ini_getkey(section, k, key, sizearray(key), conf_file) > 0; k++) {
                if (!strncmp(key, "serial", strlen(key))) {
                    ini_gets(section, key, "dummy", opt.serial, sizearray(opt.serial), conf_file);
                } else if (!strncmp(key, "usb", strlen(key))) {
                    ini_gets(section, key, "dummy", opt.usb, sizearray(opt.usb), conf_file);
                } else if (!strncmp(key, "mode", strlen(key))) {
                    opt.mode = ini_getl(section, key, 0, conf_file);
                }
            }
            if ((aws[aw_idx] = aw5808_new()) == NULL) {
                log_error("aw5808[%d] new fail", aw_idx, opt.usb);
                continue ;
            }
            if (aw5808_open(aws[aw_idx], &opt) != 0) {
                log_error("aw5808[%d] open fail: %s", aw_idx, aw5808_errmsg(aws[aw_idx]));
                aw5808_free(aws[aw_idx]);
                aws[aw_idx] = NULL;
                continue;
            }
            aw_idx++;
        }
    }
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