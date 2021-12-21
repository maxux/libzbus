#include <stdio.h>
#include <msgpack.h>
#include <hiredis/hiredis.h>
#include "libzbus.h"

//
// msgpack serializer
//
spack_t *spack_new() {
    spack_t *spack = malloc(sizeof(spack_t));
    msgpack_sbuffer_init(&spack->sbuf);
    msgpack_packer_init(&spack->pk, &spack->sbuf, msgpack_sbuffer_write);

    return spack;
}

void spack_free(spack_t *root) {
    msgpack_sbuffer_destroy(&root->sbuf);
    free(root);
}

void spack_pack_str(spack_t *sp, char *value) {
    size_t len = strlen(value);

    msgpack_pack_str(&sp->pk, len);
    msgpack_pack_str_body(&sp->pk, value, len);
}

void spack_pack_kv_str(spack_t *sp, char *key, char *value) {
    spack_pack_str(sp, key);
    spack_pack_str(sp, value);
}

void spack_pack_bin(spack_t *sp, char *value, size_t len) {
    msgpack_pack_bin(&sp->pk, len);
    msgpack_pack_bin_body(&sp->pk, value, len);
}

void spack_pack_map(spack_t *sp, size_t len) {
    msgpack_pack_map(&sp->pk, len);
}

void spack_pack_array(spack_t *sp, size_t len) {
    msgpack_pack_array(&sp->pk, len);
}


//
// msgpack deserializer
//
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
    msgpack_unpacked_destroy(&sunpack->und);
    msgpack_unpacker_destroy(&sunpack->unp);
    free(sunpack);
}

/*
void sunpack_map_dump(msgpack_object_map *map) {
    for(int i = 0; i < map->size; i++) {
        if(map->ptr[i].key.type == MSGPACK_OBJECT_STR)
            printf(">> MAP: %.*s\n", map->ptr[i].key.via.str);

        if(map->ptr[i].val.type == MSGPACK_OBJECT_MAP)
            sunpack_map_dump(&map->ptr[i].val.via.map);

        if(map->ptr[i].val.type == MSGPACK_OBJECT_ARRAY) {
            for(int i = 0; i < map->ptr[i].val.via.array.size; i++) {
                if(map->ptr[i].val.via.array.ptr[i].type == MSGPACK_OBJECT_MAP)
                    sunpack_map_dump(&map->ptr[i].val.via.array.ptr[i].via.map);

                if(map->ptr[i].val.via.array.ptr[i].type == MSGPACK_OBJECT_STR)
                    printf("<< MAP STR: %.*s\n", map->ptr[i].val.via.array.ptr[i].via.str.size, map->ptr[i].val.via.array.ptr[i].via.str.ptr);
            }
        }

        if(map->ptr[i].val.type == MSGPACK_OBJECT_STR) {
            printf("<< MAP STR: %.*s\n", map->ptr[i].val.via.str.size, map->ptr[i].val.via.str.ptr);
        }
    }
}
*/

zbus_reply_t *zbus_reply_parse(zbus_reply_t *reply) {
    sunpack_t *repack = sunpack_new(reply->raw, reply->rawsize);

    printf("[+] zbus: response: parsing %lu bytes\n", reply->rawsize);

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

    printf("[+] zbus: response: parsing %d arguments\n", args->size);

    for(size_t i = 0; i < args->size; i++) {
        sunpack_t *argpack = sunpack_new(args->ptr[i].via.str.ptr, args->ptr[i].via.str.size);

        if(argpack->obj->type != MSGPACK_OBJECT_STR && argpack->obj->type != MSGPACK_OBJECT_MAP)
            printf("[-] unsupported arg type %d\n", argpack->obj->type);

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

        if(argpack->obj->type == MSGPACK_OBJECT_STR)
            reply->argv[i] = strndup(argpack->obj->via.str.ptr, argpack->obj->via.str.size);

        sunpack_free(argpack);

    }

    reply->argc = args->size;

    sunpack_free(repack);

    return reply;
}


