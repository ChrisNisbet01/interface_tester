#pragma once

#include "configure.h"
#include "tester_common.h"

#include <libubus.h>

#include <stdbool.h>

typedef void (*ubus_connect_cb)(struct ubus_context * ubus);

void
interface_tester_send_up_down_event(struct ubus_context * ubus, bool are_up);

void
ubus_send_interface_operational_event(
    struct ubus_context * ubus, char const * interface_name, bool is_operational);

void
ubus_send_interface_test_run_event(
    struct ubus_context * ubus, char const * interface_name, bool test_run_passed);

void
ubus_subscribe_to_interface_events(
    struct ubus_context * ubus, struct ubus_event_handler * interface_events_ctx);

void
ubus_subscribe_to_interface_state_events(
    struct ubus_context * ubus, struct ubus_event_handler * interface_events_ctx);

bool
interface_get_current_state(struct ubus_context * ubus, char const * interface_name);

#if WITH_METRICS_ADJUSTMENT
bool
ubus_send_metrics_adjust_request(
    struct ubus_context * ubus, char const * interface_name, uint32_t amount);
#endif

bool
ubus_add_interface_object(interface_st * iface);

void
ubus_remove_interface_object(interface_st * iface);

bool
ubus_add_main_object(struct ubus_context * ubus);

void
ubus_init(
    struct ubus_auto_conn * ubus_auto, char const * path, ubus_connect_cb handler);

