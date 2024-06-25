#pragma once

#include "shared.h"

#include <libubox/blob.h>

void
interface_update(interface_tester_shared_st * ctx);

void
interface_flush_old(interface_tester_shared_st * ctx);

bool
config_load_config(interface_tester_shared_st * ctx, struct blob_attr * config);

void
config_load_from_file_check(interface_tester_shared_st * ctx);

void
config_init(interface_tester_shared_st * ctx);

void
config_init(interface_tester_shared_st * ctx);

