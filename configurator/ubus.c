#include "ubus.h"
#include "configurator.h"
#include "debug.h"
#include "event_processor.h"
#include "utils.h"

#include <libubox/blobmsg_json.h>

static int const UBUS_TIMEOUT_MS = 1000;
static char const interface_tester_id[] = "interface.tester";
static char const interface_tester_events[] = "interface.tester*";

static void
send_config_to_interface_tester(
    struct ubus_context * const ubus, char const * const config_path)
{
    uint32_t id;
    struct blob_buf b = { 0 };

    DPRINTF("\n");

    blob_buf_init(&b, 0);

    if (!blobmsg_add_json_from_file(&b, config_path))
    {
        DPRINTF("failed to read config json: %s\n", config_path);
        goto done;
    }

    if (ubus_lookup_id(ubus, interface_tester_id, &id) != UBUS_STATUS_OK)
    {
        goto done;
    }

    int const ret = ubus_invoke(ubus, id, "config", b.head, NULL, NULL, UBUS_TIMEOUT_MS);

    blob_buf_free(&b);

    if (ret != UBUS_STATUS_OK)
    {
        DPRINTF("failed to send config to interface tester\n");
    }

done:
    return;
}

typedef enum interface_event_policy_t
{
    INTERFACE_EVENT_STATE,
    INTERFACE_EVENT_COUNT__,
} interface_event_policy_t;

static const struct blobmsg_policy
interface_event_policy[INTERFACE_EVENT_COUNT__] =
{
    [INTERFACE_EVENT_STATE] = { .name = "state", .type = BLOBMSG_TYPE_STRING },
};

typedef enum interface_operational_event_policy_t
{
    INTERFACE_OPERATIONAL_EVENT_IS_OPERATIONAL,
    INTERFACE_OPERATIONAL_EVENT_INTERFACE,
    INTERFACE_OPERATIONAL_EVENT_COUNT__,
} interface_operational_event_policy_t;

static const struct blobmsg_policy
interface_operational_event_policy[INTERFACE_OPERATIONAL_EVENT_COUNT__] =
{
    [INTERFACE_OPERATIONAL_EVENT_IS_OPERATIONAL] = {.name = "is_operational", .type = BLOBMSG_TYPE_BOOL },
    [INTERFACE_OPERATIONAL_EVENT_INTERFACE] = { .name = "interface", .type = BLOBMSG_TYPE_STRING },
};

static void
interface_event_handler_cb(
    struct ubus_context *ubus, struct ubus_event_handler *ev,
    const char * type,
    struct blob_attr * msg)
{
    UNUSED(ev);
    configurator_st * ctx =
        container_of(ubus, configurator_st, ubus_conn.ctx);

    DPRINTF("%s\n", type);
    if (strcmp(type, interface_tester_id) == 0)
    {
        struct blob_attr * tb[INTERFACE_EVENT_COUNT__];

        blobmsg_parse(interface_event_policy, INTERFACE_EVENT_COUNT__, tb,
                      blobmsg_data(msg), blobmsg_data_len(msg));

        char const * const state =
            tb[INTERFACE_EVENT_STATE] != NULL
            ? blobmsg_get_string(tb[INTERFACE_EVENT_STATE])
            : NULL;

        if (state == NULL)
        {
            goto done;
        }

        if (strcmp(state, "up") == 0 && ctx->config_path != NULL)
        {
            send_config_to_interface_tester(ubus, ctx->config_path);
        }
    }
    else if (strcmp(type, "interface.tester.operational") == 0)
    {
        struct blob_attr * tb[INTERFACE_OPERATIONAL_EVENT_COUNT__];

        blobmsg_parse(interface_operational_event_policy,
                      INTERFACE_OPERATIONAL_EVENT_COUNT__,
                      tb,
                      blobmsg_data(msg), blobmsg_data_len(msg));

        char const * const interface_name =
            tb[INTERFACE_OPERATIONAL_EVENT_INTERFACE] != NULL
            ? blobmsg_get_string(tb[INTERFACE_OPERATIONAL_EVENT_INTERFACE])
            : NULL;
        bool const is_operational =
            tb[INTERFACE_OPERATIONAL_EVENT_IS_OPERATIONAL]
            && blobmsg_get_bool(tb[INTERFACE_OPERATIONAL_EVENT_IS_OPERATIONAL]);

        if (interface_name == NULL)
        {
            goto done;
        }
        DPRINTF("%s: is operational: %d\n", interface_name, is_operational);

        if (ctx->event_processor_path != NULL)
        {
            run_event_processor(ctx->event_processor_path, interface_name, is_operational);
        }
    }

done:
    return;
}

void
ubus_subscribe_to_tester_events(
    struct ubus_context * const ubus, struct ubus_event_handler * const interface_events_ctx)
{
    interface_events_ctx->cb = interface_event_handler_cb;
    ubus_register_event_handler(
        ubus, interface_events_ctx, interface_tester_events);
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

