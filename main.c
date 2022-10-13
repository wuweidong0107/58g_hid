#include <stdio.h>
#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <unistd.h>
#include <wordexp.h>
#include <errno.h>
#include <stdbool.h>
#include <fcntl.h>
#include <ev.h>

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
static struct ev_loop *loop;
static command_t commands[];

static int cmd_help(int argc, char *argv[])
{
    int i=0;
    printf("Avaliable command:\n");
    for (; commands[i].name; i++) {
        printf("\t%-20s %s\n", commands[i].name, commands[i].doc);
    }
    printf("Exit by Ctrl+D.\n");
    return 0;
}

static command_t commands[] = {
    { "readfw [index]", cmd_58g_readfw, "Read 5.8g firmware version" },
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
        ev_break(loop, EVBREAK_ALL);
        rl_callback_handler_remove();
    } else {
        if (*line != '\0')
            add_history(line);
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
                fprintf(stderr, "Unkown command!\n\n");
                cmd_help(0, NULL);
                break;
            case -EINVAL:
                fprintf(stderr, "Invalid command param\n\n");
                cmd_help(0, NULL);
                break;
            default:
                fprintf(stderr, "Command exec fail\n\n");
        }
        wordfree(&w);
        free(line);
    }
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

static void stdin_cb(struct ev_loop *loop, struct ev_io *w, int revents)
{
    rl_callback_read_char();
}

static void signal_cb(struct ev_loop *loop, ev_signal *w, int revents)
{
    if (w->signum == SIGINT) {
        ev_break(loop, EVBREAK_ALL);
        log_info("Normal quit\n");
    }
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

    fcntl(fileno(stdin), F_SETFL, O_NONBLOCK);
    rl_callback_handler_install("Myshell$ ", (rl_vcpfunc_t*) &process_line);

    // setup libev
    loop = EV_DEFAULT;
    ev_io stdin_watcher;
    ev_io_init(&stdin_watcher, stdin_cb, fileno(stdin), EV_READ);
    ev_io_start(loop, &stdin_watcher);
    ev_signal signal_watcher;
    ev_signal_init(&signal_watcher, signal_cb, SIGINT);
    ev_signal_start(loop, &signal_watcher);
    ev_loop(loop, 0);
    ev_loop_destroy(loop);

    devices_exit();
    thpool_wait(thpool);
	thpool_destroy(thpool);
    printf("Bye!");
}