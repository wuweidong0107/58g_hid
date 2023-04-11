#include <stdlib.h>
#include <errno.h>
#include "log.h"
#include "device.h"
#include "aw5808.h"

static void on_ws_aw5808_get_config(aw5808_t *aw, uint16_t firmware_version, uint8_t mcu_verison,
        aw5808_mode_t mode, uint8_t rf_channel, uint8_t rf_power)
{
    log_info("5.8G firmware version: %x\n", firmware_version);
    log_info("MCU firmware version: %x\n", mcu_verison);
    log_info("Mode: %s\n", mode == AW5808_MODE_I2S ? "i2s":"usb");
    log_info("RF Channel: %d\n", rf_channel);
    log_info("RF Power: %d\n", rf_power);
}

static struct aw5808_client_ops ws_aw5808_ops = {
    .on_get_config = on_ws_aw5808_get_config,
};

static struct aw5808_client ws_aw5808 = {
    .name = "websocket aw5808",
    .ops = &ws_aw5808_ops,
};

int ws_aw5808_get_config(const char *json)
{
    int index = 0, ret;

    aw5808_t *aw = get_aw5808(index);
    if (aw == NULL)
        return -EINVAL;

    if ((ret = aw5808_get_config(aw)) != 0)
        log_info("%s", aw5808_errmsg(aw));
    
    return ret;
}

int ws_aw5808_init(void)
{
    int i, ret;
    aw5808_t *aw;

    for (i=0; (aw=get_aw5808(i)) != NULL; i++) {
        if ((ret = aw5808_add_client(aw, &ws_aw5808)))
            return ret;
    }
    return 0;
}

void ws_aw5808_exit(void)
{
    int i;
    aw5808_t *aw;

    for (i=0; (aw=get_aw5808(i)) != NULL; i++) {
        aw5808_remove_client(aw, &ws_aw5808);
    }
}