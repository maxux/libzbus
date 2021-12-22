#ifndef LIBZBUS_H
    #define LIBZBUS_H

    #define ZBUS_PROTOCOL_TIMEOUT    "10"

    typedef struct redis_t {
        redisContext *ctx;

    } redis_t;

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

    typedef enum {
        ZBUS_RETURN_NIL,
        ZBUS_RETURN_U32,
        ZBUS_RETURN_STR,

    } zbus_type_t;

    typedef union zbus_obj_t {
        uint32_t u32;
        char *str;

    } zbus_obj_t;

    typedef struct zbus_value_t {
        zbus_type_t type;
        zbus_obj_t val;

    } zbus_value_t;

    typedef struct zbus_return_t {
        char *error;
        int length;
        zbus_value_t *v; // values

    } zbus_return_t;

    typedef struct zbus_reply_t {
        size_t rawsize;
        char *raw;

        char *id;
        int argc;
        zbus_value_t *argv;
        char *error;

    } zbus_reply_t;


    // internal pack serializer
    typedef struct spack_t {
        msgpack_sbuffer sbuf;
        msgpack_packer pk;

    } spack_t;

    // internal pack deserializer
    typedef struct sunpack_t {
        msgpack_unpacked und;
        msgpack_unpacker unp;
        msgpack_object *obj;

    } sunpack_t;

    // automatic return free helper
    #define retfree __attribute__((cleanup(__cleanup_zbus_return)))
    void __cleanup_zbus_return(void *p);

    // zbus request
    zbus_request_t *zbus_request_new(char *server, char *object, char *version);
    zbus_request_t *zbus_request_set_method(zbus_request_t *req, char *method);
    zbus_request_t *zbus_request_push_arg(zbus_request_t *req, char *arg);
    spack_t *zbus_request_serialize(zbus_request_t *req);
    void zbus_request_free(zbus_request_t *req);
    void zbus_request_dumps(zbus_request_t *req);

    // zbus reply
    zbus_reply_t *zbus_reply_new(char *buffer, size_t length);
    zbus_reply_t *zbus_reply_parse(zbus_reply_t *reply);
    void zbus_reply_dumps(zbus_reply_t *reply);
    void zbus_reply_free_light(zbus_reply_t *reply);
    void zbus_reply_free(zbus_reply_t *reply);

    // zbus return value
    zbus_return_t *zbus_return_new(int values, char *error);
    zbus_return_t *zbus_return_from_reply(zbus_reply_t *reply);
    void zbus_return_free(zbus_return_t *ret);

    // zbus helper
    void zbus_values_free(zbus_value_t *values, int argc);
    void zbus_error_panic(zbus_return_t *value);

    // zbus protocol
    int zbus_protocol_send(redis_t *redis, zbus_request_t *req);
    zbus_reply_t *zbus_protocol_read(redis_t *redis, zbus_request_t *req);
    zbus_reply_t *zbus_protocol_issue(redis_t *redis, zbus_request_t *req);

    // redis handler
    redis_t *redis_new(char *host, int port);
    void redis_free(redis_t *redis);

    // msgpack serializer
    spack_t *spack_new();
    void spack_pack_str(spack_t *sp, char *value);
    void spack_pack_kv_str(spack_t *sp, char *key, char *value);
    void spack_pack_bin(spack_t *sp, char *value, size_t len);
    void spack_pack_map(spack_t *sp, size_t len);
    void spack_pack_array(spack_t *sp, size_t len);
    void spack_free(spack_t *root);

    // msgpack deserializer
    sunpack_t *sunpack_new(const char *buffer, size_t length);
    int sunpack_is_supported(msgpack_object *obj);
    int sunpack_is_str(msgpack_object *obj);
    char *sunpack_str(msgpack_object *obj);
    int sunpack_is_u32(msgpack_object *obj);
    uint32_t sunpack_u32(msgpack_object *obj);
    int sunpack_is_map(msgpack_object *obj);
    int sunpack_map_size(msgpack_object_map *obj);
    msgpack_object_map *sunpack_map_get(msgpack_object *obj);
    char *sunpack_map_val_str(msgpack_object_map *map, int id);
    int sunpack_is_array(msgpack_object *obj);
    msgpack_object_array *sunpack_array_get(msgpack_object *obj);
    int sunpack_array_size(msgpack_object_array *array);
    void sunpack_free(sunpack_t *sunpack);


    // debugging
    void libzbus_diep(char *prefix, char *str);
    void *libzbus_warnp(char *str);
#endif
