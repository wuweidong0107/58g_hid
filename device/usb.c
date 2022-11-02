#include <stdlib.h>
#include <string.h>
#include "log.h"
#include "usb.h"
#include "usb.h"

#define HID_INPUT_REPORT	1
#define HID_OUTPUT_REPORT	2
#define HID_FEATURE_REPORT	3

// HID Class-Specific Requests values. See section 7.2 of the HID specifications
#define HID_GET_REPORT                0x01
#define HID_GET_IDLE                  0x02
#define HID_GET_PROTOCOL              0x03
#define HID_SET_REPORT                0x09
#define HID_SET_IDLE                  0x0A
#define HID_SET_PROTOCOL              0x0B
#define HID_REPORT_TYPE_INPUT         0x01
#define HID_REPORT_TYPE_OUTPUT        0x02
#define HID_REPORT_TYPE_FEATURE       0x03

static libusb_context *usb_context = NULL;

struct usb_handle {
    struct list_head list;
    char ident[64];
    libusb_context *context;
    libusb_device *usb_dev;
    libusb_device_handle *device_handle;

    char path[64];
	/* The interface number of the HID */
	int interface;
    
	/* Endpoint information */
	int input_endpoint;
	int output_endpoint;
	int input_ep_max_packet_size;

	/* Indexes of Strings */
	int manufacturer_index;
	int product_index;
	int serial_index;

    int is_driver_detached;
    struct list_head clients;
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

static struct usb_device_info *create_device_info_for_device(libusb_device *device, libusb_device_handle *handle, struct libusb_device_descriptor *desc, int config_number, int interface_num)
{
	struct usb_device_info *cur_dev = calloc(1, sizeof(struct usb_device_info));
	if (cur_dev == NULL) {
		return NULL;
	}

	cur_dev->vendor_id = desc->idVendor;
	cur_dev->product_id = desc->idProduct;
	cur_dev->interface_number = interface_num;
	cur_dev->path = make_path(device, config_number, interface_num);

	if (!handle) {
		return cur_dev;
	}

