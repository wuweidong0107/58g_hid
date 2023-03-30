#include <stdlib.h>
#include <errno.h>
#include "log.h"
#include "shell.h"
#include "usb.h"
#include "device.h"
#include "thpool.h"

extern threadpool thpool;

static void task_usb_hid_write(void *arg)
{
    struct cmd_context *ctx = (struct cmd_context *)arg;
    if (ctx->argc < 2) {
        log_error("invalid param");
        goto cleanup;
    }

    int index;
    uint8_t data[257];
    int i,len;
    int timeout_ms = 5;

    index = strtoul(ctx->argv[1], NULL, 10);
    usb_t *usb = get_usb(index);
    if (usb == NULL) {
        log_error("getting usb handle");
        goto cleanup;
    }
    for (i=2, len=0; i<ctx->argc && len<sizeof(data); i++, len++) {
        data[len] = strtoul(ctx->argv[i], NULL, 16);
    }
    len = len + 1;
    if (usb_hid_write(usb, data, len, timeout_ms) != len) {
        log_error("usb hid writting: %s", usb_errmsg(usb));
        goto cleanup;
    }
    
    usb_hid_get_input_report(usb, data, sizeof(data), timeout_ms);

cleanup:
    free(ctx);
}

/*
 * exmaple: usb_hid_write 0 0x0 0x06 0x55 0xAA 0x80 0x01 0xA1 0x20
 */
int cmd_usb_hid_write(int argc, char *argv[])
{
    struct cmd_context *ctx = malloc(sizeof(*ctx));
    ctx->argc = argc;
    ctx->argv = argv;
    thpool_add_work(thpool, task_usb_hid_write, ctx);
    return 0;
}

static int on_hid_get_input_report(const uint8_t *buf, size_t len)
{
    int i;
    shell_printf("Got %d bytes:\n", buf[0]);
    for(i=0; i<buf[0]; i++) {
        printf("%02x ", buf[i]);
    };
    printf("\n");
    return 0;
}

int cmd_usb_hid_enumerate(int argc, char *argv[])
{
    usb_t *usb = get_usb(0);        // all usb device can do enumeration job
    if (!usb)
        return -1;
    struct usb_device_info *devs;
    struct usb_device_info *cur_dev;
    devs = usb_hid_enumerate(usb, 0x0, 0x0);
    cur_dev = devs;
    while (cur_dev) {
        shell_printf("Device Found:\n");
        shell_printf("  Type:         0x%04hx 0x%04hx\n  path: %s\n", cur_dev->vendor_id, cur_dev->product_id, cur_dev->path);
        shell_printf("  Manufacturer: %s\n",  cur_dev->manufacturer_string);
        shell_printf("  Product:      %s\n",  cur_dev->product_string);
        shell_printf("  Interface:    %d\n",  cur_dev->interface_number);
        shell_printf("\n");
        cur_dev = cur_dev->next;
    }
    usb_hid_free_enumeration(usb, devs);
    return 0;
}

int cmd_usb_hid_list(int argc, char *argv[])
{
    int i;
    usb_t *usb;

    for (i=0; (usb=get_usb(i)) != NULL; i++)
        shell_printf("%d: %s\n", i, usb_id(usb));
    
    return 0;
}

static struct usb_client_ops usb_menu_ops = {
    .on_get_input_report = on_hid_get_input_report,
};

static struct usb_client usb_menu = {
    .name = "usb menu",
    .ops = &usb_menu_ops,
};

int usb_shell_init(void)
{
    int i,ret;
    usb_t *usb;

    for (i=0; (usb=get_usb(i)) != NULL; i++) {
        if ((ret = usb_add_client(usb, &usb_menu)))
            return ret;
    }
    return 0;
}

void usb_shell_exit(void)
{
    int i;
    usb_t *usb;

    for (i=0; (usb=get_usb(i)) != NULL; i++) {
        usb_remove_client(usb, &usb_menu);
    }
}
