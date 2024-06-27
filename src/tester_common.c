#include "tester_common.h"
#include "interface_tester.h"
#include "debug.h"
#include "ubus.h"
#include "utils.h"

#ifdef DEBUG
#include <assert.h>
#endif

const unsigned int msecs_per_sec = 1000;

char const *
tester_event_to_str(tester_event_t const event)
{
    static char const * events[TESTER_EVENT_COUNT__] =
    {
    [TESTER_EVENT_INTERFACE_DISCONNECTED] = "connection disconnected",
    [TESTER_EVENT_INTERFACE_SETTLED] = "connection settled",
    [TESTER_EVENT_INTERVAL_TIMER_ELAPSED] = "interval timer elapsed",
    [TESTER_EVENT_TEST_RUN_REQUESTED] = "start_test_run",
    [TESTER_EVENT_TEST_PASSED] = "test passed",
    [TESTER_EVENT_TEST_FAILED] = "test failed",
    [TESTER_EVENT_TEST_TIMED_OUT] = "test timed out",
    [TESTER_EVENT_RECOVERY_TASK_ENDED] = "recovery task ended",
    [TESTER_EVENT_RECOVERY_TASK_TIMED_OUT] = "recovery task timed out",
    };

#ifdef DEBUG
    assert(event < ARRAY_SIZE(events));
    assert(events[event] != NULL);
#endif

    return events[event];
}

static void
interface_tester_test_config_free(test_config_st * const test)
{
    free_const(test->executable_name);
    free_const(test->label);
    free(test->params);
}

static void
interface_tester_test_configs_free(interface_config_st * const config)
{
    for (size_t i = 0; i < config->num_tests; i++)
    {
        interface_tester_test_config_free(&config->tests[i]);
    }
    config->num_tests = 0;
    free(config->tests);
    config->tests = NULL;
}

static void
interface_tester_recovery_config_free(recovery_config_st * const recovery)
{
    free_const(recovery->executable_name);
    free_const(recovery->label);
    free(recovery->params);
}

static void
interface_tester_recovery_configs_free(interface_config_st * const config)
{
    for (size_t i = 0; i < config->num_recoverys; i++)
    {
        interface_tester_recovery_config_free(&config->recoverys[i]);
    }
    config->num_recoverys = 0;
    free(config->recoverys);
    config->recoverys = NULL;
}

void
interface_tester_config_free(interface_config_st * const config)
{
    interface_tester_test_configs_free(config);
    interface_tester_recovery_configs_free(config);
}

void
interface_tester_free(interface_st * const iface)
{
    if (iface == NULL)
    {
        goto done;
    }

    ubus_remove_interface_object(iface);
    interface_tester_cleanup(iface);
    interface_tester_config_free(&iface->config);

    free(iface);

done:
    return;
}

struct interface_st *
interface_tester_alloc(
    interface_tester_shared_st * const ctx, const char * const name)
{
    struct interface_st *iface;
    char * iface_name;

    iface = calloc_a(sizeof(*iface), &iface_name, strlen(name) + 1);
    iface->ctx = ctx;
    iface->name = strcpy(iface_name, name);

    interface_tester_initialise(iface);

    return iface;
}

interface_st *
interface_tester_lookup_by_name(
    interface_tester_shared_st * const ctx, char const * const interface_name)
{
    interface_st * iface;

    iface = vlist_find(&ctx->interfaces, interface_name, iface, node);

    return iface;
}

void
interface_testers_free(struct vlist_tree * const interfaces)
{
    vlist_update(interfaces);
    vlist_flush(interfaces);
}

