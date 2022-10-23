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
        printf("\t%-30s %s\n", commands[i].name, commands[i].doc);
    }
    printf("Exit by Ctrl+D.\n");
    return 0;
}

static command_t commands[] = {
    { "list", cmd_aw5808_list, "List all aw5808" },
    { "getconfig [index]", cmd_aw5808_get_config, "Get aw5808 config" },
    { "getrfstatus [index]", cmd_aw5808_get_rfstatus, "Get aw5808 RF status" },
    { "setmode [index] <i2s|usb>", cmd_aw5808_set_mode, "Set aw5808 mode" },
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
                shell_printf("Command exec fail, ret=%d\n\n", ret);
        }
        return;
    }
    cmd_help(0, NULL);
    return;
}

void on_aw5808_get_config(aw5808_t *aw, const uint8_t *data, int len)
{
    shell_printf("5.8G firmware version: %x %x\n", data[1], data[2]);
    shell_printf("MCU firmware version: %x\n", data[3]);
    shell_printf("Mode: %s\n", data[4] == AW5808_MODE_I2S ? "i2s":"usb");
    shell_printf("RF Channel: %d\n", data[5]);
    shell_printf("RF Power: %d\n", data[6]);    
}

void on_aw5808_get_rfstatus(aw5808_t *aw, uint8_t is_connected, uint8_t pair_status)
{
    shell_printf("5.8G link status: %s\n", is_connected ? "connected" : "disconnected");
    shell_printf("5.8G pair status: ");
    switch(pair_status) {
        case 0:
            shell_printf("exit pairing\n");
            break;
        case 1:
            shell_printf("pairing fail\n");
            break;
        case 2:
            shell_printf("pairing success\n");
            break;
        case 3:
            shell_printf("pairing\n");
            break;
    }
}

void on_aw5808_set_mode(aw5808_t *aw, int mode)
{
    switch(mode) {
        case AW5808_MODE_I2S:
            shell_printf("mode is i2s.\n");
            break;
        case AW5808_MODE_USB:
            shell_printf("mode is usb.\n");
            break;
        default:
            fprintf(stderr, "unknown mode(%x).\n", mode);
    }
}

static struct aw5808_cbs menu_cbs = {
    .on_get_config = on_aw5808_get_config,
    .on_get_rfstatus = on_aw5808_get_rfstatus,
    .on_set_mode = on_aw5808_set_mode,
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
    int index = 0;
    if (argc == 2)
        index = strtoul(argv[1], NULL, 10);

    aw5808_t *aw = get_aw5808(index);
    if (aw == NULL)
        return -1;

    return aw5808_get_config(aw);
}


int cmd_aw5808_get_rfstatus(int argc, char *argv[])
{
    int index = 0;
    if (argc == 2)
        index = strtoul(argv[1], NULL, 10);

    aw5808_t *aw = get_aw5808(index);
    if (aw == NULL)
        return -1;

    return aw5808_get_rfstatus(aw);
}

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
}

int cmd_aw5808_set_mode(int argc, char *argv[])
{
    int index, mode;
    if (argc == 2)
        index = 0;
    else if (argc == 3)
        index = strtoul(argv[1], NULL, 10);
    else
        return -1;
    
    if(!strncasecmp("i2s", argv[argc-1], 3))
        mode = AW5808_MODE_I2S;
    else if(!strncasecmp("usb", argv[argc-1], 3))
        mode = AW5808_MODE_USB;
    else
        return -1;
    
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