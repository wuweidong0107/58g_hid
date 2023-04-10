#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "list.h"
#include "wifi_internal.h"

typedef struct wpacli_handle {
    int i;
} wpacli_t;

static void* wpacli_init(void)
{
    wpacli_t *wpacli = calloc(1, sizeof(wpacli_t));
    return wpacli;
}

void wpacli_free(void *handle)
{
    if (handle)
        free(handle);
}

wifi_backend_t wifi_wpacli = {
    wpacli_init,
    wpacli_free,
    "wpacli"
};