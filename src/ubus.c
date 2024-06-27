#include "ubus.h"
#include "config.h"
#include "debug.h"
#include "dump.h"
#include "tester_common.h"
#include "interface_connection.h"
#include "shared.h"
#include "strings.h"
#include "utils.h"

#include <inttypes.h>

static int const UBUS_TIMEOUT_MS = 5000;

void
interface_tester_send_up_down_event(
    struct ubus_context * const ubus, bool const are_up)
{
    /* Tell the system that the tester is up|down. */
    if (ubus->sock.fd < 0)
    {
        goto done;
    }

    struct blob_buf b = { 0 };

    blob_buf_init(&b, 0);
    blobmsg_add_string(&b, "state", are_up ? "up" : "down");
    ubus_send_event(ubus, Sinterface_tester, b.head);
    blob_buf_free(&b);

done:
    return;
}

void
ubus_send_interface_operational_event(
    struct ubus_context * const ubus,
    char const * const interface_name,
    bool const is_operational)
{
    if (ubus->sock.fd < 0)
    {
        goto done;
    }

    struct blob_buf b = { 0 };

    blob_buf_init(&b, 0);
    blobmsg_add_u8(&b, "is_operational", is_operational);
    blobmsg_add_string(&b, "interface", interface_name);
    ubus_send_event(ubus, "interface.tester.operational", b.head);
    blob_buf_free(&b);

done:
    return;
}

void
ubus_send_interface_test_run_event(
    struct ubus_context * const ubus,
    char const * const interface_name,
    bool const test_run_passed)
{
    if (ubus->sock.fd < 0)
    {
        goto done;
    }

    struct blob_buf b = { 0 };

    blob_buf_init(&b, 0);
    blobmsg_add_string(&b, "result", test_run_passed ? "pass" : "fail");
    blobmsg_add_string(&b, "interface", interface_name);
    ubus_send_event(ubus, "interface.tester.test_run", b.head);
    blob_buf_free(&b);

done:
    return;
}

static char const interface_monitor_event_id[] = "network.interface";
static char const interface_state_event_id[] = "interface.state";

typedef enum network_interface_event_policy_t
{
    NETWORK_INTERFACE_EVENT_ACTION,
    NETWORK_INTERFACE_EVENT_INTERFACE,
    NETWORK_INTERFACE_EVENT_COUNT__,
} network_interface_event_policy_t;

static const struct blobmsg_policy
    network_interface_event_policy[NETWORK_INTERFACE_EVENT_COUNT__] =
{
    [NETWORK_INTERFACE_EVENT_ACTION] = { .name = "action", .type = BLOBMSG_TYPE_STRING },
    [NETWORK_INTERFACE_EVENT_INTERFACE] = { .name = "interface", .type = BLOBMSG_TYPE_STRING },
};

static void
network_interface_event_handler_cb(
    struct ubus_context * const ubus,
    struct ubus_event_handler * const ev,
    const char * const type,
    struct blob_attr * const msg)
{
    UNUSED(ev);
    UNUSED(type);
    interface_tester_shared_st * const ctx =
        container_of(ubus, interface_tester_shared_st, ubus_conn.ctx);
    struct blob_attr * tb[NETWORK_INTERFACE_EVENT_COUNT__];

    blobmsg_parse(network_interface_event_policy, NETWORK_INTERFACE_EVENT_COUNT__, tb,
                  blobmsg_data(msg), blobmsg_data_len(msg));

    char const * const action =
        tb[NETWORK_INTERFACE_EVENT_ACTION] != NULL
        ? blobmsg_get_string(tb[NETWORK_INTERFACE_EVENT_ACTION])
        : NULL;
    char const * const interface_name =
        tb[NETWORK_INTERFACE_EVENT_INTERFACE] != NULL
        ? blobmsg_get_string(tb[NETWORK_INTERFACE_EVENT_INTERFACE])
        : NULL;

    if (action == NULL || interface_name == NULL)
    {
        goto done;
    }

