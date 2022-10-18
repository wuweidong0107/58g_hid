#ifndef __CONFIG_H__
#define __CONFIG_H__

// Maximum size of the recv IO buffer
#ifndef MAX_RECV_BUF_SIZE
#define MAX_RECV_BUF_SIZE (1 * 1024 * 1024)
#endif

// Granularity of the send/recv IO buffer growth
#ifndef IO_SIZE
#define IO_SIZE 2048
#endif


#endif