#pragma once

#include "configure.h"

#if DEBUG
#include <stdio.h>
#include <inttypes.h>

#define DPRINTF(format, ...) fprintf(stderr, "%s(%d): " format, __func__, __LINE__, ## __VA_ARGS__)
#else
#define DPRINTF(format, ...) do {} while (0)
#endif

