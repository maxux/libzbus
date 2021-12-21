#ifndef LIBZBUS_H
    #define LIBZBUS_H

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

    typedef struct zbus_reply_t {
        size_t rawsize;
        char *raw;

        char *id;
        int argc;
        char **argv;
        char *error;

    } zbus_reply_t;

    typedef struct spack_t {
        msgpack_sbuffer sbuf;
        msgpack_packer pk;

    } spack_t;

    typedef struct sunpack_t {
        msgpack_unpacked und;
        msgpack_unpacker unp;
        msgpack_object *obj;

    } sunpack_t;

    // zbus request
    zbus_request_t *zbus_request_new(char *server, char *object, char *version);
    zbus_request_t *zbus_request_set_method(zbus_request_t *req, char *method);
    zbus_request_t *zbus_request_push_arg(zbus_request_t *req, char *arg);
    spack_t *zbus_request_serialize(zbus_request_t *req);
    void zbus_request_free(zbus_request_t *req);
    void zbus_request_dumps(zbus_request_t *req);

    // zbus reply
    void zbus_reply_free(zbus_reply_t *reply);
    void zbus_reply_dumps(zbus_reply_t *reply);

    // zbus protocol
    int zbus_protocol_send(redis_t *redis, zbus_request_t *req);
    zbus_reply_t *zbus_protocol_read(redis_t *redis, zbus_request_t *req);

    // redis handler
    redis_t *redis_new(char *host, int port);
    void redis_free(redis_t *redis);

    // msgpack serializer
    spack_t *spack_new();
    void spack_free(spack_t *root);
    void spack_pack_str(spack_t *sp, char *value);
    void spack_pack_kv_str(spack_t *sp, char *key, char *value);
    void spack_pack_bin(spack_t *sp, char *value, size_t len);
    void spack_pack_map(spack_t *sp, size_t len);
    void spack_pack_array(spack_t *sp, size_t len);

    // msgpack deserializer
    sunpack_t *sunpack_new(const char *buffer, size_t length);
    void sunpack_free(sunpack_t *sunpack);
    zbus_reply_t *zbus_reply_parse(zbus_reply_t *reply);
#endif
