#pragma once

#include "interface_tester_events.h"

#include <libubox/uloop.h>

#include <stdbool.h>

/* Maximum number of events expected to be stacked at any one time. */
#define MAX_EVENTS 3

typedef void (*event_handler_fn)(void * state_ctx, tester_event_t event);

typedef struct event_st
{
    struct list_head entry;
    event_handler_fn handler;
    void * event_ctx;
    tester_event_t event;
} event_st;

typedef struct event_q_st
{
    size_t num_events;
    event_st events[MAX_EVENTS];
} event_q_st;


/* Initialise the event queue. */
void
event_queue_init(event_q_st * event_queue);

/*
 * Add an event onto the queue and handle the events on it if not already doing
 * so.
 */
void
event_queue_add_event(
    event_q_st * event_queue,
    event_handler_fn handler,
    void * event_ctx,
    tester_event_t event);

/* Empty the event queue without handling any events that happen to be on it. */
void
event_queue_cleanup(event_q_st * event_queue);

