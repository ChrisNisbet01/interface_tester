#include "event_queue.h"
#include "debug.h"
#include "tester_common.h"

#include <libubox/utils.h>

static void
handle_events(event_q_st * const event_queue)
{
    if (event_queue->num_events > 1)
    {
        /*
         * If there is more than one event on the queue then that means that
         * this call to handle events isn't necessary as all existing events on
         * the queue will be processed by the caller that added the first
         * message to the queue.
         */
        goto done;
    }

    for (size_t event_index = 0; event_index < event_queue->num_events; event_index++)
    {
        event_st const * const e = &event_queue->events[event_index];

        e->handler(e->event_ctx, e->event);
    }

    event_queue->num_events = 0;

done:
    return;
}

void
event_queue_add_event(
    event_q_st * const event_queue,
    event_handler_fn const handler,
    void * const event_ctx,
    tester_event_t const event)
{
    if (event_queue->num_events >= ARRAY_SIZE(event_queue->events))
    {
        DPRINTF("unable to stack the next event (%s)\n",
                tester_event_to_str(event));

        goto done;
    }

    event_st * const e = &event_queue->events[event_queue->num_events];

    e->handler = handler;
    e->event_ctx = event_ctx;
    e->event = event;
    event_queue->num_events++;

    handle_events(event_queue);

done:
    return;
}

void
event_queue_init(event_q_st * const event_queue)
{
    event_queue->num_events = 0;
}
