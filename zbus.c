#include <stdio.h>
#include <msgpack.h>
#include <hiredis/hiredis.h>
#include <uuid/uuid.h>
#include "libzbus.h"

//
// zbus uuid generator
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
    zbus_request_t *req = calloc(sizeof(zbus_request_t), 1);

    req->target = malloc(strlen(server) + strlen(object) + strlen(version) + 4);
    sprintf(req->target, "%s.%s@%s", server, object, version);

    req->object_name = strdup(object);
    req->object_version = strdup(version);
    req->id = zbus_uuid();
    req->replyto = zbus_uuid();

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
// zbus reply
//
void zbus_reply_free(zbus_reply_t *reply) {
    free(reply->raw);

    free(reply->id);
    free(reply->error);

    for(int i = 0; i < reply->argc; i++)
        free(reply->argv[i]);

    free(reply->argv);
    free(reply);
}

void zbus_reply_dumps(zbus_reply_t *reply) {
    printf("[+] zbus: reply id   : %s\n", reply->id);
    printf("[+] zbus: reply argc : %d\n", reply->argc);
    printf("[+] zbus: reply argv : %s\n", reply->argv[0]);
    printf("[+] zbus: reply error: %s\n", reply->error);
}