    /*
     * Only interested in "ifdown" actions from netifd
     * Other netifd states include "ifup", but these are sent before the
     * routing table rules have been set up, so the interface is considered 'up'
     * once and interface.state event (sent by a hotplug script) has been
     * received.
     */
    if (strcmp(action, "ifdown") != 0)
    {
        goto done;
    }

    interface_st * const iface = interface_tester_lookup_by_name(ctx, interface_name);

    if (iface == NULL)
    {
        goto done;
    }

    interface_connection_disconnected(&iface->connection);

done:
    return;
}

typedef enum interface_state_event_policy_t
{
    INTERFACE_STATE_EVENT_STATE,
    INTERFACE_STATE_EVENT_INTERFACE,
    INTERFACE_STATE_EVENT_COUNT__,
} interface_state_event_policy_t;

static const struct blobmsg_policy
    interface_state_event_policy[INTERFACE_STATE_EVENT_COUNT__] =
{
    [INTERFACE_STATE_EVENT_STATE] = { .name = "state", .type = BLOBMSG_TYPE_STRING },
    [INTERFACE_STATE_EVENT_INTERFACE] = { .name = "interface", .type = BLOBMSG_TYPE_STRING },
};

static void
interface_state_event_handler_cb(
    struct ubus_context * const ubus,
    struct ubus_event_handler * const ev,
    const char * const type,
    struct blob_attr * const msg)
{
    UNUSED(ev);
    UNUSED(type);
    interface_tester_shared_st * const ctx =
        container_of(ubus, interface_tester_shared_st, ubus_conn.ctx);
    struct blob_attr * tb[INTERFACE_STATE_EVENT_COUNT__];

    blobmsg_parse(interface_state_event_policy, INTERFACE_STATE_EVENT_COUNT__, tb,
                  blobmsg_data(msg), blobmsg_data_len(msg));

    char const * const state =
        tb[INTERFACE_STATE_EVENT_STATE] != NULL
        ? blobmsg_get_string(tb[INTERFACE_STATE_EVENT_STATE])
        : NULL;
    char const * const interface_name =
        tb[INTERFACE_STATE_EVENT_INTERFACE] != NULL
        ? blobmsg_get_string(tb[INTERFACE_STATE_EVENT_INTERFACE])
        : NULL;

    if (state == NULL || interface_name == NULL)
    {
        goto done;
    }

    /*
     * Only interested in "ifup" states from the hotplug script.
     */
    if (strcmp(state, "ifup") != 0)
    {
        goto done;
    }

    interface_st * const iface = interface_tester_lookup_by_name(ctx, interface_name);

    if (iface == NULL)
    {
        goto done;
    }

    interface_connection_connected(&iface->connection);

done:
    return;
}

void
ubus_subscribe_to_interface_events(
    struct ubus_context * const ubus,
    struct ubus_event_handler * const interface_events_ctx)
{
    interface_events_ctx->cb = network_interface_event_handler_cb;
    ubus_register_event_handler(ubus, interface_events_ctx, interface_monitor_event_id);
}

void
ubus_subscribe_to_interface_state_events(
    struct ubus_context * const ubus,
    struct ubus_event_handler * const interface_events_ctx)
{
    interface_events_ctx->cb = interface_state_event_handler_cb;
    ubus_register_event_handler(ubus, interface_events_ctx, interface_state_event_id);
}

static int
iface_handle_state(
    struct ubus_context * const ubus, struct ubus_object * const obj,
    struct ubus_request_data * const req, const char * const method,
    struct blob_attr * const msg)
{
    UNUSED(ubus);
    UNUSED(req);
    UNUSED(method);
    UNUSED(msg);
    interface_st * const iface = container_of(obj, interface_st, ubus_object);
    struct blob_buf b = { 0 };

    blob_buf_init(&b, 0);

    interface_state_dump(iface, &b);
    ubus_send_reply(ubus, req, b.head);

    blob_buf_free(&b);

    return UBUS_STATUS_OK;
}

