#include <stdio.h>
#include <msgpack.h>
#include <hiredis/hiredis.h>
#include <uuid/uuid.h>

//
// msgpack wrapper
//
typedef struct spack_t {
    msgpack_sbuffer sbuf;
    msgpack_packer pk;

} spack_t;

spack_t *spack_new() {
    spack_t *spack = malloc(sizeof(spack_t));
    msgpack_sbuffer_init(&spack->sbuf);
    msgpack_packer_init(&spack->pk, &spack->sbuf, msgpack_sbuffer_write);

    return spack;
}

void spack_free(spack_t *root) {
    msgpack_sbuffer_destroy(&root->sbuf);
}

static void spack_pack_str(spack_t *sp, char *value) {
    size_t len = strlen(value);

    msgpack_pack_str(&sp->pk, len);
    msgpack_pack_str_body(&sp->pk, value, len);
}

static void spack_pack_kv_str(spack_t *sp, char *key, char *value) {
    spack_pack_str(sp, key);
    spack_pack_str(sp, value);
}

static void spack_pack_bin(spack_t *sp, char *value, size_t len) {
    msgpack_pack_bin(&sp->pk, len);
    msgpack_pack_bin_body(&sp->pk, value, len);
}

static void spack_pack_map(spack_t *sp, size_t len) {
    msgpack_pack_map(&sp->pk, len);
}

static void spack_pack_array(spack_t *sp, size_t len) {
    msgpack_pack_array(&sp->pk, len);
}

//
// zbus request handler
//
typedef struct zbus_request_t {
    char *target;
    char *id;
    int argc;
    char **argv;
    char *object_name;
    char *object_version;
    char *replyto;
    char *method;

} zbus_request_t;

char *zbus_uuid() {
    uuid_t binuuid;
    char uuid[37];

    uuid_generate_random(binuuid);
    uuid_unparse_lower(binuuid, uuid);

    return strdup(uuid);
}

zbus_request_t *zbus_request_new(char *target) {
    zbus_request_t *req = calloc(sizeof(zbus_request_t), 1);

    req->target = strdup(target);
    req->id = zbus_uuid();
    req->replyto = zbus_uuid();

    return req;
}

zbus_request_t *zbus_request_set_object(zbus_request_t *req, char *name, char *version) {
    req->object_name = strdup(name);
    req->object_version = strdup(version);

    return req;
}

zbus_request_t *zbus_request_set_method(zbus_request_t *req, char *method) {
    req->method = strdup(method);

    return req;
}

zbus_request_t *zbus_request_push_arg(zbus_request_t *req, char *arg) {
    req->argc += 1;

    req->argv = realloc(req->argv, req->argc * sizeof(char *));
    req->argv[req->argc - 1] = strdup(arg);

    return req;
}

spack_t *zbus_request_serialize(zbus_request_t *req) {
    spack_t *root = spack_new();

    spack_pack_map(root, 5);
    spack_pack_kv_str(root, "ID", req->id);

    spack_pack_str(root, "Arguments");
    spack_pack_array(root, req->argc);

    for(int i = 0; i < req->argc; i++) {
        spack_t *arg = spack_new();
        spack_pack_str(arg, req->argv[i]);
        spack_pack_bin(root, arg->sbuf.data, arg->sbuf.size);
        spack_free(arg);
    }

    spack_pack_str(root, "Object");
    spack_pack_map(root, 2);
    spack_pack_kv_str(root, "Name", req->object_name);
    spack_pack_kv_str(root, "Version", req->object_version);
    spack_pack_kv_str(root, "ReplyTo", req->replyto);
    spack_pack_kv_str(root, "Method", req->method);

    return root;
}

void zbus_request_dumps(zbus_request_t *req) {
    printf("[+] zbus: request id    : %s\n", req->id);
    printf("[+] zbus: request target: %s\n", req->target);
    printf("[+] zbus: request reply : %s\n", req->replyto);
    printf("[+] zbus: request object: %s [%s]\n", req->object_name, req->object_version);
    printf("[+] zbus: request method: %s\n", req->method);
    printf("[+] zbus: request args  : %d\n", req->argc);
}

void zbus_request_free(zbus_request_t *req) {
    for(int i = 0; i < req->argc; i++)
        free(req->argv[i]);

    free(req->argv);
    free(req->target);
    free(req->id);
    free(req->replyto);
    free(req->object_name);
    free(req->object_version);
    free(req->method);
    free(req);
}

//
// zbus reply handler
//
typedef struct zbus_reply_t {
    size_t rawsize;
    char *raw;

    char *id;
    int argc;
    char **argv;
    char *error;

} zbus_reply_t;

//
// redis handler
//
typedef struct redis_t {
    redisContext *ctx;

} redis_t;

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

