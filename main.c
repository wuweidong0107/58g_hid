#include <stdlib.h>
#include <readline/readline.h>
#include <termios.h>
#include <unistd.h>
#include <wordexp.h>
#include <errno.h>
#include <stdbool.h>

#include "log.h"
#include "hid.h"

typedef int (*cmd_fn_t)(int argc, char *argv[]);
typedef struct {
    char *name;
    cmd_fn_t func;
    char *doc;
} command_t;

static struct termios term;
static tcflag_t old_lflag;
static cc_t old_vtime;

static hid_t *hid;
static fd_set fds_read, fds_write;

enum cmd_58g_hid {
    CMD_NOP,
    CMD_READFW,
};

static enum cmd_58g_hid hid_cmd = CMD_NOP;

static int cmd_readfw(int argc, char *argv[]);
static int cmd_help(int argc, char *argv[]);
static int cmd_exit(int argc, char *argv[]);

static command_t commands[] = {
    { "readfw", cmd_readfw, "Read 5.8g FW version" },
    { "help", cmd_help, "Disply help info" },
    { "exit", cmd_exit, "Quit this menu" }, 
    { NULL, NULL, NULL},
};

static int cmd_readfw(int argc, char *argv[])
{
    hid_cmd = CMD_READFW;
    return 0;
}

static int cmd_help(int argc, char *argv[])
{
    int i=0;
    printf("Avaliable command:\n");
    for (; commands[i].name; i++) {
        printf("%-20s %s\n", commands[i].name, commands[i].doc);
    }
}

static int cmd_exit(int argc, char *argv[])
{
    term.c_cflag = old_lflag;
    term.c_cc[VTIME] = old_vtime;

    if (tcsetattr(STDIN_FILENO, TCSANOW, &term) < 0) {
        perror("tcsetattr");
        exit(1);
    }
    exit(0);
}

static command_t *find_command(const char *name)
{
    int i;
    for (i=0; commands[i].name; i++) {
        if(strcmp(name, commands[i].name) == 0)
            return (&commands[i]);
    }
    for (i=0; commands[i].name; i++) {
        if(strcmp("help", commands[i].name) == 0)
            return (&commands[i]);
    }
    return NULL;
}

static int shell_exec(int argc, char *argv[])
{
    int i=0;
    command_t *cmd = find_command(argv[0]);
    if (cmd) {
        return cmd->func(argc, argv);
    }
    return -ENOENT;
}

static void process_line(char *line)
{
    if (line == NULL) {
        fprintf(stderr, "line is NULL\n");
        term.c_cflag = old_lflag;
        term.c_cc[VTIME] = old_vtime;
        if (tcsetattr(STDIN_FILENO, TCSANOW, &term) < 0) {
            perror("tcsetattr");
            exit(1);
        }
        exit(0);
    }

    wordexp_t w;
	if (wordexp(line, &w, WRDE_NOCMD))
		return;

	if (w.we_wordc == 0) {
		wordfree(&w);
		return;
	}
    shell_exec(w.we_wordc, w.we_wordv);
    wordfree(&w);
    free(line);
}

static int init_logger(const char *log_file, int verbose)
{
    if (log_file == NULL) {
        log_set_quiet(0);
        log_set_level(verbose > 0 ? LOG_DEBUG : LOG_INFO);
    } else {
        log_set_quiet(1);
        FILE *fp;
        fp = fopen(log_file, "w");
        if(fp == NULL) {
            log_error("fopen() fail:%s\n", log_file);
            return -1;
        }
        log_add_fp(fp, verbose > 0 ? LOG_DEBUG : LOG_INFO);
    }
    return 0;
}

int main(void)
{
    init_logger(NULL, 0);

    hid_t *hid = hid_new();
    if (hid == NULL) {
        log_error("hid_new() fail");
        exit(1);
    }
    
    if (hid_open(hid, 0x06cb, 0xce44, NULL) != 0) {
        log_error("hid_open(): %s", hid_errmsg(hid));
        exit(1);
    }

    if (tcgetattr(STDIN_FILENO, &term) < 0) {
        log_error("tcgetattr() fail");
        exit(1);
    }
    old_lflag = term.c_cflag;
    old_vtime = term.c_cc[VTIME];
    term.c_cflag &= ~ICANON;
    term.c_cc[VTIME] = 1;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &term) < 0) {
        log_error("tcsetattr() fail");
        exit(1);
    }

    rl_callback_handler_install("58g_hid$ ", process_line);

    int max_fd = STDIN_FILENO;
    while(1) {
        FD_ZERO(&fds_read);
        FD_ZERO(&fds_write);
        FD_SET(STDIN_FILENO, &fds_read);
        FD_SET(hid_fd(hid), &fds_read);
        if (hid_cmd != CMD_NOP)
            FD_SET(hid_fd(hid), &fds_write);

        if (hid_fd(hid) > max_fd)
            max_fd = hid_fd(hid);

        if (select(max_fd+1, &fds_read, &fds_write, NULL, NULL) < 0) {
            log_error("select() fil");
            exit(1);
        }
        /* Receive user input */
        if (FD_ISSET(STDIN_FILENO, &fds_read)) {
            rl_callback_read_char();
        }

        if (FD_ISSET(hid_fd(hid), &fds_write)) {
            /* Send data to HID */
            hid_cmd = CMD_NOP;       
        }
    }
}