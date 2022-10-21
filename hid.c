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
#include "hid.h"
#include "log.h"
#include "io_channel.h"
#include "iobuf.h"
#include "utils.h"

struct hid_handle {
    char ident[32];
    int fd;
    struct ev_loop *loop;
    struct io_channel io;
    struct {
        int c_errno;
        char errmsg[256];
    } error;
};

static int _hid_error(hid_t *hid, int code, int c_errno, const char *fmt, ...)
{
    va_list ap;

    hid->error.c_errno = c_errno;

    va_start(ap, fmt);
    vsnprintf(hid->error.errmsg, sizeof(hid->error.errmsg), fmt, ap);
    va_end(ap);

    if (c_errno) {
        char buf[64];
        strerror_r(c_errno, buf, sizeof(buf));
        snprintf(hid->error.errmsg+strlen(hid->error.errmsg), sizeof(hid->error.errmsg)-strlen(hid->error.errmsg), ": %s [errno %d]", buf, c_errno);
    }

    return code;
}

const char* hid_id(hid_t *hid)
{
    return hid->ident;
}

const char *hid_errmsg(hid_t *hid)
{
    return hid->error.errmsg;
}

int hid_errno(hid_t *hid)
{
    return hid->error.c_errno;
}

int hid_fd(hid_t *hid)
{
    return hid->fd;
}

hid_t *hid_new()
{
    hid_t *hid = calloc(1, sizeof(hid_t));
    if (hid == NULL)
        return NULL;

    hid->fd = -1;
    return hid;
}

void hid_free(hid_t *hid)
{
    free(hid);
}

