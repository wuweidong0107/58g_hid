#include <stdlib.h>
#include <errno.h>
#include "log.h"
#include "shell.h"
#include "wifi.h"
#include "device.h"

static void help(void)
{
    shell_printf("Usage: wifi <opr> [args]\n");
    shell_printf("  Available opr: connect <ssid> <password>\n");
}

int cmd_wifi(int argc, char *argv[])
{
    int i;

    if (argc < 2) {
        help();
        return 0;
    }

    wifi_t *wifi = get_wifi(0);
    if (wifi == NULL)
        return -EINVAL;

    if (!strncmp(argv[1], "connect", strlen("connect")) && argc == 4) {
        wifi_scan(get_wifi(0));
        wifi_network_info_t network;
        strncpy(network.ssid, argv[2], sizeof(network.ssid)-1);
        strncpy(network.password, argv[3], sizeof(network.password)-1);
        if (wifi_connect_ssid(wifi, &network) == true && network.connected == true)
            return 0; 
    }

    return -1;
}