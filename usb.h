#ifndef __USB_H__
#define __USB_H__

#include <stdint.h>

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

int usb_init(void);
void usb_exit(void);
usb_t *usb_new(void);
void usb_free(usb_t *usb);
struct hid_device_info* usb_hid_enumerate(usb_t *usb, uint16_t vendor_id, uint16_t product_id);
void usb_hid_free_enumeration(struct hid_device_info *devs);
#endif