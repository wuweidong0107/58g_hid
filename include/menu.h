#ifndef __MENU_H__
#define __MENU_H__

#define COLOR_OFF	"\001\x1B[0m\002"
#define COLOR_BLUE	"\001\x1B[0;94m\002"
#define PROMPT_ON	COLOR_BLUE "[devctl]" COLOR_OFF "# "
#define PROMPT_OFF	""

void shell_exec(int argc, char *argv[]);
void menu_init(void);
#endif