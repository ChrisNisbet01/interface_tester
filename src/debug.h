#pragma once

#include "configure.h"

#include <libubox/ulog.h>

#include <inttypes.h>

#if DEBUG
#define DLOG(format, ...) ulog(LOG_DEBUG, format, ## __VA_ARGS__)
#else
#define DLOG(format, ...) do {} while (0)
#endif

#define ILOG(format, ...) ulog(LOG_INFO, format, ## __VA_ARGS__)

