#include "timers.h"
#include "utils.h"

void
timer_stop(timer_st * const t)
{
    uloop_timeout_cancel(&t->t_);
}

void
timer_start(timer_st * const t, uint32_t const timeout_msecs)
{
    uloop_timeout_set(&t->t_, (int)timeout_msecs);
}

bool
timer_is_running(timer_st const * const t)
{
    return t->t_.pending;
}

uint64_t
timer_remaining(timer_st const * const t)
{
    return uloop_timeout_remaining64(UNCONST(struct uloop_timeout, &t->t_));
}

static void
timer_expired_cb(struct uloop_timeout * const t)
{
    timer_st * const timer = container_of(t, timer_st, t_);

    timer->cb_(timer);
}

char const *
timer_label(timer_st const * const t)
{
    return t->label_;
}

void
timer_init(
    timer_st * const t, char const * const label, timer_expired_fn const expired_timer_cb)
{
    t->t_.cb = timer_expired_cb;
    t->label_ = label;
    t->cb_ = expired_timer_cb;
}

