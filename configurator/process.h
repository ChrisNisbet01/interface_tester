#pragma once

#include <libubox/uloop.h>
#include <libubox/blob.h>

#include <stdbool.h>

bool
event_processor_start_process(char * * argv, char const * working_dir);

