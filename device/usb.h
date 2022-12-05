#ifndef __USB_H__
#define __USB_H__

#include <stdint.h>
#include <libusb-1.0/libusb.h>
#include "list.h"

typedef struct usb_handle usb_t;

struct usb_device_info {
    /* Platform-specific device path */
    char *path;
    /* Device Vendor ID */
    unsigned short vendor_id;
    /* Device Product ID */
    unsigned short product_id;
    /* Manufacturer String */
    unsigned char manufacturer_string[256];
    /* Product string */
    unsigned char product_string[256];
    /* The USB interface which this logical device represents */
    int interface_number;
    /* Pointer to the next device */
    struct usb_device_info *next;
};

enum usb_error_code {
    USB_ERROR_ARG            = -1, /* Invalid arguments */
    USB_ERROR_OPEN                            = -2, /* Opening usb device */
    USB_ERROR_QUERY          = -3, /* Querying usb device attributes */
    USB_ERROR_CONFIGURE      = -4, /* Configuring usb device attributes */
    USB_ERROR_IO             = -5, /* Reading/writing usb device */
    USB_ERROR_CLOSE          = -6, /* Closing usb device */
};

struct usb_client_ops {
    int (*on_get_input_report)(const uint8_t *buf, size_t len);
};

struct usb_client {
    char name[64];
    struct usb_client_ops *ops;
    struct list_head list;
};

typedef struct usb_options {
    uint16_t vid;
    uint16_t pid;
    char path[96];
} usb_options_t;

int usb_init(void);
void usb_exit(void);
usb_t *usb_new(void);
void usb_free(usb_t *usb);
int usb_open(usb_t *usb, uint16_t vendor_id, uint16_t product_id, const char *path);
void usb_close(usb_t *usb);
int usb_hid_write(usb_t *usb, const uint8_t *data, size_t length, int timeout_ms);
int usb_hid_get_input_report(usb_t *usb, uint8_t *data, size_t length, int timeout_ms);
struct usb_device_info* usb_hid_enumerate(usb_t *usb, uint16_t vendor_id, uint16_t product_id);
void usb_hid_free_enumeration(usb_t *usb, struct usb_device_info *devs);
const char* usb_id(usb_t *usb);
int usb_add_client(usb_t *usb, struct usb_client *client);
void usb_remove_client(usb_t *usb, struct usb_client *client);
const char *usb_errmsg(usb_t *usb);
int usb_errno(usb_t *usb);
#endif