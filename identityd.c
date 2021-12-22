#include <stdio.h>
#include <msgpack.h>
#include <hiredis/hiredis.h>
#include <stdint.h>
#include "libzbus.h"

static char *identityd_server = "identityd";
static char *identityd_object = "manager";
static char *identityd_version = "0.0.1";

//
// identityd stubs
//

static zbus_return_t *identityd_noargs(redis_t *redis, char *method) {
    zbus_request_t *req = zbus_request_new(identityd_server, identityd_object, identityd_version);
    zbus_request_set_method(req, method);

    zbus_reply_t *reply = zbus_protocol_issue(redis, req);
    if(!reply)
        return NULL;

    zbus_return_t *ret = zbus_return_from_reply(reply);

    zbus_reply_free_light(reply);
    zbus_request_free(req);

    return ret;

}

// returns node id (numeric)
zbus_return_t *identityd_nodeid(redis_t *redis) {
    return identityd_noargs(redis, "NodeIDNumeric");
}

// returns farm name
zbus_return_t *identityd_farm(redis_t *redis) {
    return identityd_noargs(redis, "Farm");
}

// returns farm id
zbus_return_t *identityd_farmid(redis_t *redis) {
    return identityd_noargs(redis, "FarmID");
}

