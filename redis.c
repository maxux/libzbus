#include <stdio.h>
#include <msgpack.h>
#include <stdint.h>
#include <hiredis/hiredis.h>
#include "libzbus.h"

//
// redis helper
//
redis_t *redis_new(char *host, int port) {
    redis_t *redis;

    if(!(redis = calloc(sizeof(redis_t), 1)))
        return libzbus_warnp("redis: calloc");

    libzbus_debug("[+] redis: connecting backend [%s:%d]\n", host, port);

    if(!(redis->ctx = redisConnect(host, port))) {
        fprintf(stderr, "[-] redis: connect: [%s:%d]: cannot initialize context\n", host, port);
        redis_free(redis);
        return NULL;
    }

    if(redis->ctx->err) {
        redis_free(redis);
        return NULL;
    }

    return redis;
}

void redis_free(redis_t *redis) {
    redisFree(redis->ctx);
    free(redis);
}

