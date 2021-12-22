#include <stdio.h>
#include <stdlib.h>
#include <msgpack.h>
#include <stdint.h>
#include <hiredis/hiredis.h>
#include <uuid/uuid.h>
#include "libzbus.h"

//
// zbus internal uuid generator
//
static char *zbus_uuid() {
    uuid_t binuuid;
    char uuid[37];

    uuid_generate_random(binuuid);
    uuid_unparse_lower(binuuid, uuid);

    return strdup(uuid);
}

//
// zbus request
//
zbus_request_t *zbus_request_new(char *server, char *object, char *version) {
    zbus_request_t *req;

    if(!(req = calloc(sizeof(zbus_request_t), 1)))
        return libzbus_warnp("zbus: request: calloc");

    req->target = malloc(strlen(server) + strlen(object) + strlen(version) + 4);
    sprintf(req->target, "%s.%s@%s", server, object, version);

    if(!(req->object_name = strdup(object))) {
        zbus_request_free(req);
        return libzbus_warnp("zbus: request: strdup");
    }

    if(!(req->object_version = strdup(version))) {
        zbus_request_free(req);
        return libzbus_warnp("zbus: request: strdup");
    }

    req->id = zbus_uuid();
    req->replyto = zbus_uuid();

    return req;
}

zbus_request_t *zbus_request_set_method(zbus_request_t *req, char *method) {
    if(!(req->method = strdup(method)))
        return NULL;

    return req;
}

zbus_request_t *zbus_request_push_arg(zbus_request_t *req, char *arg) {
    req->argc += 1;

    if(!(req->argv = realloc(req->argv, req->argc * sizeof(char *))))
        return NULL;

    if(!(req->argv[req->argc - 1] = strdup(arg)))
        return NULL;

    return req;
}

