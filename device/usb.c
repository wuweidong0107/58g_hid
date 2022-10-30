#include <stdlib.h>
#include <string.h>
#include "log.h"
#include "usb.h"
#include "usbhid.h"

static libusb_context *usb_context = NULL;

struct usb_handle {
    char ident[64];
    libusb_context *context;
    struct hid_device_info *devs;

    struct {
        int c_errno;
        char errmsg[256];
    } error;
};

static int _error(usb_t *usb, int code, int c_errno, const char *fmt, ...)
{
    va_list ap;

    usb->error.c_errno = c_errno;

    va_start(ap, fmt);
    vsnprintf(usb->error.errmsg, sizeof(usb->error.errmsg), fmt, ap);
    va_end(ap);

    if (c_errno) {
        char buf[64];
        strerror_r(c_errno, buf, sizeof(buf));
        snprintf(usb->error.errmsg+strlen(usb->error.errmsg), sizeof(usb->error.errmsg)-strlen(usb->error.errmsg), ": %s [errno %d]", buf, c_errno);
    }

    return code;
}

/**
  Max length of the result: "000-000.000.000.000.000.000.000:000.000" (39 chars).
  64 is used for simplicity/alignment.
*/
static void get_path(char (*result)[64], libusb_device *dev, int config_number, int interface_number)
{
	char *str = *result;

	/* Note that USB3 port count limit is 7; use 8 here for alignment */
	uint8_t port_numbers[8] = {0, 0, 0, 0, 0, 0, 0, 0};
	int num_ports = libusb_get_port_numbers(dev, port_numbers, 8);

	if (num_ports > 0) {
		int n = snprintf(str, sizeof("000-000"), "%u-%u", libusb_get_bus_number(dev), port_numbers[0]);
		for (uint8_t i = 1; i < num_ports; i++) {
			n += snprintf(&str[n], sizeof(".000"), ".%u", port_numbers[i]);
		}
		n += snprintf(&str[n], sizeof(":000.000"), ":%u.%u", (uint8_t)config_number, (uint8_t)interface_number);
		str[n] = '\0';
	} else {
		/* Likely impossible, but check: USB3.0 specs limit number of ports to 7 and buffer size here is 8 */
		if (num_ports == LIBUSB_ERROR_OVERFLOW) {
			log_error("make_path() failed. buffer overflow error\n");
		} else {
			log_error("make_path() failed. unknown error\n");
		}
		str[0] = '\0';
	}
}

static char *make_path(libusb_device *dev, int config_number, int interface_number)
{
	char str[64];
	get_path(&str, dev, config_number, interface_number);
	return strdup(str);
}

static struct hid_device_info * create_device_info_for_device(libusb_device *device, libusb_device_handle *handle, struct libusb_device_descriptor *desc, int config_number, int interface_num)
{
	struct hid_device_info *cur_dev = calloc(1, sizeof(struct hid_device_info));
	if (cur_dev == NULL) {
		return NULL;
	}

	/* VID/PID */
	cur_dev->vendor_id = desc->idVendor;
	cur_dev->product_id = desc->idProduct;
	cur_dev->interface_number = interface_num;
	cur_dev->path = make_path(device, config_number, interface_num);

	if (!handle) {
		return cur_dev;
	}

	return cur_dev;
}

int usb_init(void)
{
	if (!usb_context) {
		if (libusb_init(&usb_context))
			return -1;
	}

	return 0;
}

void usb_exit(void)
{
	if (usb_context) {
		libusb_exit(usb_context);
		usb_context = NULL;
	}
}

usb_t *usb_new(void)
{
    if (!usb_context)
        return NULL;

    usb_t *usb = calloc(1, sizeof(usb_t));
    if (usb == NULL)
        return NULL;

    usb->context = usb_context;
    return usb;
}

void usb_free(usb_t *usb)
{
    free(usb);
}

/*
int usb_open(usb_t *usb)
{
    if (usb_init() < 0)
        return _usb_error(usbhid, USBHID_ERROR_OPEN, 0, "Openging usbhid device %s", path);


    return 0;
}

int usb_close(usbhid_t *serial);
{

}
*/

struct hid_device_info* usb_hid_enumerate(usb_t *usb, uint16_t vendor_id, uint16_t product_id)
{
	libusb_device **devs;
	libusb_device *dev;
	libusb_device_handle *handle = NULL;
	ssize_t num_devs;
	int i = 0;

