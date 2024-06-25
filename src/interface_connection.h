#pragma once

#include "tester_common.h"

char const *
interface_connection_state_to_str(interface_connection_state_t state);

void
interface_connection_init(interface_connection_st * connection);

void
interface_connection_begin(interface_connection_st * connection);

void
interface_connection_cleanup(interface_connection_st * connection);

void
interface_connection_connected(interface_connection_st * connection);

void
interface_connection_disconnected(interface_connection_st * connection);

