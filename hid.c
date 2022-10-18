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

#include "hid.h"
#include "log.h"
#include "io_channel.h"
#include "iobuf.h"
#include "utils.h"

struct hid_handle {
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

hid_t *hid_new(struct ev_loop *loop)
{
    hid_t *hid = calloc(1, sizeof(hid_t));
    if (hid == NULL)
        return NULL;

    hid->fd = -1;
    hid->loop = loop;
    return hid;
}

void hid_free(hid_t *hid)
{
    free(hid);
}

static ssize_t _hid_write_sync(hid_t *hid, const uint8_t *buf, size_t len)
{
    ssize_t ret;

    if ((ret = write(hid->fd, buf, len)) < 0) {
        return _hid_error(hid, HID_ERROR_IO, errno, "Writing hid device");
    }

    return ret;
}

static void _hid_write_cb(struct ev_loop *loop, struct ev_io *w, int revents)
{
    log_info("");
    hid_t *hid = container_of(w, hid_t, io.iow);
    struct iobuf *wbuf = &hid->io.wbuf;

    unsigned char *buf = wbuf->buf;
    size_t len = wbuf->len;
    ssize_t n;
    n = _hid_write_sync(hid, buf, len);
    if (n > 0)
        iobuf_del(wbuf, 0, (size_t) n);
    ev_io_stop(hid->loop, w);
}

static void _hid_read_cb(struct ev_loop *loop, struct ev_io *w, int revents)
{
    log_info("");
}

int hid_open(hid_t *hid, unsigned short vendor_id, unsigned short product_id, const char *name)
{
    int fd = -1, ret = 0;
    int i;
    struct hidraw_devinfo info;

    char *path="/dev/hidraw*";
    glob_t globres;
    char buf[256];
    
    if (glob(path, 0, NULL, &globres))
        return _hid_error(hid, HID_ERROR_OPEN, errno, "Searching hid device");

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
        }
    }

    if (i < globres.gl_pathc)
        hid->fd = fd;
log_info("hid fd=%d", hid->fd);
    ev_io_init(&hid->io.iow, _hid_write_cb, hid->fd, EV_WRITE);
    ev_io_init(&hid->io.ior, _hid_read_cb, hid->fd, EV_READ);
    ev_io_start(hid->loop, &hid->io.ior);

    globfree(&globres);
    return hid->fd != -1 ? 0: _hid_error(hid, ret < 0 ? ret:HID_ERROR_OPEN, ret < 0 ? errno:0, "Openging hid device");;
}

ssize_t hid_write(hid_t *hid, const uint8_t *buf, size_t len)
{
    ssize_t ret;
    struct iobuf *wbuf = &hid->io.wbuf;
    struct ev_io *iow = &hid->io.iow;

    ret = iobuf_add(wbuf, wbuf->len, buf, len);
    ev_io_start(hid->loop, iow);
    log_info("");
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
