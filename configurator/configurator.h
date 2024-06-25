#pragma once

#include <libubox/uloop.h>
#include <libubox/vlist.h>

#include <libubus.h>

typedef struct configurator_st
{
    struct ubus_auto_conn ubus_conn;
    struct ubus_event_handler interface_events;
    char const * config_path;
    char const * event_processor_path;
} configurator_st;

