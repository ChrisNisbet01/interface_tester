#pragma once

#include "tester_common.h"

char const *
interface_tester_state_to_str(interface_tester_state_t state);

char const *
interface_recovery_state_to_str(interface_recovery_state_t state);

void
interface_tester_send_event(interface_st * iface, tester_event_t event);

void
interface_tester_initialise(interface_st * iface);

void
interface_tester_begin(interface_st * iface);

void
interface_tester_cleanup(interface_st * iface);

void
interface_tester_stop(interface_st * iface);

void
interface_tester_start(interface_st * iface);

