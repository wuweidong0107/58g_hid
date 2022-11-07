#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <unistd.h>
#include <wordexp.h>
#include <errno.h>
#include <stdbool.h>
#include <fcntl.h>
#include <ev.h>

#include "log.h"
#include "device.h"
#include "thpool.h"
#include "shell.h"

threadpool thpool;
static struct ev_loop *loop;
static void process_line(char *line)
{
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
        
        shell_exec(w.we_wordc, w.we_wordv);
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
        rl_callback_handler_remove();
    }
}

static void show_help(void)
{
	fprintf(stderr, "Usage:\n" );
    fprintf(stderr, "  --config <filename>   Specify config file\n" );
    fprintf(stderr, "  --log <filename>      Log to file\n" );
}

int main(int argc, char *argv[])
{
    int c, option_index = 0;
    char *log_file = NULL;
    char *conf_file = "/etc/devctl.conf";

    struct option long_options[] = {
        {"config", required_argument, 0, 'c'},
        {"log", required_argument, 0, 'l'},
        {0, 0, 0, 0}
    };

    while ((c = getopt_long(argc, argv, "c:hl:", long_options, &option_index)) != -1) {
        switch(c) {
            case 'c':
                conf_file = optarg;
                break;
            case 'l':
                log_file = optarg;
                break;
            case 'h':
                show_help();
                return 0;
            case '?':
                shell_printf("unknown option: %c\n", optopt);
                return 1;
        }
    }
    logger_init(log_file, 0);

    thpool = thpool_init(4);
    if (thpool == NULL) {
        log_error("thpool_init() fail");
        exit(1);
    }

    /* setup async io */
    loop = ev_loop_new(EVBACKEND_EPOLL);
    ev_io stdin_watcher;
    ev_io_init(&stdin_watcher, stdin_cb, fileno(stdin), EV_READ);
    ev_io_start(loop, &stdin_watcher);
    ev_signal signal_watcher;
    ev_signal_init(&signal_watcher, signal_cb, SIGINT);
    ev_signal_start(loop, &signal_watcher);

    if (devices_init(loop, conf_file) < 0) {
        log_error("devices_init() fail");
        exit(1);
    }

    /* setup menu */
    shell_init();
    fcntl(fileno(stdin), F_SETFL, O_NONBLOCK);
    rl_callback_handler_install(NULL, (rl_vcpfunc_t*) &process_line);
    rl_set_prompt(PROMPT_ON);
    shell_printf("Build time: %s %s\n", __DATE__, __TIME__);
    shell_printf("Config file: %s\n", conf_file);
    ev_run(loop, 0);

    /* cleanup */
    devices_exit();
    ev_io_stop(loop, &stdin_watcher);
    ev_loop_destroy(loop);
    thpool_wait(thpool);
    thpool_destroy(thpool);
    log_info("Bye!");
}