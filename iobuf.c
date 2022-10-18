#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "iobuf.h"

//#define DEBUG
#ifdef DEBUG
#define DBG(fmt, args...) do { fprintf(stderr, fmt,  ##args); } while(0)
#else
#define DBG(fmt, args...)   do {  } while(0)
#endif

#define min(x, y) (((x) < (y)) ? (x) : (y))
#define max(x, y) (((x) > (y)) ? (x) : (y))

static void memzero(volatile unsigned char *buf, size_t len)
{
    if (buf != NULL) {
        while(len--)
            *buf++ = 0;
    }
}

int iobuf_resize(struct iobuf *io, size_t new_size)
{
    int ok = 1;

    if (new_size % IOBUF_CHUNK_SIZE)
        new_size = (new_size / IOBUF_CHUNK_SIZE + 1) * IOBUF_CHUNK_SIZE;

    if (new_size == 0) {
        memzero(io->buf, io->cap);
        free(io->buf);
        io->buf = NULL;
        io->len = io->cap = 0;
    } else if (new_size != io->cap) {
        DBG("memory realloc: %ld ---> %ld\n", io->cap, new_size);
        void *p = calloc(1, new_size);
        if (p) {
            size_t len = min(new_size, io->len);
            if (len > 0) memmove(p, io->buf, len);
            memzero(io->buf, io->cap);
            free(io->buf);
            io->buf = (unsigned char *)p;
            io->cap = new_size;
        } else {
            ok = 0;
        }
    }
    return ok;
}

int iobuf_init(struct iobuf *io, size_t size)
{
    return iobuf_resize(io, size);
}

void iobuf_free(struct iobuf *io)
{
    iobuf_resize(io, 0);
}

void iobuf_dump(struct iobuf *io, size_t len)
{
    size_t i;
    size_t line = 16;

    if (len > io->cap)
        len = io->cap;

    for (i=0; i<len; i++) {
        if (!(i%16))
            DBG("%4lx  ", i);
        DBG("%02x ", io->buf[i]);
        
        if (!((i+1) % 8))
            DBG(" ");

        if (!((i+1) % line)) {
            int j = i - (line -1);
            DBG("|");
            for(; j<(i+1); j++) {
                DBG("%c", isalnum(io->buf[j]) ? io->buf[j] : '.');
            }
            DBG("|");
            DBG("\n");
        }
    }
    // printf("|");
    // for (i=0; i<len; i++)
    //     printf("%c ", io->buf[i]);
}

size_t iobuf_add(struct iobuf *io, size_t offset, const void *buf,
        size_t len)
{
    size_t new_cap = max((io->len + len), (offset + len));
    if (new_cap > io->cap) {
        new_cap += IOBUF_CHUNK_SIZE;
        new_cap -= new_cap % IOBUF_CHUNK_SIZE;
        if (!iobuf_resize(io, new_cap))
            return 0;
    }
    if (offset < io->len)
        memmove(io->buf + offset + len, io->buf + offset, io->len - offset);
    if (buf != NULL)
        memmove(io->buf + offset, buf, len);
    if (offset > io->len)
        io->len += offset - io->len;
    io->len += len;
    return len;
}

size_t iobuf_del(struct iobuf *io, size_t offset, size_t len)
{
    if (offset > io->len)
        return 0;
    if ((offset + len) > io->len)
        len = io->len - offset;
    
    memmove(io->buf + offset, io->buf + offset + len, io->len - offset - len);
    memzero(io->buf + io->len - len, len);
    io->len -= len;
    return len;
}