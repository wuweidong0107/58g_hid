#include <stdlib.h>
#include <errno.h>
#include "log.h"
#include "shell.h"
#include "aw5808.h"
#include "device.h"

static void on_aw5808_get_config(aw5808_t *aw, uint16_t firmware_version, uint8_t mcu_verison,
        aw5808_mode_t mode, uint8_t rf_channel, uint8_t rf_power)
{
    shell_printf("5.8G firmware version: %x\n", firmware_version);
    shell_printf("MCU firmware version: %x\n", mcu_verison);
    shell_printf("Mode: %s\n", mode == AW5808_MODE_I2S ? "i2s":"usb");
    shell_printf("RF Channel: %d\n", rf_channel);
    shell_printf("RF Power: %d\n", rf_power);
}

static void on_aw5808_get_rfstatus(aw5808_t *aw, uint8_t is_connected, uint8_t pair_status)
{
    char *pair_str[] = {
        "exit pairing",
        "pairing fail",
        "pairing success",
        "pairing"
    };
    shell_printf("5.8G link status: %s\n", is_connected ? "connected" : "disconnected");
    shell_printf("5.8G pair status: %s\n", pair_str[pair_status]);
}

static void on_aw5808_notify_rfstatus(aw5808_t *aw, uint8_t is_connected, uint8_t pair_status)
{
    on_aw5808_get_rfstatus(aw, is_connected, pair_status);
    aw5808_reply_rfstatus_notify(aw);
}

static void on_aw5808_pair(aw5808_t *aw)
{
    shell_printf("pair request send ok.");
}

static void on_aw5808_set_mode(aw5808_t *aw, aw5808_mode_t mode)
{
    switch(mode) {
        case AW5808_MODE_I2S:
            shell_printf("mode is i2s.\n");
            break;
        case AW5808_MODE_USB:
            shell_printf("mode is usb.\n");
            break;
        default:
            shell_printf("unknown mode(%x).\n", mode);
    }
}

static void on_aw5808_set_i2s_mode(aw5808_t *aw, aw5808_i2s_mode_t mode)
{
    switch(mode) {
        case AW5808_MODE_I2S_MASTER:
            shell_printf("i2s mode is master.\n");
            break;
        case AW5808_MODE_I2S_SLAVE:
            shell_printf("i2s mode is slave.\n");
            break;
        default:
            shell_printf("unknown i2s mode(%x).\n", mode);
    }
}

static void on_aw5808_set_connect_mode(aw5808_t *aw, aw5808_connect_mode_t mode)
{
    switch(mode) {
        case AW5808_MODE_CONN_MULTI:
            shell_printf("connect mode is multi.\n");
            break;
        case AW5808_MODE_CONN_SINGLE:
            shell_printf("connect mode is single.\n");
            break;
        default:
            shell_printf("unknown connect mode(%x).\n", mode);
    }
}

static void on_aw5808_set_rfchannel(aw5808_t *aw, uint8_t channel)
{
    shell_printf("rf channel is %d.\n", channel);
}

static void on_aw5808_set_rfpower(aw5808_t *aw, uint8_t power)
{
    shell_printf("rf power is %d.\n", power);
}

int cmd_aw5808_list(int argc, char *argv[])
{
    int i;
    aw5808_t *aw;

    for (i=0; (aw=get_aw5808(i)) != NULL; i++)
        shell_printf("%d: %s\n", i, aw5808_id(aw));
    
    return 0;
}

int cmd_aw5808_get_config(int argc, char *argv[])
{
    int index = 0, ret;
    if (argc == 2)
        index = strtoul(argv[1], NULL, 10);

    aw5808_t *aw = get_aw5808(index);
    if (aw == NULL)
        return -EINVAL;

    if ((ret = aw5808_get_config(aw)) != 0)
        log_info("%s", aw5808_errmsg(aw));
    
    return ret;
}


int cmd_aw5808_get_rfstatus(int argc, char *argv[])
{
    int index = 0, ret;
    if (argc == 2)
        index = strtoul(argv[1], NULL, 10);

    aw5808_t *aw = get_aw5808(index);
    if (aw == NULL)
        return -EINVAL;

    if ((ret = aw5808_get_rfstatus(aw)) != 0)
        log_info("%s", aw5808_errmsg(aw));
    
    return ret;
}

int cmd_aw5808_pair(int argc, char *argv[])
{
    int index = 0, ret;
    if (argc == 2)
        index = strtoul(argv[1], NULL, 10);

    aw5808_t *aw = get_aw5808(index);
    if (aw == NULL)
        return -EINVAL;

    if ((ret = aw5808_pair(aw)) != 0)
        log_info("%s", aw5808_errmsg(aw));

    return ret;
}

int cmd_aw5808_set_mode(int argc, char *argv[])
{
    int index, mode, ret;
    if (argc == 2)
        index = 0;
    else if (argc == 3)
        index = strtoul(argv[1], NULL, 10);
    else
        return -EINVAL;
    
    if(!strncasecmp("i2s", argv[argc-1], 3))
        mode = AW5808_MODE_I2S;
    else if(!strncasecmp("usb", argv[argc-1], 3))
        mode = AW5808_MODE_USB;
    else
        return -EINVAL;
    
    aw5808_t *aw = get_aw5808(index);
    if (aw == NULL)
        return -EINVAL;

    if ((ret = aw5808_set_mode(aw, mode)) != 0)
        log_info("%s", aw5808_errmsg(aw));

    return ret;
}

