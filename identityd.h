#ifndef IDENTITYD_STUBS_H
    #define IDENTITYD_STUBS_H

    zbus_return_t *identityd_nodeid(redis_t *redis);
    zbus_return_t *identityd_farmid(redis_t *redis);
    zbus_return_t *identityd_farm(redis_t *redis);
#endif
