#ifndef __IOBUF_H__
#define __IOBUF_H__

#include <stddef.h>

struct iobuf {
    unsigned char *buf;  // Point to data
    size_t len;          // Used size
    size_t cap;         // Total size
};

#define IOBUF_CHUNK_SIZE (4096)

int iobuf_init(struct iobuf *io, size_t size);
int iobuf_resize(struct iobuf *io, size_t new_size);
void iobuf_dump(struct iobuf *io, size_t len);
size_t iobuf_add(struct iobuf *io, size_t offset, const void *buf, size_t len);
size_t iobuf_del(struct iobuf *io, size_t offset, size_t len);
void iobuf_free(struct iobuf *io);

#endif