int cmd_aw5808_set_i2s_mode(int argc, char *argv[])
{
    int index, mode, ret;
    if (argc == 2)
        index = 0;
    else if (argc == 3)
        index = strtoul(argv[1], NULL, 10);
    else
        return -EINVAL;
    
    if(!strncasecmp("master", argv[argc-1], 3))
        mode = AW5808_MODE_I2S_MASTER;
    else if(!strncasecmp("slave", argv[argc-1], 3))
        mode = AW5808_MODE_I2S_SLAVE;
    else
        return -EINVAL;
    
    aw5808_t *aw = get_aw5808(index);
    if (aw == NULL)
        return -EINVAL;

    if ((ret = aw5808_set_i2s_mode(aw, mode)) != 0)
        log_info("%s", aw5808_errmsg(aw));
    
    return ret;
}

int cmd_aw5808_set_connect_mode(int argc, char *argv[])
{
    int index, mode, ret;
    if (argc == 2)
        index = 0;
    else if (argc == 3)
        index = strtoul(argv[1], NULL, 10);
    else
        return -EINVAL;
    
    if(!strncasecmp("multi", argv[argc-1], 3))
        mode = AW5808_MODE_CONN_MULTI;
    else if(!strncasecmp("single", argv[argc-1], 3))
        mode = AW5808_MODE_CONN_SINGLE;
    else
        return -EINVAL;
    
    aw5808_t *aw = get_aw5808(index);
    if (aw == NULL)
        return -EINVAL;

    if ((ret = aw5808_set_connect_mode(aw, mode)) != 0)
        log_info("%s", aw5808_errmsg(aw));
    
    return ret;
}

int cmd_aw5808_set_rfchannel(int argc, char *argv[])
{
    int index, channel, ret;
    if (argc == 2) {
        index = 0;
        channel = strtoul(argv[1], NULL, 10);
    } else if (argc == 3) {
        index = strtoul(argv[1], NULL, 10);
        channel = strtoul(argv[2], NULL, 10);
    } else {
        return -EINVAL;
    }

    if (channel < 1 || channel > 8)
        return -EINVAL;

    aw5808_t *aw = get_aw5808(index);
    if (aw == NULL)
        return -EINVAL;

    if ((ret = aw5808_set_rfchannel(aw, channel)) != 0)
        log_info("%s", aw5808_errmsg(aw));
    
    return ret;
}

int cmd_aw5808_set_rfpower(int argc, char *argv[])
{
    int index, power, ret;
    if (argc == 2) {
        index = 0;
        power = strtoul(argv[1], NULL, 10);
    } else if (argc == 3) {
        index = strtoul(argv[1], NULL, 10);
        power = strtoul(argv[2], NULL, 10);
    } else {
        return -EINVAL;
    }

    if (power < 1 || power > 16)
        return -EINVAL;

    aw5808_t *aw = get_aw5808(index);
    if (aw == NULL)
        return -EINVAL;

    if ((ret = aw5808_set_rfpower(aw, power)) != 0)
        log_info("%s", aw5808_errmsg(aw));
    
    return ret;
}

static help(void)
{
    shell_printf("Usage: aw5808 <opr> [args]");
    shell_printf("  Available opr: select_device <index>, default 0");
    shell_printf("                 hid_set_channel <channel>");
    shell_printf("                 hid_set_mute <mute>");
    shell_printf("                 hid_get_status");
    shell_printf("                 uart_select_mode <mode>");
    shell_printf("                 uart_set_channel <channel>");
    shell_printf("                 uart_req_pair");
    shell_printf("  Available args: mode, usb / i2s");
    shell_printf("                  channel, 1 ~ 8");
    shell_printf("                  mute, 0 ~ 1");
    shell_printf("                  freqmode, fix / auto");
    shell_printf("  Example:  aw5808 hid_set_channel 4");
    shell_printf("            aw5808 hid_set_mute 0");
    shell_printf("            aw5808 hid_get_status");
    shell_printf("            aw5808 uart_set_channel");
    shell_printf("            aw5808 uart_set_freqmode");
    shell_printf("            aw5808 uart_get_config");
    shell_printf("            aw5808 uart_get_rfstatus");

}

static struct aw5808_client_ops menu_aw5808_ops = {
    .on_get_config = on_aw5808_get_config,
    .on_get_rfstatus = on_aw5808_get_rfstatus,
    .on_notify_rfstatus = on_aw5808_notify_rfstatus,
    .on_pair = on_aw5808_pair,
    .on_set_mode = on_aw5808_set_mode,
    .on_set_i2s_mode = on_aw5808_set_i2s_mode,
    .on_set_connect_mode = on_aw5808_set_connect_mode,
    .on_set_rfchannel = on_aw5808_set_rfchannel,
    .on_set_rfpower = on_aw5808_set_rfpower,
};

static struct aw5808_client menu_aw5808 = {
    .name = "menu aw5808",
    .ops = &menu_aw5808_ops,
};

int cmd_aw5808(int argc, char *argv[])
{
    int i;

    for(i=0; i<argc; i++) {
        printf("%d: %s\n", i, argv[i]);
    }
    return 0;
}

int menu_aw5808_init(void)
{
    int i, ret;
    aw5808_t *aw;

    for (i=0; (aw=get_aw5808(i)) != NULL; i++) {
        if ((ret = aw5808_add_client(aw, &menu_aw5808)))
            return ret;
    }
    return 0;
}

void menu_aw5808_exit(void)
{
    int i;
    aw5808_t *aw;

    for (i=0; (aw=get_aw5808(i)) != NULL; i++) {
        aw5808_remove_client(aw, &menu_aw5808);
    }
}