spack_t *zbus_request_serialize(zbus_request_t *req) {
    spack_t *root;

    if(!(root = spack_new()))
        return NULL;

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
    printf("[+] zbus: request method: %s\n", req->method);
    printf("[+] zbus: request argc  : %d\n", req->argc);
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
// zbus values
//
void zbus_values_free(zbus_value_t *values, int argc) {
    for(int i = 0; i < argc; i++) {
        if(values[i].type == ZBUS_RETURN_STR)
            free(values[i].val.str);
    }

    free(values);
}

//
// zbus reply
//
zbus_reply_t *zbus_reply_new(char *buffer, size_t length) {
    zbus_reply_t *reply;

    if(!(reply = calloc(sizeof(zbus_reply_t), 1)))
        return libzbus_warnp("zbus: reply: calloc");

    if(!(reply->raw = malloc(length))) {
        zbus_reply_free(reply);
        return libzbus_warnp("zbus: reply: raw malloc");
    }

    // copy buffer
    memcpy(reply->raw, buffer, length);
    reply->rawsize = length;

    return reply;
}

void zbus_reply_free_light(zbus_reply_t *reply) {
    free(reply->raw);
    free(reply->id);
    free(reply->error);
    free(reply);
}

void zbus_reply_free(zbus_reply_t *reply) {
    zbus_values_free(reply->argv, reply->argc);
    zbus_reply_free_light(reply);
}

zbus_reply_t *zbus_reply_parse_args(zbus_reply_t *reply, msgpack_object_array *args) {
    printf("[+] zbus: response: parsing %d arguments\n", reply->argc);

    for(size_t i = 0; i < args->size; i++) {
        sunpack_t *argpack = sunpack_new(args->ptr[i].via.str.ptr, args->ptr[i].via.str.size);

        if(!sunpack_is_supported(argpack->obj))
            printf("[-] unsupported arg type %d\n", argpack->obj->type);

        /*
        if(argpack->obj->type == MSGPACK_OBJECT_MAP) {
            // sunpack_map_dump(&argpack->obj->via.map);

            printf("Map Size: %d\n", argpack->obj->via.map.size);
            for(size_t i = 0; i < argpack->obj->via.map.size; i++) {
                msgpack_object_map *xmap = &argpack->obj->via.map;

                printf("Map KK : %.*s\n", xmap->ptr[i].key.via.str.size, xmap->ptr[i].key.via.str.ptr);
                printf("Map XX : %d\n", xmap->ptr[i].val.type);
                printf("Map SZ : %d\n", xmap->ptr[i].val.via.array.size);

                printf(">> %d\n", xmap->ptr[i].val.via.array.ptr[0].type);
            }
        }
        */

        if(sunpack_is_str(argpack->obj)) {
            reply->argv[i].type = ZBUS_RETURN_STR;
            reply->argv[i].val.str = sunpack_str(argpack->obj);
        }

        if(sunpack_is_u32(argpack->obj)) {
            reply->argv[i].type = ZBUS_RETURN_U32;
            reply->argv[i].val.u32 = sunpack_u32(argpack->obj);
        }

        sunpack_free(argpack);
    }

    return reply;
}

zbus_reply_t *zbus_reply_parse(zbus_reply_t *reply) {
    sunpack_t *repack = sunpack_new(reply->raw, reply->rawsize);

    printf("[+] zbus: response: parsing %lu bytes\n", reply->rawsize);

    if(!sunpack_is_map(repack->obj)) {
        fprintf(stderr, "[-] msgpack: could not parse, map expected\n");
        return NULL;
    }

    msgpack_object_map *map = sunpack_map_get(repack->obj);
    if(sunpack_map_size(map) != 3) {
        fprintf(stderr, "[-] msgpack: wrong map size\n");
        return NULL;
    }

    if(!sunpack_is_array(&map->ptr[1].val)) {
        fprintf(stderr, "[-] msgpack: could not parse, arguments not array\n");
        return NULL;
    }

    // copy id and error
    reply->id = sunpack_map_val_str(map, 0);
    reply->error = sunpack_map_val_str(map, 2);

    // building arguments list
    msgpack_object_array *args = sunpack_array_get(&map->ptr[1].val);

    // FIXME: memory free
    reply->argc = sunpack_array_size(args);
    if(!(reply->argv = calloc(sizeof(zbus_value_t), reply->argc)))
        return NULL;

    zbus_reply_parse_args(reply, args);

    // msgpack buffer not needed anymore
    sunpack_free(repack);

    // FIXME: if debug
    zbus_reply_dumps(reply);

    return reply;
}

void zbus_reply_dumps(zbus_reply_t *reply) {
    printf("[+] zbus: reply id   : %s\n", reply->id);
    printf("[+] zbus: reply argc : %d\n", reply->argc);
    // printf("[+] zbus: reply argv : %s\n", reply->argv[0].val.str); // FIXME
    printf("[+] zbus: reply error: %s\n", reply->error);
}

//
// zbus return value
//
zbus_return_t *zbus_return_new(int values, char *error) {
    zbus_return_t *ret;

    if(!(ret = calloc(sizeof(zbus_return_t), 1)))
        return NULL;

    ret->error = error;
    ret->length = values;

    /*
    if(!(ret->v = (zbus_value_t *) calloc(sizeof(zbus_value_t), values)))
        return NULL;
    */

    return ret;
}

zbus_return_t *zbus_return_from_reply(zbus_reply_t *reply) {
    zbus_return_t *ret;

    if(!(ret = zbus_return_new(reply->argc, NULL)))
        return NULL;

    ret->v = reply->argv;

    return ret;
}

void zbus_return_free(zbus_return_t *ret) {
    zbus_values_free(ret->v, ret->length);
    free(ret);
}

// auto free function (cleanup handler)
void __cleanup_zbus_return(void *p) {
    zbus_return_free(* (void **) p);
}

//
// zbus helpers
//
void zbus_error_panic(zbus_return_t *value) {
    if(!value) {
        printf("[-] zbus: panic: could not allocate response\n");
        exit(EXIT_FAILURE);
    }

    if(value->error) {
        printf("[-] zbus: panic: %s\n", value->error);
        exit(EXIT_FAILURE);
    }
}
