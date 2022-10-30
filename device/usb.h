#ifndef __USB_H__
#define __USB_H__

#include <stdint.h>
#include <libusb-1.0/libusb.h>

typedef struct usb_handle usb_t;

struct hid_device_info {
    /* Platform-specific device path */
    char *path;
    /* Device Vendor ID */
    unsigned short vendor_id;
    /* Device Product ID */
    unsigned short product_id;
    /* The USB interface which this logical device represents */
    int interface_number;
    /* Pointer to the next device */
    struct hid_device_info *next;
};

enum usb_error_code {
    USB_ERROR_ARG            = -1, /* Invalid arguments */
    USB_ERROR_OPEN           = -2, /* Opening usb device */
    USB_ERROR_QUERY          = -3, /* Querying usb device attributes */
    USB_ERROR_CONFIGURE      = -4, /* Configuring usb device attributes */
    USB_ERROR_IO             = -5, /* Reading/writing usb device */
    USB_ERROR_CLOSE          = -6, /* Closing usb device */
};

int usb_init(void);
void usb_exit(void);
usb_t *usb_new(void);
void usb_free(usb_t *usb);
struct hid_device_info* usb_hid_enumerate(usb_t *usb, uint16_t vendor_id, uint16_t product_id);
void usb_hid_free_enumeration(usb_t *usb, struct hid_device_info *devs);
#endif