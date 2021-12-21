#include <stdio.h>
#include <msgpack.h>
#include <hiredis/hiredis.h>
#include "libzbus.h"

//
// redis helper
//
redis_t *redis_new(char *host, int port) {
    redis_t *redis = calloc(sizeof(redis_t), 1);

    printf("[+] redis: connecting backend [%s:%d]\n", host, port);

    if(!(redis->ctx = redisConnect(host, port))) {
        fprintf(stderr, "redis: connect: [%s:%d]: cannot initialize context", host, port);
        free(redis);
        return NULL;
    }

    if(redis->ctx->err) {
        fprintf(stderr, "redis: connect: [%s:%d]: %s", host, port, redis->ctx->errstr);
        // free redis ctx
        free(redis);
        return NULL;
    }

    return redis;
}

void redis_free(redis_t *redis) {
    redisFree(redis->ctx);
    free(redis);
}

