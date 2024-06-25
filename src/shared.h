#pragma once

#include <libubox/vlist.h>

#include <libubus.h>

/* Data shared by all interface tester contexts. */
typedef struct interface_tester_shared_st
{
    struct vlist_tree interfaces;
    struct ubus_auto_conn ubus_conn;
    struct ubus_event_handler interface_events;
    struct ubus_event_handler interface_state_events;
    char const * test_directory;
    char const * recovery_directory;
    char const * config_file;
} interface_tester_shared_st;