int zbus_protocol_send(redis_t *redis, zbus_request_t *req) {
    spack_t *root;

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

zbus_reply_t *zbus_protocol_read(redis_t *redis, zbus_request_t *req) {
    zbus_reply_t *zreply;
    redisReply *reply;

    if(!(zreply = calloc(sizeof(zbus_reply_t), 1))) {
        return NULL;
    }

    const char *argv[] = {"BLPOP", req->replyto, "10"};

    printf("[+] redis: wait reply on: %s\n", req->replyto);

    if(!(reply = redisCommandArgv(redis->ctx, 3, argv, NULL))) {
        fprintf(stderr, "[-] redis: blpop: %s", redis->ctx->errstr);
        return NULL;
    }

    if(reply->type == REDIS_REPLY_ERROR) {
        fprintf(stderr, "[-] redis: reply: error: %s\n", reply->str);
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

    zreply->raw = sub->str;
    zreply->rawsize = sub->len;

    return zreply;
}





typedef struct sunpack_t {
    msgpack_unpacked und;
    msgpack_unpacker unp;
    msgpack_object *obj;

} sunpack_t;

sunpack_t *sunpack_new(const char *buffer, size_t length) {
    sunpack_t *sunpack = calloc(sizeof(sunpack_t), 1);
    msgpack_unpacked_init(&sunpack->und);

    if(!msgpack_unpacker_init(&sunpack->unp, 128)) {
        fprintf(stderr, "[-] msgpack unpack init error\n");
        return NULL;
    }

    if(msgpack_unpacker_buffer_capacity(&sunpack->unp) < length) {
        if(!msgpack_unpacker_reserve_buffer(&sunpack->unp, length)) {
            fprintf(stderr, "[-] msgpack reserve buffer failed\n");
            return NULL;
        }
    }

    memcpy(msgpack_unpacker_buffer(&sunpack->unp), buffer, length);
    msgpack_unpacker_buffer_consumed(&sunpack->unp, length);

    msgpack_unpack_return ret;
    ret = msgpack_unpacker_next(&sunpack->unp, &sunpack->und);

    if(ret == MSGPACK_UNPACK_CONTINUE) {
        fprintf(stderr, "[-] msgpack: not enough space for parser\n");
        return NULL;
    }

    if(ret == MSGPACK_UNPACK_PARSE_ERROR) {
        fprintf(stderr, "[-] msgpack: parser error\n");
        return NULL;
    }

    if(ret != MSGPACK_UNPACK_SUCCESS) {
        fprintf(stderr, "[-] msgpack: unknown error\n");
        return NULL;
    }

    // linking object data
    sunpack->obj = &sunpack->und.data;

    return sunpack;
}

void sunpack_free(sunpack_t *sunpack) {
    msgpack_unpacker_destroy(&sunpack->unp);
}

zbus_reply_t *zbus_reply_parse(zbus_reply_t *reply) {
    sunpack_t *repack = sunpack_new(reply->raw, reply->rawsize);

    printf("[+] zbus: parsing response [%lu bytes]\n", reply->rawsize);

    if(repack->obj->type != MSGPACK_OBJECT_MAP) {
        fprintf(stderr, "[-] msgpack: could not parse, map expected\n");
        return NULL;
    }

    msgpack_object_map *map = &repack->obj->via.map;
    if(map->size != 3) {
        fprintf(stderr, "[-] msgpack: wrong map size\n");
        return NULL;
    }

    // copy id
    reply->id = strndup(map->ptr[0].val.via.str.ptr, map->ptr[0].val.via.str.size);

    // only set error if set
    if(map->ptr[2].val.via.str.size > 0)
        reply->error = strndup(map->ptr[2].val.via.str.ptr, map->ptr[2].val.via.str.size);

    // building arguments list
    msgpack_object_array *args = &map->ptr[1].val.via.array;
    reply->argv = calloc(sizeof(char *), args->size);

    for(int i = 0; i < args->size; i++) {
        sunpack_t *argpack = sunpack_new(args->ptr[i].via.str.ptr, args->ptr[i].via.str.size);

        if(argpack->obj->type == MSGPACK_OBJECT_STR)
            reply->argv[i] = strndup(argpack->obj->via.str.ptr, argpack->obj->via.str.size);

        sunpack_free(argpack);

    }

    reply->argc = args->size;

    sunpack_free(repack);

    return reply;
}

void zbus_reply_dumps(zbus_reply_t *reply) {
    printf("[+] zbus: reply id   : %s\n", reply->id);
    printf("[+] zbus: reply argc : %d\n", reply->argc);
    printf("[+] zbus: reply argv : %s\n", reply->argv[0]);
    printf("[+] zbus: reply error: %s\n", reply->error);
}

int main() {
    redis_t *redis = redis_new("localhost", 6379);

    zbus_request_t *req = zbus_request_new("server.utils@1.0");
    zbus_request_set_object(req, "utils", "1.0");
    zbus_request_set_method(req, "Capitalize");
    zbus_request_push_arg(req, "Hello World, Zbus !");
    zbus_request_dumps(req);

    zbus_protocol_send(redis, req);

    zbus_reply_t *reply = zbus_protocol_read(redis, req);
    zbus_reply_parse(reply);
    zbus_reply_dumps(reply);

    return 0;
}

