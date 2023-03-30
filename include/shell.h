#ifndef __SHELL_H__
#define __SHELL_H__

#include <ev.h>
#define COLOR_OFF	"\001\x1B[0m\002"
#define COLOR_BLUE	"\001\x1B[0;94m\002"
#define PROMPT_ON	COLOR_BLUE "[devctl]" COLOR_OFF "# "
#define PROMPT_OFF	""

/* for thread job */
struct cmd_context {
    int argc;
    char **argv;
};

void shell_printf(const char *fmt, ...);
int shell_init(struct ev_loop *loop, int argc, char **argv, bool mode);
int shell_run(struct ev_loop *loop);
void shell_exit(struct ev_loop *loop);
#endif