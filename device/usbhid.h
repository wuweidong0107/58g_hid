#ifndef __USBHID_H__
#define __USBHID_H__

typedef struct usbhid_handle usbhid_t;

enum usbhid_error_code {
    USBHID_ERROR_ARG            = -1, /* Invalid arguments */
    USBHID_ERROR_OPEN           = -2, /* Opening usbhid device */
    USBHID_ERROR_QUERY          = -3, /* Querying usbhid device attributes */
    USBHID_ERROR_CONFIGURE      = -4, /* Configuring usbhid device attributes */
    USBHID_ERROR_IO             = -5, /* Reading/writing usbhid device */
    USBHID_ERROR_CLOSE          = -6, /* Closing usbhid device */
};

const char *usbhid_errmsg(usbhid_t *usbhid);
int usbhid_errno(usbhid_t *usbhid);

#endif