	struct hid_device_info *root = NULL; /* return object */
	struct hid_device_info *cur_dev = NULL;

	if(usb_init() < 0)
		return NULL;

	num_devs = libusb_get_device_list(usb->context, &devs);
	if (num_devs < 0)
		return NULL;
	while ((dev = devs[i++]) != NULL) {
		struct libusb_device_descriptor desc;
		struct libusb_config_descriptor *conf_desc = NULL;
		int j, k;

		int res = libusb_get_device_descriptor(dev, &desc);
		unsigned short dev_vid = desc.idVendor;
		unsigned short dev_pid = desc.idProduct;

		if ((vendor_id != 0x0 && vendor_id != dev_vid) ||
		    (product_id != 0x0 && product_id != dev_pid)) {
			continue;
		}

		res = libusb_get_active_config_descriptor(dev, &conf_desc);
		if (res < 0)
			libusb_get_config_descriptor(dev, 0, &conf_desc);
		if (conf_desc) {
			for (j = 0; j < conf_desc->bNumInterfaces; j++) {
				const struct libusb_interface *intf = &conf_desc->interface[j];
				for (k = 0; k < intf->num_altsetting; k++) {
					const struct libusb_interface_descriptor *intf_desc;
					intf_desc = &intf->altsetting[k];
					if (intf_desc->bInterfaceClass == LIBUSB_CLASS_HID) {
						struct hid_device_info *tmp;

						res = libusb_open(dev, &handle);
						tmp = create_device_info_for_device(dev, handle, &desc, conf_desc->bConfigurationValue, intf_desc->bInterfaceNumber);
						if (tmp) {
							if (cur_dev) {
								cur_dev->next = tmp;
							}
							else {
								root = tmp;
							}
							cur_dev = tmp;
						}

						if (res >= 0)
							libusb_close(handle);
					}
				} /* altsettings */
			} /* interfaces */
			libusb_free_config_descriptor(conf_desc);
		}
	}

	libusb_free_device_list(devs, 1);
	return root;
}

void usb_hid_free_enumeration(usb_t *usb, struct hid_device_info *devs)
{
	struct hid_device_info *d = devs;
	while (d) {
		struct hid_device_info *next = d->next;
		free(d->path);
		free(d);
		d = next;
	}
}

#if 0
int usb_hid_open_path(usb_t *usb, const char *path, int *config_number, struct libusb_interface_descriptor *intf_desc)
{
	libusb_device **devs = NULL;
	libusb_device *usb_dev = NULL;
	int res = 0;
	int d = 0;
	int good_open = 0;

	if(usb_init() < 0)
		return NULL;

	libusb_get_device_list(usb_context, &devs);
	while ((usb_dev = devs[d++]) != NULL && !good_open) {
		struct libusb_config_descriptor *conf_desc = NULL;
		int j,k;

		if (libusb_get_active_config_descriptor(usb_dev, &conf_desc) < 0)
			continue;
		for (j = 0; j < conf_desc->bNumInterfaces && !good_open; j++) {
			const struct libusb_interface *intf = &conf_desc->interface[j];
			for (k = 0; k < intf->num_altsetting && !good_open; k++) {
				const struct libusb_interface_descriptor *intf_desc = &intf->altsetting[k];
				if (intf_desc->bInterfaceClass == LIBUSB_CLASS_HID) {
					char dev_path[64];
					get_path(&dev_path, usb_dev, conf_desc->bConfigurationValue, intf_desc->bInterfaceNumber);
					if (!strcmp(dev_path, path)) {
						/* Matched Paths. Open this device */

						/* OPEN HERE */
						res = libusb_open(usb_dev, &dev->device_handle);
						if (res < 0) {
							LOG("can't open device\n");
							break;
						}
						good_open = hidapi_initialize_device(dev, conf_desc->bConfigurationValue, intf_desc);
						if (!good_open)
							libusb_close(dev->device_handle);
					}
				}
			}
		}
		libusb_free_config_descriptor(conf_desc);
	}

	libusb_free_device_list(devs, 1);

	/* If we have a good handle, return it. */
	if (good_open) {
		return dev;
	}
	else {
		/* Unable to open any devices. */
		free_hid_device(dev);
		return NULL;
	}
}
#endif