static int
iface_handle_all_states(
    struct ubus_context * const ubus, struct ubus_object * const obj,
    struct ubus_request_data * const req, const char * const method,
    struct blob_attr * const msg)
{
    UNUSED(obj);
    UNUSED(req);
    UNUSED(method);
    UNUSED(msg);
    interface_tester_shared_st * const ctx =
        container_of(ubus, interface_tester_shared_st, ubus_conn.ctx);
    struct blob_buf b = { 0 };

    blob_buf_init(&b, 0);

    interface_states_dump(ctx, &b);

    ubus_send_reply(ubus, req, b.head);

    blob_buf_free(&b);

    return UBUS_STATUS_OK;
}

static int
iface_handle_config_reload(
    struct ubus_context * const ubus, struct ubus_object * const obj,
    struct ubus_request_data * const req, const char * const method,
    struct blob_attr * const msg)
{
    UNUSED(obj);
    UNUSED(req);
    UNUSED(method);
    UNUSED(msg);
    interface_tester_shared_st * const ctx =
        container_of(ubus, interface_tester_shared_st, ubus_conn.ctx);

    config_load_from_file_check(ctx);

    return UBUS_STATUS_OK;
}

enum
{
    STATE_GET_UP,
    STATE_GET_COUNT,
};

static const struct blobmsg_policy state_get_policy[STATE_GET_COUNT] =
{
    [STATE_GET_UP] = { .name = "up", .type = BLOBMSG_TYPE_BOOL },
};

static void
get_interface_state_cb(
    struct ubus_request * const req,
    int const type,
    struct blob_attr * const msg)
{
    UNUSED(type);
    struct blob_attr * tb[STATE_GET_COUNT];
    bool * const state = (bool *)req->priv;

    blobmsg_parse(state_get_policy, STATE_GET_COUNT, tb, blob_data(msg), blob_len(msg));

    *state = tb[STATE_GET_UP] != NULL && blobmsg_get_bool(tb[STATE_GET_UP]);
}

bool
interface_get_current_state(
    struct ubus_context * const ubus, char const * const interface_name)
{
    uint32_t id;
    bool state = false;
    char * path = NULL;

    if (asprintf(&path, "network.interface.%s", interface_name) == -1)
    {
        goto done;
    }

    if (ubus_lookup_id(ubus, path, &id) != UBUS_STATUS_OK)
    {
        goto done;
    }

    int const ret = ubus_invoke(
        ubus, id, "status", NULL, get_interface_state_cb, &state, UBUS_TIMEOUT_MS);

    if (ret != UBUS_STATUS_OK)
    {
        goto done;
    }
    /* Else the state has been set by the callback. */

done:
    free(path);

    return state;
}

#if WITH_METRICS_ADJUSTMENT
bool
ubus_send_metrics_adjust_request(
    struct ubus_context * const ubus,
    char const * const interface_name,
    uint32_t const amount)
{
    uint32_t id;
    bool success = false;
    char * path = NULL;

    if (asprintf(&path, "network.interface.%s", interface_name) == -1)
    {
        goto done;
    }

    if (ubus_lookup_id(ubus, path, &id) != UBUS_STATUS_OK)
    {
        goto done;
    }
    struct blob_buf b = { 0 };

    blob_buf_init(&b, 0);

    blobmsg_add_u32(&b, "adjustment", amount);
    blobmsg_add_u8(&b, "persist", true);

    int const ret = ubus_invoke(
        ubus, id, "adjust_metrics", b.head, NULL, NULL, UBUS_TIMEOUT_MS);

    blob_buf_free(&b);

    if (ret != UBUS_STATUS_OK)
    {
        DPRINTF("%s: failed to set metrics adjustment to %"PRIu32"\n",
                interface_name, amount);
        goto done;
    }
    /* Else the state has been set by the callback. */
    success = true;

done:
    free(path);

    return success;
}
#endif

/* The methods supported by each interface tester. */
static struct
    ubus_method iface_object_methods[] =
{
    { .name = "state", .handler = iface_handle_state },
};

