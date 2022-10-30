#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <linux/input.h>
#include <linux/hidraw.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <glob.h>
#include <stdint.h>
#include <unistd.h>
#include <poll.h>
#include <limits.h>

#define DEBUG
#include "config.h"
#include "hidraw.h"
#include "log.h"
#include "io_channel.h"
#include "iobuf.h"
#include "utils.h"

struct hidraw_handle {
    char ident[64];
    int fd;
    struct ev_loop *loop;
    struct io_channel io;
    struct {
        int c_errno;
        char errmsg[256];
    } error;
};

static int _error(hidraw_t *hidraw, int code, int c_errno, const char *fmt, ...)
{
    va_list ap;

    hidraw->error.c_errno = c_errno;

    va_start(ap, fmt);
    vsnprintf(hidraw->error.errmsg, sizeof(hidraw->error.errmsg), fmt, ap);
    va_end(ap);

    if (c_errno) {
        char buf[64];
        strerror_r(c_errno, buf, sizeof(buf));
        snprintf(hidraw->error.errmsg+strlen(hidraw->error.errmsg), sizeof(hidraw->error.errmsg)-strlen(hidraw->error.errmsg), ": %s [errno %d]", buf, c_errno);
    }

    return code;
}

const char* hidraw_id(hidraw_t *hidraw)
{
    return hidraw->ident;
}

const char *hidraw_errmsg(hidraw_t *hidraw)
{
    return hidraw->error.errmsg;
}

int hidraw_errno(hidraw_t *hidraw)
{
    return hidraw->error.c_errno;
}

int hidraw_fd(hidraw_t *hidraw)
{
    return hidraw->fd;
}

hidraw_t *hidraw_new()
{
    hidraw_t *hidraw = calloc(1, sizeof(hidraw_t));
    if (hidraw == NULL)
        return NULL;

    hidraw->fd = -1;
    return hidraw;
}

void hidraw_free(hidraw_t *hidraw)
{
    free(hidraw);
}

static void _hidraw_write_cb(struct ev_loop *loop, struct ev_io *w, int revents)
{
    hidraw_t *hidraw = container_of(w, hidraw_t, io.iow);
    struct iobuf *wbuf = &hidraw->io.wbuf;
    uint8_t *buf = wbuf->buf;
    size_t len = wbuf->len;
    ssize_t n;
    ssize_t remain = len;
    bool nonblock = fd_is_nonblock(hidraw->fd);

    //iobuf_dump(wbuf, wbuf->len);
    do {
        n = write(hidraw->fd, buf, remain);
        if (unlikely(n < 0)) {
            if (errno == EINTR)
                continue;

            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == ENOTCONN)
                break;
            
            log_error("Writing data %s", strerror(errno));
            return;
        }
        remain -= n;
        iobuf_del(wbuf, 0, (size_t) n);
    } while (remain && nonblock);

    if (wbuf->len == 0)
        ev_io_stop(hidraw->loop, w);
}

static void _hidraw_read_cb(struct ev_loop *loop, struct ev_io *w, int revents)
{
    hidraw_t *hidraw = container_of(w, hidraw_t, io.ior);
    bool nonblock = fd_is_nonblock(hidraw->fd);
    struct iobuf *rbuf = &hidraw->io.rbuf;
    ssize_t remain = INT_MAX;
    ssize_t ret;

    do {
        size_t want;
        uint8_t *buf = &rbuf->buf[rbuf->len];

        if ((rbuf->cap - rbuf->len) < IO_SIZE &&
            !iobuf_resize(rbuf, rbuf->cap + IO_SIZE)) {
            log_error("hidraw recv buf too small");
            return;
        }

        want = rbuf->cap - rbuf->len;
        if (want > remain)
            want = remain;

        ret = read(hidraw->fd, buf, want);
        if (unlikely(ret < 0)) {
            if (errno == EINTR)
                continue;

            if (errno == EAGAIN || errno == ENOTCONN)
                break;
            log_error("hidraw read: %s", strerror(errno));
            return;
        }
        if (ret == 0)
            break;
        rbuf->len += ret;
        remain -= ret;
    } while (remain && nonblock);
    
    //iobuf_dump(rbuf, rbuf->len);
/*
    if(hidraw->cbs->on_read) {
        int len = hidraw->cbs->on_read(hidraw, rbuf->buf, rbuf->len);
        iobuf_del(rbuf, 0, len);
    }
*/
}

