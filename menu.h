#ifndef __MENU_H__
#define __MENU_H__

#define COLOR_OFF	"\001\x1B[0m\002"
#define COLOR_BLUE	"\001\x1B[0;94m\002"
#define PROMPT_ON	COLOR_BLUE "[devctl]" COLOR_OFF "# "
#define PROMPT_OFF	""

typedef int (*cmd_fn_t)(int argc, char *argv[]);
typedef struct {
    const char *name;
    cmd_fn_t func;
    const char *doc;
} command_t;

void shell_set_prompt(const char *string);
void shell_exec(int argc, char *argv[]);

int cmd_aw5808_list(int argc, char *argv[]);
int cmd_aw5808_get_config(int argc, char *argv[]);
int cmd_aw5808_get_rfstatus(int argc, char *argv[]);
int cmd_aw5808_pair(int argc, char *argv[]);
int cmd_aw5808_set_mode(int argc, char *argv[]);
int cmd_aw5808_set_i2s_mode(int argc, char *argv[]);
int cmd_aw5808_set_connect_mode(int argc, char *argv[]);
int cmd_aw5808_set_rfchannel(int argc, char *argv[]);
int cmd_aw5808_set_rfpower(int argc, char *argv[]);

int cmd_serial_list(int argc, char *argv[]);
int cmd_serial_send(int argc, char *argv[]);

void menu_init(void);
#endif