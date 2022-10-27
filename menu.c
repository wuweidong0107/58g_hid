#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <ev.h>
#include <readline/readline.h>
#include <unistd.h>
#include "log.h"
#include "menu.h"
#include "aw5808.h"
#include "serial.h"

#include "thpool.h"
#include "device.h"
#include "stdstring.h"

extern struct ev_loop *loop;

/*
void task_58g_readfw_handler(void *arg)
{
    aw5808_t *aw = (aw5808_t *) arg;
    uint8_t buf[2];
    
    aw5808_lock(aw);
    if (aw5808_read_fw(aw, buf, sizeof(buf)/sizeof(buf[0]), 10) == 2)
        shell_printf("fimware version: %02x %02x\n",buf[0], buf[1]);
    aw5808_unlock(aw);
}
*/

static command_t commands[];
static int cmd_help(int argc, char *argv[])
{
    int i=0;
    printf("Avaliable command:\n");
    for (; commands[i].name; i++) {
        char *tokens[1];
        string_split(commands[i].name, "_", tokens, 1);
        if ((!strncmp("aw5808", tokens[0], strlen("aw5808")) && get_aw5808(0) == NULL) 
            || (!strncmp("serial", tokens[0],strlen("serial")) && get_serial(0) == NULL)) {
            free(tokens[0]);
        } else {
            free(tokens[0]);
            printf("\t%-40s %s\n", commands[i].name, commands[i].doc);
        }
    }
    printf("Exit by Ctrl+D.\n");
    return 0;
}

static command_t commands[] = {
    { "aw5808_list", cmd_aw5808_list, "List all aw5808" },
    { "aw5808_getconfig [index]", cmd_aw5808_get_config, "Get aw5808 config" },
    { "aw5808_getrfstatus [index]", cmd_aw5808_get_rfstatus, "Get aw5808 RF status" },
    { "aw5808_pair [index]", cmd_aw5808_pair, "Pair aw5808 with headphone" },
    { "aw5808_setmode [index] <i2s|usb>", cmd_aw5808_set_mode, "Set aw5808 mode" },
    { "aw5808_seti2smode [index] <master|slave>", cmd_aw5808_set_i2s_mode, "Set aw5808 i2s mode" },
    { "aw5808_setconnmode [index] <multi|single>", cmd_aw5808_set_connect_mode, "Set aw5808 connect mode" },
    { "aw5808_setrfchannel [index] <1-8>", cmd_aw5808_set_rfchannel, "Set aw5808 RF channel" },
    { "aw5808_setrfpower [index] <1-16>", cmd_aw5808_set_rfpower, "Set aw5808 RF power" },
    { "serial_list", cmd_serial_list, "List all serial" },
    { "serial_send <index> <data1 data2 ...>", cmd_serial_send, "Send hex data by serial" },
    //{ "readid [index]", cmd_58g_read_id, "Read 5.8g operated ID" },
    { "help", cmd_help, "Disply help info" },
    { NULL, NULL, NULL},
};

static command_t *find_command(const char *name)
{
    int i;

    for (i=0; commands[i].name; i++) {
        char *tokens[2];
        size_t count;
        count = string_split(commands[i].name, " ", tokens, 2);
        if(strcmp(name, tokens[0]) == 0)
            return (&commands[i]);
        
        for(int j=0; j<count; j++)
            free(tokens[j]);
    }

    return NULL;
}

void shell_set_prompt(const char *string)
{
	rl_set_prompt(string);
	rl_redisplay();
}

void shell_printf(const char *fmt, ...)
{
	va_list args;
	bool save_input;
	char *saved_line;
	int saved_point;

	save_input = !RL_ISSTATE(RL_STATE_DONE);
	if (save_input) {
		saved_point = rl_point;
		saved_line = rl_copy_text(0, rl_end);
		rl_save_prompt();
		rl_replace_line("", 0);
		rl_redisplay();
	}

	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);

	if (save_input) {
		rl_restore_prompt();
		rl_replace_line(saved_line, 0);
		rl_point = saved_point;
		rl_forced_update_display();
		free(saved_line);
	}
}

void shell_exec(int argc, char *argv[])
{
    int ret;
    command_t *cmd = find_command(argv[0]);
    if (cmd) {
        ret = cmd->func(argc, argv);
        switch(ret) {
            case 0:
                break;
            case -ENOENT: 
                shell_printf("Unkown command!\n");
                cmd_help(0, NULL);
                break;
            case -EINVAL:
                shell_printf("Invalid command param\n");
                cmd_help(0, NULL);
                break;
            default:
                shell_printf("Command exec fail, ret=%d\n", ret);
        }
        return;
    }
    cmd_help(0, NULL);
    return;
}

static void on_aw5808_get_config(aw5808_t *aw, const uint8_t *data, int len)
{
    shell_printf("5.8G firmware version: %x %x\n", data[1], data[2]);
    shell_printf("MCU firmware version: %x\n", data[3]);
    shell_printf("Mode: %s\n", data[4] == AW5808_MODE_I2S ? "i2s":"usb");
    shell_printf("RF Channel: %d\n", data[5]);
    shell_printf("RF Power: %d\n", data[6]);    
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

static struct aw5808_cbs aw5808_menu_cbs = {
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

static int on_serial_receive(serial_t *serial, const uint8_t *buf, size_t len)
{
    int i;
    for (i=0; i<len; i++) {
        shell_printf("%i:%x\n",i, buf[i]);
    }
    return 0;
}

static struct serial_cbs serial_menu_cbs = {
    .on_receive = on_serial_receive,
};

int cmd_serial_list(int argc, char *argv[])
{
    int i;
    serial_t *aw;

    for (i=0; (aw=get_serial(i)) != NULL; i++)
        shell_printf("%d: %s\n", i, serial_id(aw));
    
    return 0;
}

int cmd_serial_send(int argc, char *argv[])
{
    int index;
    uint8_t data[128];
    int i,len;

    if (argc < 2)
        return -EINVAL;

    index = strtoul(argv[1], NULL, 10);
    for (i=2, len=0; i<argc && len<128; i++, len++) {
        data[len] = strtoul(argv[i], NULL, 16);
        printf("%d:%x\n", len, data[len]);
    }
    serial_t *serial = get_serial(index);
    if (serial == NULL)
        return -EINVAL;

    len = len + 1;
    if (serial_write(serial, data, len) != len)
        log_info("%s", serial_errmsg(serial));
    
    return 0;
}
void menu_init(void)
{
    int i;
    aw5808_t *aw;
    serial_t *serial;

    for (i=0; (aw=get_aw5808(i)) != NULL; i++) {
        aw5808_set_cbs(aw, &aw5808_menu_cbs);
    }

    for (i=0; (serial=get_serial(i)) != NULL; i++) {
        serial_set_cbs(serial, &serial_menu_cbs);
    }
}