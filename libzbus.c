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

    if(!(redis = redis_new("10.241.0.29", 6379)))
        diep("redis: new");

    retfree zbus_return_t *farmid = identityd_farmid(redis);
    zbus_error_panic(farmid);

    retfree zbus_return_t *nodeid = identityd_nodeid(redis);
    zbus_error_panic(nodeid);

    retfree zbus_return_t *farmname = identityd_farm(redis);
    zbus_error_panic(farmname);

    printf("[+] node id  : %u\n", nodeid->v[0].val.u32);
    printf("[+] farm id  : %u\n", farmid->v[0].val.u32);
    printf("[+] farm name: %s\n", farmname->v[0].val.str);

    redis_free(redis);

    return 0;
}