static void _hid_write_cb(struct ev_loop *loop, struct ev_io *w, int revents)
{
    hid_t *hid = container_of(w, hid_t, io.iow);
    struct iobuf *wbuf = &hid->io.wbuf;
    uint8_t *buf = wbuf->buf;
    size_t len = wbuf->len;
    ssize_t n;
    ssize_t remain = len;
    bool nonblock = fd_is_nonblock(hid->fd);

    iobuf_dump(wbuf, wbuf->len);
    do {
        n = write(hid->fd, buf, remain);
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
        ev_io_stop(hid->loop, w);
}

static void _hid_read_cb(struct ev_loop *loop, struct ev_io *w, int revents)
{
    hid_t *hid = container_of(w, hid_t, io.ior);
    bool nonblock = fd_is_nonblock(hid->fd);
    struct iobuf *rbuf = &hid->io.rbuf;
    ssize_t remain = INT_MAX;
    ssize_t ret;

    do {
        size_t want;
        uint8_t *buf = &rbuf->buf[rbuf->len];

        if ((rbuf->cap - rbuf->len) < IO_SIZE &&
            !iobuf_resize(rbuf, rbuf->cap + IO_SIZE)) {
            log_error("hid recv buf too small");
            return;
        }

        want = rbuf->cap - rbuf->len;
        if (want > remain)
            want = remain;

        ret = read(hid->fd, buf, want);
        if (unlikely(ret < 0)) {
            if (errno == EINTR)
                continue;

            if (errno == EAGAIN || errno == ENOTCONN)
                break;
            log_error("hid read: %s", strerror(errno));
            return;
        }
        if (ret == 0)
            break;
        rbuf->len += ret;
        remain -= ret;
    } while (remain && nonblock);
    
    iobuf_dump(rbuf, rbuf->len);
/*
    if(hid->cbs->on_read) {
        int len = hid->cbs->on_read(hid, rbuf->buf, rbuf->len);
        iobuf_del(rbuf, 0, len);
    }
*/
}

int hid_open(hid_t *hid, const char *dev, uint16_t vendor_id, uint16_t product_id, const char *name, struct ev_loop *loop)
{
    int fd = -1;
log_warn("");

    if (hid->fd != -1)
        return 0;
log_warn("");
    if (dev) {
        if ((fd = open(dev, O_RDWR|O_NONBLOCK)) < 0)
            return _hid_error(hid, HID_ERROR_OPEN, errno, "Openging hid device %s", dev);
        strncpy(hid->ident, dev, sizeof(hid->ident)-1);
    } else {
        struct hidraw_devinfo info;
        char *path="/dev/hidraw*";
        glob_t globres;
        char buf[256];
        int i, ret = 0;
        
        if (glob(path, 0, NULL, &globres))
            return _hid_error(hid, HID_ERROR_OPEN, errno, "Searching hid device %x-%x", vendor_id, product_id);

        for(i = 0; i < globres.gl_pathc; i++) {
            fd = open(globres.gl_pathv[i], O_RDWR|O_NONBLOCK);
            if (fd < 0) {
                ret = -1;
                continue;
            }

            /* Get Raw Name */
            memset(buf, 0x0, sizeof(buf));
            ret = ioctl(fd, HIDIOCGRAWNAME(256), buf);
            if (ret < 0)
                continue;

            /* Get Raw Info */
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
            return _hid_error(hid, HID_ERROR_OPEN, 0, "Searching hid device %x-%x", vendor_id, product_id);
        strncpy(hid->ident, globres.gl_pathv[i], sizeof(hid->ident)-1);
        globfree(&globres);
    }
    hid->fd = fd;
    hid->loop = loop;
    iobuf_init(&hid->io.rbuf, IO_SIZE);
    ev_io_init(&hid->io.iow, _hid_write_cb, hid->fd, EV_WRITE);
    ev_io_init(&hid->io.ior, _hid_read_cb, hid->fd, EV_READ);
    ev_io_start(hid->loop, &hid->io.ior);
    return 0;
}

ssize_t hid_write(hid_t *hid, const uint8_t *buf, size_t len)
{
    ssize_t ret;
    struct iobuf *wbuf = &hid->io.wbuf;
    struct ev_io *iow = &hid->io.iow;

    ret = iobuf_add(wbuf, wbuf->len, buf, len);
    ev_io_start(hid->loop, iow);
    return ret;
}

ssize_t hid_read(hid_t *hid, uint8_t *buf, size_t len, int timeout_ms)
{
    ssize_t ret;

    struct timeval tv_timeout;
    tv_timeout.tv_sec = timeout_ms / 1000;
    tv_timeout.tv_usec = (timeout_ms % 1000) * 1000;

    size_t bytes_read = 0;

    while (bytes_read < len) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(hid->fd, &rfds);

        if ((ret = select(hid->fd+1, &rfds, NULL, NULL, (timeout_ms < 0) ? NULL : &tv_timeout)) < 0)
            return _hid_error(hid, HID_ERROR_IO, errno, "select() on hid port");

        /* Timeout */
        if (ret == 0)
            break;

        if ((ret = read(hid->fd, buf + bytes_read, len - bytes_read)) < 0)
            return _hid_error(hid, HID_ERROR_IO, errno, "Reading hid device");

        /* Empty read */
        if (ret == 0 && len != 0)
            return _hid_error(hid, HID_ERROR_IO, 0, "Reading hid device: unexpected empty read");

        bytes_read += ret;
    }

    return bytes_read;
}

int hid_poll(hid_t *hid, int timeout_ms)
{
    struct pollfd fds[1];
    int ret;

    /* Poll */
    fds[0].fd = hid->fd;
    fds[0].events = POLLIN | POLLPRI;
    if ((ret = poll(fds, 1, timeout_ms)) < 0)
        return _hid_error(hid, HID_ERROR_IO, errno, "Polling hid device");

    if (ret)
        return 1;

    /* Timed out */
    return 0;
}

int hid_close(hid_t *hid)
{
    if (hid->fd < 0)
        return 0;

    ev_io_stop(hid->loop, &hid->io.ior);
    ev_io_stop(hid->loop, &hid->io.iow);

    if (close(hid->fd) < 0)
        return _hid_error(hid, HID_ERROR_CLOSE, errno, "Closing hid device");

    hid->fd = -1;
    return 0;
}
