#pragma once

#include <libubox/uloop.h>

#include <stdint.h>

typedef struct timer_st timer_st;
typedef void (*timer_expired_fn)(timer_st * t);

struct timer_st
{
    /* Users should not access the fields within this structure directly. */
    struct uloop_timeout t_;
    char const * label_;
    timer_expired_fn cb_;
};

void
timer_stop(timer_st * t);

void
timer_start(timer_st * t, uint32_t timeout_msecs);

bool
timer_is_running(timer_st const * t);

uint64_t
timer_remaining(timer_st const * t);

char const *
timer_label(timer_st const * t);

void
timer_init(timer_st * t, char const * label, timer_expired_fn expired_timer_cb);