int hidraw_open(hidraw_t *hidraw, const char *path, uint16_t vendor_id, uint16_t product_id, const char *name, struct ev_loop *loop)
{
    int fd = -1;

    if (hidraw->fd != -1)
        return 0;

    if (path) {
        if ((fd = open(path, O_RDWR|O_NONBLOCK)) < 0)
            return _error(hidraw, HID_ERROR_OPEN, errno, "Openging hidraw device %s", path);
        snprintf(hidraw->ident, sizeof(hidraw->ident)-1, "%s(%s)", path, name);
    } else {
        struct hidraw_devinfo info;
        char *path2="/dev/hidraw*";
        glob_t globres;
        char buf[32];
        int i, ret = 0;
        
        if (glob(path2, 0, NULL, &globres))
            return _error(hidraw, HID_ERROR_OPEN, errno, "Searching hidraw device %x-%x", vendor_id, product_id);

        for(i = 0; i < globres.gl_pathc; i++) {
            fd = open(globres.gl_pathv[i], O_RDWR|O_NONBLOCK);
            if (fd < 0) {
                ret = -1;
                continue;
            }

            memset(buf, 0x0, sizeof(buf));
            ret = ioctl(fd, HIDIOCGRAWPHYS(256), buf);
            if (ret < 0)
                continue;

            memset(&info, 0x0, sizeof(info));
            ret = ioctl(fd, HIDIOCGRAWINFO, &info);
            if (ret < 0)
                continue;

            if ((info.vendor & 0xFFFF) == vendor_id && (info.product & 0xFFFF) == product_id) {
                if (name) {
                    if (!strncmp(buf, name, strlen(name)))
                        break;
                } else {
                    break;
                }
            } else {
                close(fd);
            }
        }
        if (i >= globres.gl_pathc)
            return _error(hidraw, HID_ERROR_OPEN, 0, "Searching hidraw device %x-%x", vendor_id, product_id);
        snprintf(hidraw->ident, sizeof(hidraw->ident)-1, "%s(%s)", globres.gl_pathv[i], buf);
        globfree(&globres);
    }
    hidraw->fd = fd;
    hidraw->loop = loop;
    iobuf_init(&hidraw->io.wbuf, IO_SIZE);
    iobuf_init(&hidraw->io.rbuf, IO_SIZE);
    ev_io_init(&hidraw->io.iow, _hidraw_write_cb, hidraw->fd, EV_WRITE);
    ev_io_init(&hidraw->io.ior, _hidraw_read_cb, hidraw->fd, EV_READ);
    ev_io_start(hidraw->loop, &hidraw->io.ior);
    return 0;
}

ssize_t hidraw_write(hidraw_t *hidraw, const uint8_t *buf, size_t len)
{
    ssize_t ret;
    struct iobuf *wbuf = &hidraw->io.wbuf;
    struct ev_io *iow = &hidraw->io.iow;

    ret = iobuf_add(wbuf, wbuf->len, buf, len);
    ev_io_start(hidraw->loop, iow);
    return ret;
}

ssize_t hidraw_read(hidraw_t *hidraw, uint8_t *buf, size_t len, int timeout_ms)
{
    ssize_t ret;

    struct timeval tv_timeout;
    tv_timeout.tv_sec = timeout_ms / 1000;
    tv_timeout.tv_usec = (timeout_ms % 1000) * 1000;

    size_t bytes_read = 0;

    while (bytes_read < len) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(hidraw->fd, &rfds);

        if ((ret = select(hidraw->fd+1, &rfds, NULL, NULL, (timeout_ms < 0) ? NULL : &tv_timeout)) < 0)
            return _error(hidraw, HID_ERROR_IO, errno, "select() on hidraw port");

        /* Timeout */
        if (ret == 0)
            break;

        if ((ret = read(hidraw->fd, buf + bytes_read, len - bytes_read)) < 0)
            return _error(hidraw, HID_ERROR_IO, errno, "Reading hidraw device");

        /* Empty read */
        if (ret == 0 && len != 0)
            return _error(hidraw, HID_ERROR_IO, 0, "Reading hidraw device: unexpected empty read");

        bytes_read += ret;
    }

    return bytes_read;
}

int hidraw_poll(hidraw_t *hidraw, int timeout_ms)
{
    struct pollfd fds[1];
    int ret;

    /* Poll */
    fds[0].fd = hidraw->fd;
    fds[0].events = POLLIN | POLLPRI;
    if ((ret = poll(fds, 1, timeout_ms)) < 0)
        return _error(hidraw, HID_ERROR_IO, errno, "Polling hidraw device");

    if (ret)
        return 1;

    /* Timed out */
    return 0;
}

int hidraw_close(hidraw_t *hidraw)
{
    if (hidraw->fd < 0)
        return 0;

    ev_io_stop(hidraw->loop, &hidraw->io.ior);
    ev_io_stop(hidraw->loop, &hidraw->io.iow);
    iobuf_free(&hidraw->io.rbuf);
    iobuf_free(&hidraw->io.rbuf);

    memset(hidraw->ident, 0, sizeof(hidraw->ident));
    if (close(hidraw->fd) < 0)
        return _error(hidraw, HID_ERROR_CLOSE, errno, "Closing hidraw device");

    hidraw->fd = -1;
    return 0;
}
