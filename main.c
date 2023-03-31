#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <ev.h>

#include "log.h"
#include "device.h"
#include "thpool.h"
#include "shell.h"

threadpool thpool;
static struct ev_loop *loop;

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

static void signal_cb(struct ev_loop *loop, ev_signal *w, int revents)
{
    if (w->signum == SIGINT) {
        ev_break(loop, EVBREAK_ALL);
    }
}

static void show_help(void)
{
    fprintf(stderr, "Usage:\n" );
    fprintf(stderr, "  --config <filename>   Specify config file.\n" );
    fprintf(stderr, "  --log <filename>      Log to file.\n" );
    fprintf(stderr, "  -y                    Disable interactive mode.\n");
}

int main(int argc, char *argv[])
{
    int c, option_index = 0;
    char *log_file = NULL;
    char *conf_file = "/etc/devctl.conf";
    bool mode = true;

    struct option long_options[] = {
        {"config", required_argument, 0, 'c'},
        {"log", required_argument, 0, 'l'},
        {0, 0, 0, 0}
    };

    while ((c = getopt_long(argc, argv, "c:hl:y", long_options, &option_index)) != -1) {
        printf("c=%c\n", c);
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
            case 'y':
                mode = false;
                break;
            default:
                break;
        }
        if (c == 'y')
            break;
    }
    logger_init(log_file, 0);

    thpool = thpool_init(4);
    if (thpool == NULL) {
        log_error("thpool_init() fail");
        exit(1);
    }

    if (devices_init(loop, conf_file) < 0) {
        log_error("devices_init() fail");
        exit(1);
    }

    /* setup async io */
    loop = ev_loop_new(EVBACKEND_EPOLL);
    ev_signal signal_watcher;
    ev_signal_init(&signal_watcher, signal_cb, SIGINT);
    ev_signal_start(loop, &signal_watcher);

    /* setup menu */
    shell_init(loop, argc - optind, argv + optind, mode);
    shell_printf("Build time: %s %s\n", __DATE__, __TIME__);
    shell_printf("Config file: %s\n", conf_file);
    shell_run(loop);

    /* cleanup */
    shell_exit(loop);
    devices_exit();
    ev_loop_destroy(loop);
    thpool_wait(thpool);
    thpool_destroy(thpool);
    log_info("Bye!");
}