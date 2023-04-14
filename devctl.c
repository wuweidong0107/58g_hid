#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <ev.h>

#include "devctl.h"
#include "log.h"
#include "ws_server.h"
#include "device.h"
#include "thpool.h"
#include "shell.h"

threadpool thpool;
static struct ev_loop *loop;
static const char *ws_server_url = "ws://localhost:8000";

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
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "    devctl <-c config_file> [-l config file] <-m mode> [-r command]\n");
    fprintf(stderr, "       --config <filename>   Specify config file.\n");
    fprintf(stderr, "       --log <filename>      Log to file.\n");
    fprintf(stderr, "       --mode <mode>         Select run mode. 0: normal, 1: shell, 2: server\n");
    fprintf(stderr, "       --run <command>       Specify command in normal mode\n");
    fprintf(stderr, "    exmaple:\n");
    fprintf(stderr, "       devctl -c ./conf.d/devctl.conf -m 0 -r io\n");
}

int main(int argc, char *argv[])
{
    int c, option_index = 0;
    char *log_file = NULL;
    char *conf_file = "/etc/devctl.conf";
    char *command = NULL;
    int mode = -1;
    int log_level = LOG_INFO;

    struct option long_options[] = {
        {"config", required_argument, 0, 'c'},
        {"log", required_argument, 0, 'l'},
        {"mode", required_argument, 0, 'm'},
        {"run", required_argument, 0, 'r'},
        {"quiet", no_argument, 0, 'q'},
        {0, 0, 0, 0}
    };

    while ((c = getopt_long(argc, argv, "c:hl:m:qr:", long_options, &option_index)) != -1) {
        switch(c) {
            case 'c':
                conf_file = optarg;
                break;
            case 'l':
                log_file = optarg;
                break;
            case 'm':
                mode = atoi(optarg);
                break;
            case 'q':
                log_level = LOG_WARN;
                break;
            case 'h':
                show_help();
                return 0;
            case 'r':
                command = optarg;
                break;
            default:
                break;
        }
    }

    if (mode == -1 \
        || (mode == MODE_NORMAL && command == NULL)) {
        show_help();
        exit(1);
    }

    logger_init(log_file, 0);
    log_set_level(log_level);
    log_info("Build time: %s %s", __DATE__, __TIME__);
    log_info("Config file: %s", conf_file);

    /* init thread pool */
    thpool = thpool_init(4);
    if (thpool == NULL) {
        log_error("thpool_init() fail");
        exit(1);
    }

    /* setup async io */
    loop = ev_loop_new(EVBACKEND_EPOLL);
    ev_signal signal_watcher;
    ev_signal_init(&signal_watcher, signal_cb, SIGINT);
    ev_signal_start(loop, &signal_watcher);

    /* init hardware device */
    if (devices_init(loop, conf_file) < 0) {
        log_error("devices_init() fail");
        exit(1);
    }

    /* real work */    
    switch(mode) {
        case MODE_NORMAL:
            /* run a command */
            shell_init(loop, argc - optind, argv + optind, mode);
            shell_exec(command);
            shell_exit(loop, mode);
            break;
        case MODE_SHELL:
            /* setup menu */
            shell_init(loop, argc - optind, argv + optind, mode);
            shell_printf("");
            ev_run(loop, 0);
            shell_exit(loop, mode);
            break;
        case MODE_SERVER:
            /* setup websocket server */
            if (ws_server_init(thpool, ws_server_url) != 0) {
                log_error("websocket server start fail");
                exit(1);
            }
            log_info("Starting websocket listener on %s/websocket", ws_server_url);
            ev_run(loop, 0);
            ws_server_exit();
            break;
    }

    /* cleanup */
    log_info("Cleaning...");
    devices_exit();
    ev_loop_destroy(loop);
    thpool_wait(thpool);
    thpool_destroy(thpool);
    log_info("Bye!");
}