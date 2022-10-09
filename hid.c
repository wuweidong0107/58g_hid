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

#include "hid.h"
#include "log.h"

struct hid_handle {
    int fd;
    struct {
        int c_errno;
        char errmsg[256];
    } error;
};

static const char *bus_str(int bus)
{
	switch (bus) {
	case BUS_USB:
		return "USB";
		break;
	case BUS_HIL:
		return "HIL";
		break;
	case BUS_BLUETOOTH:
		return "Bluetooth";
		break;
	case BUS_VIRTUAL:
		return "Virtual";
		break;
	default:
		return "Other";
		break;
	}
}

static int _hid_error(hid_t *hid, int code, int c_errno, const char *fmt, ...) {
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

hid_t *hid_new(void)
{
    hid_t *hid = calloc(1, sizeof(hid_t));
    if (hid == NULL)
        return NULL;

    hid->fd = -1;

    return hid;
}

int hid_open(hid_t *hid, unsigned short vendor_id, unsigned short product_id, const char *name)
{
    int fd = -1, ret;
    int i;
    struct hidraw_devinfo info;

    char *path="/dev/hidraw*";
    glob_t globres;
    char buf[256];
    
    if (glob(path, 0, NULL, &globres))
        return _hid_error(hid, HID_ERROR_OPEN, errno, "Search hid device");

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

    globfree(&globres);
    return hid->fd != -1 ? 0: _hid_error(hid, ret < 0 ? ret:HID_ERROR_OPEN, ret < 0 ? errno:0, "Fail to open hid device");;
}