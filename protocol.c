#include <stdio.h>
#include <msgpack.h>
#include <stdint.h>
#include <hiredis/hiredis.h>
#include "libzbus.h"

//
// zbus protocol sender
//
int zbus_protocol_send(redis_t *redis, zbus_request_t *req) {
    spack_t *root;

    // FIXME: if debug
    zbus_request_dumps(req);

    if(!(root = zbus_request_serialize(req))) {
        fprintf(stderr, "[-] redis: cannot serialize request\n");
        return 1;
    }

    const char *argv[] = {"RPUSH", req->target, root->sbuf.data};
    size_t argvl[] = {5, strlen(req->target), root->sbuf.size};
    redisReply *reply;

    printf("[+] redis: sending serialized request [%lu bytes]\n", root->sbuf.size);

    if(!(reply = redisCommandArgv(redis->ctx, 3, argv, argvl))) {
        fprintf(stderr, "redis: rpush: %s", redis->ctx->errstr);
        return 1;
    }

    if(reply->type != REDIS_REPLY_INTEGER) {
        fprintf(stderr, "[-] redis: unexpected reply: %d\n", reply->type);
        return 1;
    }


    if(reply->integer != 1) {
        fprintf(stderr, "[-] redis: wrong reply, expected 1, got: %lld\n", reply->integer);
        return 1;
    }

    spack_free(root);
    freeReplyObject(reply);

    return 0;
}

//
// zbus protocol receiver
//
zbus_reply_t *zbus_protocol_read(redis_t *redis, zbus_request_t *req) {
    redisReply *reply;

    const char *argv[] = {"BLPOP", req->replyto, ZBUS_PROTOCOL_TIMEOUT};

    printf("[+] redis: wait reply on: %s\n", req->replyto);

    if(!(reply = redisCommandArgv(redis->ctx, 3, argv, NULL))) {
        fprintf(stderr, "[-] redis: blpop: %s", redis->ctx->errstr);
        return NULL;
    }

    if(reply->type == REDIS_REPLY_ERROR) {
        fprintf(stderr, "[-] redis: reply: error: %s\n", reply->str);
        return NULL;
    }

    if(reply->type == REDIS_REPLY_NIL) {
        freeReplyObject(reply);
        return NULL;
    }

    if(reply->type != REDIS_REPLY_ARRAY) {
        fprintf(stderr, "[-] redis: reply: wrong array response: %d\n", reply->type);
        return NULL;
    }

    if(reply->elements != 2) {
        fprintf(stderr, "[-] redis: reply: wrong size reply %ld\n", reply->elements);
        return NULL;
    }

    // blpop returns an array
    // first entry is the key, second is the payload
    redisReply *sub = reply->element[1];

    if(sub->type != REDIS_REPLY_STRING) {
        fprintf(stderr, "[-] redis: reply: string expected for blpop\n");
        return NULL;
    }

    // FIXME: double allocation
    zbus_reply_t *zreply = zbus_reply_new(sub->str, sub->len);

    freeReplyObject(reply);

    return zreply;
}



