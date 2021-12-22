#include <stdio.h>
#include <msgpack.h>
#include <stdint.h>
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

int sunpack_is_str(msgpack_object *obj) {
    return (obj->type == MSGPACK_OBJECT_STR);
}

char *sunpack_str(msgpack_object *obj) {
    if(obj->via.str.size == 0)
        return NULL;

    return strndup(obj->via.str.ptr, obj->via.str.size);
}

int sunpack_is_map(msgpack_object *obj) {
    return (obj->type == MSGPACK_OBJECT_MAP);
}

int sunpack_map_size(msgpack_object_map *obj) {
    return obj->size;
}

msgpack_object_map *sunpack_map_get(msgpack_object *obj) {
    return &obj->via.map;
}

char *sunpack_map_val_str(msgpack_object_map *map, int id) {
    return sunpack_str(&map->ptr[id].val);
}


int sunpack_is_array(msgpack_object *obj) {
    return (obj->type == MSGPACK_OBJECT_ARRAY);
}

msgpack_object_array *sunpack_array_get(msgpack_object *obj) {
    return &obj->via.array;
}

int sunpack_array_size(msgpack_object_array *array) {
    return array->size;
}

// only support few return value parser
int sunpack_is_supported(msgpack_object *obj) {
    if(obj->type == MSGPACK_OBJECT_STR)
        return 1;

    if(obj->type == MSGPACK_OBJECT_MAP)
        return 1;

    if(obj->type == MSGPACK_OBJECT_POSITIVE_INTEGER)
        return 1;

    return 0;
}

int sunpack_is_u32(msgpack_object *obj) {
    return (obj->type == MSGPACK_OBJECT_POSITIVE_INTEGER);
}

uint32_t sunpack_u32(msgpack_object *obj) {
    return obj->via.u64;
}
