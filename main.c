#include <stdio.h>
#include <stdlib.h>
#include <readline/readline.h>
#include <termios.h>
#include <unistd.h>
#include <wordexp.h>
#include <errno.h>
#include <stdbool.h>
#include <fcntl.h>

#include "log.h"
#include "stdstring.h"
#include "device.h"
#include "thpool.h"
#include "menu.h"

typedef int (*cmd_fn_t)(int argc, char *argv[]);
typedef struct {
    const char *name;
    cmd_fn_t func;
    const char *doc;
} command_t;

threadpool thpool;

static struct termios term;
static tcflag_t old_lflag;
static cc_t old_vtime;
static command_t commands[];

static int cmd_help(int argc, char *argv[])
{
    int i=0;
    printf("Avaliable command:\n");
    for (; commands[i].name; i++) {
        printf("\t%-20s %s\n", commands[i].name, commands[i].doc);
    }
    return 0;
}

static int cmd_exit(int argc, char *argv[])
{
    term.c_cflag = old_lflag;
    term.c_cc[VTIME] = old_vtime;

    if (tcsetattr(STDIN_FILENO, TCSANOW, &term) < 0) {
        log_error("tcsetattr()");
        exit(1);
    }
    exit(0);
}

static command_t commands[] = {
    { "readfw [index]", cmd_58g_readfw, "Read 5.8g firmware version" },
    //{ "readid [index]", cmd_58g_read_id, "Read 5.8g operated ID" },
    { "help", cmd_help, "Disply help info" },
    { "exit", cmd_exit, "Quit this menu" }, 
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
    int ret;

    if (line == NULL) {
        fprintf(stderr, "line is NULL\n");
        term.c_cflag = old_lflag;
        term.c_cc[VTIME] = old_vtime;
        if (tcsetattr(STDIN_FILENO, TCSANOW, &term) < 0) {
            log_error("tcsetattr()");
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

    ret = shell_exec(w.we_wordc, w.we_wordv);
    switch (ret) {
        case 0:
            break;
        case -ENOENT:            
            fprintf(stderr, "Unkown command\n\n");
            cmd_help(0, NULL);
            break;
        case -EINVAL:
            fprintf(stderr, "Invalid command param\n\n");
            cmd_help(0, NULL);
            break;
        default:
            fprintf(stderr, "Command exec fail\n");
    }
    wordfree(&w);
    free(line);
}

static int logger_init(const char *log_file, int verbose)
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
    logger_init(NULL, 0);

    if (devices_init() < 0) {
        log_error("devices_init() fail");
        exit(1);
    }

    thpool = thpool_init(4);
    if (thpool == NULL) {
        log_error("thpool_init() fail");
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

    static fd_set fds_read;
    int max_fd;
    while(1) {
        FD_ZERO(&fds_read);
        FD_SET(STDIN_FILENO, &fds_read);
        max_fd = STDIN_FILENO;

        if (select(max_fd+1, &fds_read, NULL, NULL, NULL) < 0) {
            log_error("select() fail");
            exit(1);
        }
        /* Receive user input */
        if (FD_ISSET(STDIN_FILENO, &fds_read)) {
            rl_callback_read_char();
        }
    }
    devices_exit();
    thpool_wait(thpool);
	thpool_destroy(thpool);
}