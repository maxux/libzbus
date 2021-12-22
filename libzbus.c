#include <stdio.h>
#include <msgpack.h>
#include <stdint.h>
#include <hiredis/hiredis.h>
#include "libzbus.h"
#include "identityd.h"

static void diep(char *str) {
    libzbus_diep("libzbus", str);
}

int main() {
    redis_t *redis;

    printf("[+] core: initializing\n");
    libzbus_debug_set(0);

    char *host = "10.241.0.29";
    int port = 6379;

    printf("[+] core: connecting redis: %s:%d\n", host, port);

    if(!(redis = redis_new(host, port)))
        diep("redis: new");

    retfree zbus_return_t *farmid = identityd_farmid(redis);
    zbus_error_panic(farmid);

    retfree zbus_return_t *nodeid = identityd_nodeid(redis);
    zbus_error_panic(nodeid);

    retfree zbus_return_t *farmname = identityd_farm(redis);
    zbus_error_panic(farmname);

    printf("[+] core: node id  : %u\n", nodeid->v[0].val.u32);
    printf("[+] core: farm id  : %u\n", farmid->v[0].val.u32);
    printf("[+] core: farm name: %s\n", farmname->v[0].val.str);

    redis_free(redis);

    return 0;
}

