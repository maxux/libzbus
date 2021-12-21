#include <stdio.h>
#include <msgpack.h>
#include <hiredis/hiredis.h>
#include "libzbus.h"

int main() {
    // redis_t *redis = redis_new("localhost", 6379);
    redis_t *redis = redis_new("10.241.0.29", 6379);

    zbus_request_t *req = zbus_request_new("identityd", "manager", "0.0.1");
    // zbus_request_set_method(req, "NodeID");
    // zbus_request_set_method(req, "NodeIDNumeric");
    zbus_request_set_method(req, "FarmID");

    /*
    zbus_request_t *req = zbus_request_new("server", "utils", "1.0");
    zbus_request_set_method(req, "Panic");
    */

    /*
    zbus_request_t *req = zbus_request_new("server", "utils", "1.0");
    zbus_request_set_method(req, "Capitalize");
    zbus_request_push_arg(req, "Hello World, Zbus !");
    */

    zbus_request_dumps(req);
    zbus_protocol_send(redis, req);

    zbus_reply_t *reply = zbus_protocol_read(redis, req);
    zbus_reply_parse(reply);
    zbus_reply_dumps(reply);

    zbus_reply_free(reply);
    zbus_request_free(req);

    redis_free(redis);

    return 0;
}

