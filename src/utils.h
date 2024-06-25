#pragma once

#include <libubus.h>

#include <errno.h>
#include <malloc.h>

#define UNCONST(ptr_type, ptr) ((ptr_type *)(ptr))

#define UNUSED(x) ((void)x)

#ifndef TEMP_FAILURE_RETRY

# define TEMP_FAILURE_RETRY(expression) \
  (__extension__                                                             \
    ({ long int __result;                                                    \
       do __result = (long int)(expression);                                 \
       while (__result == -1L && errno == EINTR);                            \
       __result; }))


#endif

static inline void free_const(void const * p)
{
    free((void *)p);
}

