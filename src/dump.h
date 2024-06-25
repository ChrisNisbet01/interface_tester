#pragma once

#include "tester_common.h"

#include <libubox/blob.h>

void
interface_state_dump(interface_st * iface, struct blob_buf * b);

void
interface_states_dump(
    interface_tester_shared_st * const ctx, struct blob_buf * const b);

