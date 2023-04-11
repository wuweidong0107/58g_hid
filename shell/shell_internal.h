#ifndef __SHELL_INTERNAL_H__
#define __SHELL_INTERNAL_H__

extern int menu_aw5808_init(void);
extern void menu_aw5808_exit(void);
extern int serial_shell_init(void);
extern void serial_shell_exit(void);
extern int usb_shell_init(void);
extern int usb_shell_exit(void);

extern int cmd_aw5808(int argc, char *argv[]);
extern int cmd_aw5808_list(int argc, char *argv[]);
extern int cmd_aw5808_get_config(int argc, char *argv[]);
extern int cmd_aw5808_get_rfstatus(int argc, char *argv[]);
extern int cmd_aw5808_pair(int argc, char *argv[]);
extern int cmd_aw5808_set_mode(int argc, char *argv[]);
extern int cmd_aw5808_set_i2s_mode(int argc, char *argv[]);
extern int cmd_aw5808_set_connect_mode(int argc, char *argv[]);
extern int cmd_aw5808_set_rfchannel(int argc, char *argv[]);
extern int cmd_aw5808_set_rfpower(int argc, char *argv[]);
extern int cmd_serial_list(int argc, char *argv[]);
extern int cmd_serial_write(int argc, char *argv[]);
extern int cmd_usb_hid_enumerate(int argc, char *argv[]);
extern int cmd_usb_hid_list(int argc, char *argv[]);
extern int cmd_usb_hid_write(int argc, char *argv[]);
extern int cmd_io(int argc, char *argv[]);
#endif