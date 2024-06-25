#pragma once

#include <libubus.h>

#include <stdbool.h>

typedef void (*ubus_connect_cb)(struct ubus_context * ubus);

void
ubus_subscribe_to_tester_events(
    struct ubus_context * ubus, struct ubus_event_handler * interface_events_ctx);

void
ubus_init(
    struct ubus_auto_conn * ubus_auto, char const * path, ubus_connect_cb handler);
