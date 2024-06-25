#include "interface_connection.h"
#include "debug.h"
#include "event_queue.h"
#include "interface_tester.h"
#include "timers.h"
#include "ubus.h"
#include "utils.h"

#ifdef DEBUG
#include <assert.h>
#endif

char const *
interface_connection_state_to_str(interface_connection_state_t const state)
{
    static char const * states[CONNECTION_STATE_COUNT__] =
    {
    [CONNECTION_STATE_DISCONNECTED] = "disconnected",
    [CONNECTION_STATE_SETTLING] = "settling",
    [CONNECTION_STATE_CONNECTED] = "connected",
    };

#ifdef DEBUG
    assert(state < ARRAY_SIZE(states));
    assert(states[state] != NULL);
#endif

    return states[state];
}

static void
connection_state_transition(
    interface_connection_st * const connection,
    interface_connection_state_t const new_state)
{
#ifdef DEBUG
    interface_st * const iface = container_of(connection, interface_st, connection);
#endif
    DPRINTF("%s: change state from %s -> %s\n",
            iface->name,
            interface_connection_state_to_str(connection->state),
            interface_connection_state_to_str(new_state));

    connection->state = new_state;
}

static void
settling_delay_timer_stop(interface_connection_st * const connection)
{
    timer_st * const t = &connection->settling_delay_timer;
#ifdef DEBUG
    interface_st * const iface = container_of(connection, interface_st, connection);
#endif

    DPRINTF("%s\n", iface->name);

    timer_stop(t);
}

static void
settling_delay_timer_expired(timer_st * const t)
{
    interface_connection_st * const connection =
        container_of(t, interface_connection_st, settling_delay_timer);
    interface_st * const iface = container_of(connection, interface_st, connection);

    DPRINTF("%s\n", iface->name);

    if (connection->state == CONNECTION_STATE_SETTLING)
    {
        connection_state_transition(connection, CONNECTION_STATE_CONNECTED);
        interface_tester_send_event(iface, TESTER_EVENT_INTERFACE_SETTLED);
    }
}

static void
settling_delay_timer_start(interface_connection_st * const connection)
{
    timer_st * const t = &connection->settling_delay_timer;
    interface_st * const iface = container_of(connection, interface_st, connection);
    uint32_t const settling_delay_msecs =
        iface->config.settling_delay_secs * msecs_per_sec;

    DPRINTF("%s: delay: %u msecs\n", iface->name, settling_delay_msecs);

    timer_start(t, settling_delay_msecs);
}

void
interface_connection_init(interface_connection_st * const connection)
{
#ifdef DEBUG
    interface_st * const iface = container_of(connection, interface_st, connection);
#endif

    DPRINTF("%s\n", iface->name);
    timer_init(
        &connection->settling_delay_timer, "settling_delay_timer", settling_delay_timer_expired);
    connection_state_transition(connection, CONNECTION_STATE_DISCONNECTED);
}

void
interface_connection_begin(interface_connection_st * const connection)
{
    interface_st * const iface = container_of(connection, interface_st, connection);
    bool const is_connected =
        interface_get_current_state(&iface->ctx->ubus_conn.ctx, iface->name);

    is_connected
        ? interface_connection_connected(connection)
        : interface_connection_disconnected(connection);
}

void
interface_connection_cleanup(interface_connection_st * const connection)
{
#ifdef DEBUG
    interface_st * const iface = container_of(connection, interface_st, connection);
#endif

    DPRINTF("%s\n", iface->name);

    settling_delay_timer_stop(connection);
}

void
interface_connection_connected(interface_connection_st * const connection)
{
#ifdef DEBUG
    interface_st * const iface = container_of(connection, interface_st, connection);
#endif

    DPRINTF("%s\n", iface->name);

    if (connection->state == CONNECTION_STATE_DISCONNECTED)
    {
        settling_delay_timer_start(connection);
        connection_state_transition(connection, CONNECTION_STATE_SETTLING);
    }
}

void
interface_connection_disconnected(interface_connection_st * const connection)
{
    interface_st * const iface = container_of(connection, interface_st, connection);

    DPRINTF("%s\n", iface->name);

    if (connection->state != CONNECTION_STATE_DISCONNECTED)
    {
        bool const was_connected =
            connection->state == CONNECTION_STATE_CONNECTED;

        connection_state_transition(connection, CONNECTION_STATE_DISCONNECTED);
        settling_delay_timer_stop(connection);
        if (was_connected)
        {
            interface_tester_send_event(iface, TESTER_EVENT_INTERFACE_DISCONNECTED);
        }
    }
}

