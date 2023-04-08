#ifndef __WS_SERVER_H__
#define __WS_SERVER_H__

#include "thpool.h"

int ws_server_init(threadpool thpool, const char *url);
void ws_server_exit(void);

#endif