static struct
    ubus_object_type iface_object_type =
    UBUS_OBJECT_TYPE("tester_interface", iface_object_methods);

static struct ubus_object
    interface_object = {
    .name = "interface", /* Gets overwritten with an interface-specific name. */
    .type = &iface_object_type,
    .methods = iface_object_methods,
    .n_methods = ARRAY_SIZE(iface_object_methods),
};

static int
interface_tester_handle_config(
    struct ubus_context * const ubus, struct ubus_object * const obj,
    struct ubus_request_data * const req, const char * const method,
    struct blob_attr * const msg)
{
    UNUSED(obj);
    UNUSED(req);
    UNUSED(method);
    int res;
    interface_tester_shared_st * const ctx =
        container_of(ubus, interface_tester_shared_st, ubus_conn.ctx);

    if (!config_load_config(ctx, msg))
    {
        res = UBUS_STATUS_INVALID_ARGUMENT;
        goto done;
    }

    res = UBUS_STATUS_OK;

done:
    return res;
}

typedef enum interface_tester_config_policy_t
{
    INTERFACE_TESTER_CONFIG,
    INTERFACE_TESTER_COUNT,
} interface_tester_config_policy_t;

static const struct blobmsg_policy
    interface_tester_config_policy[INTERFACE_TESTER_COUNT] =
{
    [INTERFACE_TESTER_CONFIG] = { .name = "interfaces", .type = BLOBMSG_TYPE_TABLE },
};

/* The methods supported by the main process. */
static struct ubus_method
    iface_tester_object_methods[] =
{
    UBUS_METHOD("config", interface_tester_handle_config, interface_tester_config_policy),
    UBUS_METHOD_NOARG("state", iface_handle_all_states),
    UBUS_METHOD_NOARG("config_reload", iface_handle_config_reload),
};

static struct
    ubus_object_type iface_tester_object_type =
    UBUS_OBJECT_TYPE("interface_tester", iface_tester_object_methods);


static struct
    ubus_object interface_tester_object = {
    .name = Sinterface_tester,
    .type = &iface_tester_object_type,
    .methods = iface_tester_object_methods,
    .n_methods = ARRAY_SIZE(iface_tester_object_methods),
};

bool
ubus_add_interface_object(interface_st * const iface)
{
    int ret;
    struct ubus_context * const ubus = &iface->ctx->ubus_conn.ctx;
    struct ubus_object * const obj = &iface->ubus_object;

    *obj = interface_object;

    char * name = NULL;

    if (asprintf(&name, "%s.interface.%s", interface_tester_object.name, iface->name) == -1)
    {
        ret = false;
        goto done;
    }

    obj->name = name;
    if (ubus_add_object(ubus, obj) != UBUS_STATUS_OK)
    {
        DPRINTF("%s: failed to publish ubus object for interface\n",
                iface->name);
        free(name);
        obj->name = NULL;
        ret = false;
        goto done;
    }

    ret = true;

done:
    return ret;
}

void
ubus_remove_interface_object(interface_st * const iface)
{
    struct ubus_context * const ubus = &iface->ctx->ubus_conn.ctx;
    struct ubus_object * const obj = &iface->ubus_object;

    if (obj->name == NULL)
    {
        goto done;
    }

    DPRINTF("remove objects for interface: %s\n", iface->name);

    ubus_remove_object(ubus, obj);
    free_const(obj->name);
    obj->name = NULL;

done:
    return;
}

bool
ubus_add_main_object(struct ubus_context * const ubus)
{
    int const ret = ubus_add_object(ubus, &interface_tester_object);
    bool const success = ret == UBUS_STATUS_OK;

    if (!success)
    {
        DPRINTF("Failed to publish object '%s': %s\n",
                interface_tester_object.name, ubus_strerror(ret));
    }

    return success;
}

void
ubus_init(
    struct ubus_auto_conn * const ubus_auto,
    char const * const path,
    ubus_connect_cb const handler)
{
    ubus_auto->path = path;
    ubus_auto->cb = handler;
    ubus_auto_connect(ubus_auto);
}