    if (desc->iManufacturer)
        libusb_get_string_descriptor_ascii(handle, desc->iManufacturer, cur_dev->manufacturer_string, sizeof(cur_dev->manufacturer_string));
    if (desc->iProduct)
        libusb_get_string_descriptor_ascii(handle, desc->iProduct, cur_dev->product_string, sizeof(cur_dev->product_string));
	
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
    INIT_LIST_HEAD(&usb->clients);
    return usb;
}

void usb_free(usb_t *usb)
{
    free(usb);
}

static int usb_initialize_device(usb_t *usb, const struct libusb_interface_descriptor *intf_desc)
{
	int i =0;
	int res = 0;
	struct libusb_device_descriptor desc;
	libusb_get_device_descriptor(libusb_get_device(usb->device_handle), &desc);

	/* Detach the kernel driver, but only if the
	   device is managed by the kernel */
	usb->is_driver_detached = 0;
	if (libusb_kernel_driver_active(usb->device_handle, intf_desc->bInterfaceNumber) == 1) {
		res = libusb_detach_kernel_driver(usb->device_handle, intf_desc->bInterfaceNumber);
        log_error("res=%d\n", res);
        if (res < 0) {
			return 0;
		} else {
			usb->is_driver_detached = 1;
		}
	}

	res = libusb_claim_interface(usb->device_handle, intf_desc->bInterfaceNumber);
    log_error("res=%d\n", res);
	if (res < 0) {
		if (usb->is_driver_detached) {
			libusb_attach_kernel_driver(usb->device_handle, intf_desc->bInterfaceNumber);
		}
		return 0;
	}

	/* Store off the string descriptor indexes */
	usb->manufacturer_index = desc.iManufacturer;
	usb->product_index      = desc.iProduct;
	usb->serial_index       = desc.iSerialNumber;

	/* Store off the USB information */
	usb->interface = intf_desc->bInterfaceNumber;
	usb->input_endpoint = 0;
	usb->input_ep_max_packet_size = 0;
	usb->output_endpoint = 0;

	/* Find the INPUT and OUTPUT endpoints. An
	   OUTPUT endpoint is not required. */
	for (i = 0; i < intf_desc->bNumEndpoints; i++) {
		const struct libusb_endpoint_descriptor *ep
			= &intf_desc->endpoint[i];

		/* Determine the type and direction of this
		   endpoint. */
		int is_interrupt =
			(ep->bmAttributes & LIBUSB_TRANSFER_TYPE_MASK)
		      == LIBUSB_TRANSFER_TYPE_INTERRUPT;
		int is_output =
			(ep->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK)
		      == LIBUSB_ENDPOINT_OUT;
		int is_input =
			(ep->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK)
		      == LIBUSB_ENDPOINT_IN;

		/* Decide whether to use it for input or output. */
		if (usb->input_endpoint == 0 &&
		    is_interrupt && is_input) {
			/* Use this endpoint for INPUT */
			usb->input_endpoint = ep->bEndpointAddress;
			usb->input_ep_max_packet_size = ep->wMaxPacketSize;
		}
		if (usb->output_endpoint == 0 &&
		    is_interrupt && is_output) {
			/* Use this endpoint for OUTPUT */
			usb->output_endpoint = ep->bEndpointAddress;
		}
	}
	return 1;
}

/*
 * path: bus-port:config_number.interface_number
 */
int usb_open(usb_t *usb, uint16_t vendor_id, uint16_t product_id, const char *path)
{
	struct libusb_device **devs;
	struct libusb_device *found = NULL;
	struct libusb_device *dev;
	struct libusb_device_handle *device_handle = NULL;
	size_t i = 0;
	int r;
    int good_open = 0;

    if (usb_init() < 0) {
        return _error(usb, USB_ERROR_OPEN, 0, "USB not init");
    }

	if (libusb_get_device_list(usb->context, &devs) < 0)
		return NULL;

	while ((dev = devs[i++]) != NULL) {
		struct libusb_device_descriptor desc;
		r = libusb_get_device_descriptor(dev, &desc);
		if (r < 0)
			goto out;
		if (desc.idVendor == vendor_id && desc.idProduct == product_id) {
			found = dev;
			break;
		}
	}

	if (found) {
        struct libusb_config_descriptor *conf_desc = NULL;
        r = libusb_get_active_config_descriptor(dev, &conf_desc);
        if (r < 0)
            libusb_get_config_descriptor(dev, 0, &conf_desc);
        if (conf_desc) {
            int j,k;
            for (j = 0; j < conf_desc->bNumInterfaces; j++) {
                const struct libusb_interface *intf = &conf_desc->interface[j];
                for (k = 0; k < intf->num_altsetting; k++) {
                    const struct libusb_interface_descriptor *intf_desc;
                    intf_desc = &intf->altsetting[k];
                    get_path(&usb->path, dev, conf_desc->bConfigurationValue, intf_desc->bInterfaceNumber);
                    printf("path=%s usb->path=%s\n", path, usb->path);
                    if (!path || !strncmp(path, usb->path, strlen(path))) {
                        r = libusb_open(found, &usb->device_handle);
                        printf("r=%d\n", r);
                        if (r < 0) {
                            break;
                        }
                        good_open = usb_initialize_device(usb, intf_desc);
                        printf("good_open=%d\n", good_open);
                        if (!good_open)
                            libusb_close(usb->device_handle);
                    }
                } /* altsettings */
            } /* interfaces */
            libusb_free_config_descriptor(conf_desc);
        }
	}

out:
	libusb_free_device_list(devs, 1);
	return good_open == 1 ? 0:_error(usb, USB_ERROR_OPEN, 0, "Openning usb device");;
}

int usb_close(usb_t *usb)
{                                  
    libusb_close(usb->device_handle);
}

int usb_hid_write(usb_t *usb, const const uint8_t *data, size_t length, int timeout_ms)
{
	int res;
	int report_number;
	int skipped_report_id = 0;

	if (!data || (length ==0)) {
		return -1;
	}

	report_number = data[0];

	if (report_number == 0x0) {
		data++;
		length--;
		skipped_report_id = 1;
	}

	if (usb->output_endpoint <= 0) {
		/* No interrupt out endpoint. Use the Control Endpoint */
		res = libusb_control_transfer(usb->device_handle,
			LIBUSB_REQUEST_TYPE_CLASS|LIBUSB_RECIPIENT_INTERFACE|LIBUSB_ENDPOINT_OUT,
			HID_SET_REPORT,
			htole16((HID_OUTPUT_REPORT/*HID output*/ << 8) | report_number),
			htole16(usb->interface),
			(unsigned char *)data, length,
			timeout_ms);

		if (res < 0)
			return -1;

		if (skipped_report_id)
			length++;

		return length;
	} else {
		/* Use the interrupt out endpoint */
		int actual_length;
		res = libusb_interrupt_transfer(usb->device_handle,
			usb->output_endpoint,
			(unsigned char*)data,
			length,
			&actual_length, 1000);

		if (res < 0)
			return -1;

		if (skipped_report_id)
			actual_length++;

		return actual_length;
	}
}

int usb_hid_get_input_report(usb_t *usb, uint8_t *data, size_t length, int timeout_ms)
{
	int res = -1;
	int skipped_report_id = 0;
	int report_number = data[0];

	if (report_number == 0x0) {
		/* Offset the return buffer by 1, so that the report ID
		   will remain in byte 0. */
		data++;
		length--;
		skipped_report_id = 1;
	}

	res = libusb_control_transfer(usb->device_handle,
		LIBUSB_REQUEST_TYPE_CLASS|LIBUSB_RECIPIENT_INTERFACE|LIBUSB_ENDPOINT_IN,
		0x01/*HID get_report*/,
		htole16((HID_INPUT_REPORT/*HID Input*/ << 8) | report_number),
		htole16(usb->interface),
		(unsigned char *)data, length,
		timeout_ms);

	if (res < 0)
		return -1;

	if (skipped_report_id)
		res++;

    struct usb_client *client;
	list_for_each_entry(client, &usb->clients, list) {
        if (client->ops->on_hid_get_input_report)
            client->ops->on_hid_get_input_report(data, length);
	}
	return res;
}

struct usb_device_info* usb_hid_enumerate(usb_t *usb, uint16_t vendor_id, uint16_t product_id)
{
	libusb_device **devs;
	libusb_device *dev;
	libusb_device_handle *handle = NULL;
	ssize_t num_devs;
	int i = 0;

	struct usb_device_info *root = NULL; /* return object */
	struct usb_device_info *cur_dev = NULL;

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
                    struct usb_device_info *tmp;
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
				} /* altsettings */
			} /* interfaces */
			libusb_free_config_descriptor(conf_desc);
		}
	}

	libusb_free_device_list(devs, 1);
	return root;
}

void usb_hid_free_enumeration(usb_t *usb, struct usb_device_info *devs)
{
	struct usb_device_info *d = devs;
	while (d) {
		struct usb_device_info *next = d->next;
		free(d->path);
		free(d);
		d = next;
	}
}

const char *usb_errmsg(usb_t *usb)
{
    return usb->error.errmsg;
}

int usb_errno(usb_t *usb)
{
    return usb->error.c_errno;
}

int usb_add_client(usb_t *usb, struct usb_client *client)
{
    if (!client || !client->ops)
        return -1;
    list_add_tail(&client->list, &usb->clients);
    return 0;
}

void usb_remove_client(usb_t *usb, struct usb_client *client)
{
    list_del(&client->list);
}