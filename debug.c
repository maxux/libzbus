#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <msgpack.h>
#include <hiredis/hiredis.h>
#include "libzbus.h"

int libzbus_debug_flag = 0;

void libzbus_diep(char *prefix, char *str) {
    fprintf(stderr, "[-] %s: %s: %s\n", prefix, str, strerror(errno));
    exit(EXIT_FAILURE);
}

void *libzbus_warnp(char *str) {
    printf("[-] %s: %s\n", str, strerror(errno));
    return NULL;
}

void libzbus_debug_set(int flag) {
    libzbus_debug_flag = flag;
}

