#include "config.h"
#include "debug.h"
#include "interface_connection.h"
#include "interface_tester.h"
#include "shared.h"
#include "strings.h"
#include "tester_common.h"
#include "ubus.h"
#include "utils.h"

#include <libubox/avl-cmp.h>
#include <libubox/blobmsg_json.h>

#ifdef DEBUG
#include <assert.h>
#endif

static bool
ubus_publish_interface_object(interface_st * const iface)
{
    DPRINTF(": %s\n", iface->name);

    return ubus_add_interface_object(iface);
}

void
interface_update(interface_tester_shared_st * const ctx)
{
    vlist_update(&ctx->interfaces);
}

void
interface_flush_old(interface_tester_shared_st * const ctx)
{
    vlist_flush(&ctx->interfaces);
}

static bool interface_tester_config_tests_changed(
    interface_config_st const * const existing_config,
    interface_config_st const * const new_config)
{
    if (existing_config->num_tests != new_config->num_tests)
    {
        return true;
    }
    for (size_t i = 0; i < existing_config->num_tests; i++)
    {
        test_config_st const * const existing_test = &existing_config->tests[i];
        test_config_st const * const new_test = &new_config->tests[i];
        bool const changed =
            strcmp(existing_test->executable_name, new_test->executable_name) != 0
            || strcmp(existing_test->label, new_test->label) != 0
            || existing_test->response_timeout_secs != new_test->response_timeout_secs
            || !blob_attr_equal(existing_test->params, new_test->params);

        if (changed)
        {
            return true;
        }
    }

    return false;
}

static bool interface_tester_config_recoverys_changed(
    interface_config_st const * const existing_config,
    interface_config_st const * const new_config)
{
    if (existing_config->num_recoverys != new_config->num_recoverys)
    {
        return true;
    }
    for (size_t i = 0; i < existing_config->num_recoverys; i++)
    {
        recovery_config_st const * const existing_recovery = &existing_config->recoverys[i];
        recovery_config_st const * const new_recovery = &new_config->recoverys[i];
        bool const changed =
            strcmp(existing_recovery->executable_name, new_recovery->executable_name) != 0
            || strcmp(existing_recovery->label, new_recovery->label) != 0
            || existing_recovery->response_timeout_secs != new_recovery->response_timeout_secs
            || !blob_attr_equal(existing_recovery->params, new_recovery->params);

        if (changed)
        {
            return true;
        }
    }

    return false;
}

static void
config_update(interface_st * const existing_iface, interface_st * const new_iface)
{
    /*
     * Update any of the configuration that has changed.
     * If anything has changed, restart the tester.
     */
    interface_config_st * const existing_config = &existing_iface->config;
    interface_config_st * const new_config = &new_iface->config;

    DPRINTF("%s\n", existing_iface->name);

    bool changed =
        existing_config->success_condition != new_config->success_condition
        || existing_config->settling_delay_secs != new_config->settling_delay_secs
        || existing_config->test_passing_interval_secs
        != new_config->test_passing_interval_secs
        || existing_config->test_failing_interval_secs
        != new_config->test_failing_interval_secs
        || existing_config->pass_threshold != new_config->pass_threshold
        || existing_config->fail_threshold != new_config->fail_threshold
        || existing_config->response_timeout_secs != new_config->response_timeout_secs
#if WITH_METRICS_ADJUSTMENT
        || existing_config->failing_tests_metrics_increase != new_config->failing_tests_metrics_increase
#endif
        || existing_config->num_tests != new_config->num_tests
        || interface_tester_config_tests_changed(existing_config, new_config)
        || interface_tester_config_recoverys_changed(existing_config, new_config);

    if (changed)
    {
        /*
         * Replace the old config with the new_config and ensure the new config
         * is cleared from the new interface to ensure that any memory allocated
         * by it isn't freed when the new instance is freed.
         */
        interface_tester_stop(existing_iface);
        interface_tester_config_free(existing_config);
        *existing_config = *new_config;
        memset(new_config, 0, sizeof(*new_config));
        interface_tester_start(existing_iface);
    }
}

static void
config_add(interface_st * const iface)
{
    DPRINTF("%s\n", iface->name);

    ubus_publish_interface_object(iface);
    interface_tester_begin(iface);
}

static void
config_remove(interface_st * const iface)
{
    DPRINTF("%s\n", iface->name);

    interface_tester_free(iface);
}

static void
interface_update_cb(
    struct vlist_tree * const tree,
    struct vlist_node * const node_new,
    struct vlist_node * const node_old)
{
    UNUSED(tree);

    if (node_new != NULL && node_old != NULL)
    {
        /* Updating an existing interface. */
        interface_st * const new_iface = container_of(node_new, interface_st, node);
        interface_st * const old_iface = container_of(node_old, interface_st, node);

        config_update(old_iface, new_iface);

        /*
         * The new interface will not be added to the vlist,
         * so it must now be freed.
         */
        interface_tester_free(new_iface);
    }
    else if (node_new != NULL)
    {
        /* Adding an interface. */
        interface_st * const new_iface = container_of(node_new, interface_st, node);

        config_add(new_iface);
    }
    else if (node_old != NULL)
    {
        /* Removing an interface. */
        interface_st * const old_iface = container_of(node_old, interface_st, node);

        config_remove(old_iface);
    }
    else
    {
        /* Shouldn't happen. */
#ifdef DEBUG
        assert(false);
#endif
    }
}

typedef enum interface_test_config_policy_t
{
    INTERFACE_TEST_CONFIG_EXECUTABLE,
    INTERFACE_TEST_CONFIG_LABEL,
    INTERFACE_TEST_CONFIG_RESPONSE_TIMEOUT,
    INTERFACE_TEST_CONFIG_PARAMS,
    INTERFACE_TEST_CONFIG_COUNT,
} interface_test_config_policy_t;

static const struct blobmsg_policy interface_test_config_policy[INTERFACE_TEST_CONFIG_COUNT] =
{
    [INTERFACE_TEST_CONFIG_EXECUTABLE] = {.name = Sexecutable, .type = BLOBMSG_TYPE_STRING },
    [INTERFACE_TEST_CONFIG_LABEL] = {.name = Slabel, .type = BLOBMSG_TYPE_STRING },
    [INTERFACE_TEST_CONFIG_RESPONSE_TIMEOUT] = {.name = Sresponse_timeout_secs, .type = BLOBMSG_TYPE_INT32 },
    [INTERFACE_TEST_CONFIG_PARAMS] = {.name = Sparams, .type = BLOBMSG_TYPE_TABLE },
};

static bool add_test_configuration(
    test_config_st * const config, struct blob_attr * const test, size_t const index)
{
    bool success;
    struct blob_attr * tb[INTERFACE_TEST_CONFIG_COUNT];

    blobmsg_parse(interface_test_config_policy, ARRAY_SIZE(tb), tb,
                  blobmsg_data(test), blobmsg_data_len(test));

    if (tb[INTERFACE_TEST_CONFIG_EXECUTABLE] == NULL
        || tb[INTERFACE_TEST_CONFIG_LABEL] == NULL
        || tb[INTERFACE_TEST_CONFIG_PARAMS] == NULL)
    {
        success = false;
        goto done;
    }

    config->index = index;
    config->executable_name = strdup(blobmsg_get_string(tb[INTERFACE_TEST_CONFIG_EXECUTABLE]));
    config->label = strdup(blobmsg_get_string(tb[INTERFACE_TEST_CONFIG_LABEL]));
    if (tb[INTERFACE_TEST_CONFIG_RESPONSE_TIMEOUT] != NULL)
    {
        config->response_timeout_secs = blobmsg_get_u32(tb[INTERFACE_TEST_CONFIG_RESPONSE_TIMEOUT]);
    }
    else
    {
        config->response_timeout_secs = 0;
    }
    config->params = blob_memdup(tb[INTERFACE_TEST_CONFIG_PARAMS]);

    success = true;

done:
    return success;
}

static bool
add_test_configurations(
    interface_config_st * const config, struct blob_attr * const tests)
{
    bool success;

    if (blobmsg_type(tests) != BLOBMSG_TYPE_ARRAY)
    {
        DPRINTF("tests not an array\n");

        success = false;
        goto done;
    }

    size_t rem;
    struct blob_attr * cur;
    int const num_tests = blobmsg_check_array(tests, BLOBMSG_TYPE_TABLE);

    if (num_tests <= 0)
    {
        /* An interface tester needs at least one test to perform. */
        success = false;
        goto done;
    }

    config->num_tests = 0;
    config->tests = calloc(num_tests, sizeof(*config->tests));

    blobmsg_for_each_attr(cur, tests, rem)
    {
        if (!add_test_configuration(&config->tests[config->num_tests], cur, config->num_tests))
        {
            DPRINTF("failed to add test %zu\n", config->num_tests);

            success = false;
            goto done;
        }
        config->num_tests++;
    }

    success = true;

done:
    return success;
}

typedef enum interface_recovery_config_policy_t
{
    INTERFACE_RECOVERY_CONFIG_EXECUTABLE,
    INTERFACE_RECOVERY_CONFIG_LABEL,
    INTERFACE_RECOVERY_CONFIG_RESPONSE_TIMEOUT,
    INTERFACE_RECOVERY_CONFIG_PARAMS,
    INTERFACE_RECOVERY_CONFIG_COUNT,
} interface_recovery_config_policy_t;

static const struct blobmsg_policy interface_recovery_config_policy[INTERFACE_RECOVERY_CONFIG_COUNT] =
{
    [INTERFACE_RECOVERY_CONFIG_EXECUTABLE] = {.name = Sexecutable, .type = BLOBMSG_TYPE_STRING },
    [INTERFACE_RECOVERY_CONFIG_LABEL] = {.name = Slabel, .type = BLOBMSG_TYPE_STRING },
    [INTERFACE_RECOVERY_CONFIG_RESPONSE_TIMEOUT] = {.name = Sresponse_timeout_secs, .type = BLOBMSG_TYPE_INT32 },
    [INTERFACE_RECOVERY_CONFIG_PARAMS] = { .name = Sparams, .type = BLOBMSG_TYPE_TABLE },
};

static bool add_recovery_configuration(
    recovery_config_st * const config, struct blob_attr * const recovery, size_t const index)
{
    bool success;
    struct blob_attr * tb[INTERFACE_RECOVERY_CONFIG_COUNT];

    blobmsg_parse(interface_recovery_config_policy, ARRAY_SIZE(tb), tb,
                  blobmsg_data(recovery), blobmsg_data_len(recovery));

    if (tb[INTERFACE_RECOVERY_CONFIG_EXECUTABLE] == NULL
        || tb[INTERFACE_RECOVERY_CONFIG_LABEL] == NULL
        || tb[INTERFACE_RECOVERY_CONFIG_PARAMS] == NULL)
    {
        success = false;
        goto done;
    }

    config->index = index;
    config->executable_name = strdup(blobmsg_get_string(tb[INTERFACE_RECOVERY_CONFIG_EXECUTABLE]));
    config->label = strdup(blobmsg_get_string(tb[INTERFACE_RECOVERY_CONFIG_LABEL]));
    if (tb[INTERFACE_RECOVERY_CONFIG_RESPONSE_TIMEOUT] != NULL)
    {
        config->response_timeout_secs = blobmsg_get_u32(tb[INTERFACE_RECOVERY_CONFIG_RESPONSE_TIMEOUT]);
    }
    else
    {
        config->response_timeout_secs = 0;
    }
    config->params = blob_memdup(tb[INTERFACE_RECOVERY_CONFIG_PARAMS]);

    success = true;

done:
    return success;
}

static bool
add_recovery_configurations(
    interface_config_st * const config, struct blob_attr * const recoverys)
{
    bool success;

    if (blobmsg_type(recoverys) != BLOBMSG_TYPE_ARRAY)
    {
        DPRINTF("recoverys not an array\n");

        success = false;
        goto done;
    }

    size_t rem;
    struct blob_attr * cur;
    int const num_recoverys = blobmsg_check_array(recoverys, BLOBMSG_TYPE_TABLE);

    if (num_recoverys < 0)
    {
        success = false;
        goto done;
    }

    config->num_recoverys = 0;
    config->recoverys = calloc(num_recoverys, sizeof(*config->recoverys));

    blobmsg_for_each_attr(cur, recoverys, rem)
    {
        if (!add_recovery_configuration(&config->recoverys[config->num_recoverys], cur, config->num_recoverys))
        {
            DPRINTF("failed to add recovery %zu\n", config->num_recoverys);

            success = false;
            goto done;
        }
        config->num_recoverys++;
    }

    success = true;

done:
    return success;
}

static success_condition_st const *
success_condition_from_name(char const * const name)
{
    static const success_condition_st
        condition_definitions[test_run_success_condition_COUNT] =
    {
        [test_run_success_condition_one] =
        {
            .name = "one_test_must_pass",
            .condition = test_run_success_condition_one,
        },
        [test_run_success_condition_all] =
        {
            .name = "all_tests_must_pass",
            .condition = test_run_success_condition_all,
        },
    };

    for (size_t i = 0; i < ARRAY_SIZE(condition_definitions); i++)
    {
        if (strcmp(name, condition_definitions[i].name) == 0)
        {
            return &condition_definitions[i];
        }
    }

    return NULL;
}

static bool
interface_config_validate(interface_config_st const * const config)
{
    /* TODO: Do all the validation at one place, and at the end. */
    bool const is_valid =
        config->success_condition != NULL
        && config->response_timeout_secs > 0
        && config->test_passing_interval_secs > 0
        && config->test_failing_interval_secs > 0
        && config->pass_threshold > 0
        && config->fail_threshold > 0;

    return is_valid;
}

typedef enum interface_config_policy_t
{
    INTERFACE_CONFIG_SUCCESS_CONDITION,
    INTERFACE_CONFIG_SETTLING_DELAY,
    INTERFACE_CONFIG_PASSING_INTERVAL,
    INTERFACE_CONFIG_FAILING_INTERVAL,
    INTERFACE_CONFIG_PASS_THRESHOLD,
    INTERFACE_CONFIG_FAIL_THRESHOLD,
    INTERFACE_CONFIG_RESPONSE_TIMEOUT,
    INTERFACE_CONFIG_TESTS,
    INTERFACE_CONFIG_RECOVERY,
#if WITH_METRICS_ADJUSTMENT
    INTERFACE_CONFIG_FAILING_TESTS_METRICS_INCREASE,
#endif
    INTERFACE_CONFIG_COUNT,
} interface_config_policy_t;

static const struct blobmsg_policy interface_config_policy[INTERFACE_CONFIG_COUNT] =
{
    [INTERFACE_CONFIG_SUCCESS_CONDITION] =
        {.name = Ssuccess_condition, .type = BLOBMSG_TYPE_STRING },
    [INTERFACE_CONFIG_SETTLING_DELAY] =
        {.name = Ssettling_delay_secs, .type = BLOBMSG_TYPE_INT32 },
    [INTERFACE_CONFIG_PASSING_INTERVAL] =
        {.name = Spassing_interval_secs, .type = BLOBMSG_TYPE_INT32 },
    [INTERFACE_CONFIG_FAILING_INTERVAL] =
        {.name = Sfailing_interval_secs, .type = BLOBMSG_TYPE_INT32 },
    [INTERFACE_CONFIG_PASS_THRESHOLD] =
        { .name = Spass_threshold, .type = BLOBMSG_TYPE_INT32 },
    [INTERFACE_CONFIG_FAIL_THRESHOLD] =
        {.name = Sfail_threshold, .type = BLOBMSG_TYPE_INT32 },
    [INTERFACE_CONFIG_RESPONSE_TIMEOUT] =
        {.name = Sresponse_timeout_secs, .type = BLOBMSG_TYPE_INT32 },
    [INTERFACE_CONFIG_TESTS] =
        { .name = Stests, .type = BLOBMSG_TYPE_ARRAY },
    [INTERFACE_CONFIG_RECOVERY] =
        {.name = Srecovery_tasks, .type = BLOBMSG_TYPE_ARRAY },
#if WITH_METRICS_ADJUSTMENT
    [INTERFACE_CONFIG_FAILING_TESTS_METRICS_INCREASE] =
        {.name = Sfailing_tests_metrics_increase, .type = BLOBMSG_TYPE_INT32 },
#endif
};

static int
interface_handle_config(
    interface_tester_shared_st * const ctx, struct blob_attr * const msg)
{
    int res = UBUS_STATUS_INVALID_ARGUMENT;
    interface_st * iface = NULL;
    char const * const iface_name = blobmsg_name(msg);

    if (iface_name == NULL)
    {
        DPRINTF("failed to get interface name\n");

        goto done;
    }

    struct blob_attr * tb[INTERFACE_CONFIG_COUNT];
    res = blobmsg_parse(interface_config_policy, INTERFACE_CONFIG_COUNT,
                        tb, blobmsg_data(msg), blobmsg_data_len(msg));
    if (res != UBUS_STATUS_OK)
    {
        DPRINTF("failed to parse config: %s\n", ubus_strerror(res));

        goto done;
    }

    for (size_t i = 0; i < ARRAY_SIZE(tb); i++)
    {
        if (tb[i] == NULL)
        {
            DPRINTF("missing required data: %s\n", interface_config_policy[i].name);

            goto done;
        }
    }

    iface = interface_tester_alloc(ctx, iface_name);
    interface_config_st * const config = &iface->config;

    config->success_condition =
        success_condition_from_name(blobmsg_get_string(tb[INTERFACE_CONFIG_SUCCESS_CONDITION]));

    config->settling_delay_secs = blobmsg_get_u32(tb[INTERFACE_CONFIG_SETTLING_DELAY]);
    config->test_passing_interval_secs = blobmsg_get_u32(tb[INTERFACE_CONFIG_PASSING_INTERVAL]);
    config->test_failing_interval_secs = blobmsg_get_u32(tb[INTERFACE_CONFIG_FAILING_INTERVAL]);
    config->pass_threshold = blobmsg_get_u32(tb[INTERFACE_CONFIG_PASS_THRESHOLD]);
    config->fail_threshold = blobmsg_get_u32(tb[INTERFACE_CONFIG_FAIL_THRESHOLD]);
    config->response_timeout_secs = blobmsg_get_u32(tb[INTERFACE_CONFIG_RESPONSE_TIMEOUT]);
#if WITH_METRICS_ADJUSTMENT
    config->failing_tests_metrics_increase = blobmsg_get_u32(tb[INTERFACE_CONFIG_FAILING_TESTS_METRICS_INCREASE]);
#endif

    if (!add_test_configurations(config, tb[INTERFACE_CONFIG_TESTS]))
    {
        DPRINTF("failed to add test configuration\n");

        goto done;
    }

    if (!add_recovery_configurations(config, tb[INTERFACE_CONFIG_RECOVERY]))
    {
        DPRINTF("failed to add recovery configuration\n");

        goto done;
    }

    if (!interface_config_validate(config))
    {
        DPRINTF("config failed validation\n");

        goto done;
    }

    vlist_add(&ctx->interfaces, &iface->node, iface->name);

    iface = NULL;
    res = UBUS_STATUS_OK;

done:
    interface_tester_free(iface);
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

bool
config_load_config(
    interface_tester_shared_st * const ctx, struct blob_attr * const config)
{
    bool success;
    struct blob_attr * cur;
    size_t rem;

    DPRINTF("\n");

    struct blob_attr * tb[INTERFACE_TESTER_COUNT];

    blobmsg_parse(interface_tester_config_policy, INTERFACE_TESTER_COUNT,
                  tb, blob_data(config), blob_len(config));

    if (tb[INTERFACE_TESTER_CONFIG] == NULL
        || blobmsg_type(tb[INTERFACE_TESTER_CONFIG]) != BLOBMSG_TYPE_TABLE)
    {
        success = false;
        goto done;
    }

    interface_update(ctx);

    blobmsg_for_each_attr(cur, tb[INTERFACE_TESTER_CONFIG], rem)
    {
        if (blobmsg_type(cur) != BLOBMSG_TYPE_TABLE
            || !blobmsg_check_attr(cur, true))
        {
            success = false;
            goto done;
        }
        /*
         * Individual interfaces are not added if their configuration is
         * invalid.
         * This is not enough to prevent other interfaces from being added
         * though.
         */
        interface_handle_config(ctx, cur);
    }

    success = true;

done:
    interface_flush_old(ctx);

    return success;
}

void
config_load_from_file_check(interface_tester_shared_st * const ctx)
{
    char const * const config_file = ctx->config_file;

    DPRINTF("Config file: %s\n", config_file);

    if (config_file == NULL)
    {
        goto done;
    }

    struct blob_buf b = { 0 };

    blob_buf_init(&b, 0);

    if (!blobmsg_add_json_from_file(&b, config_file))
    {
        DPRINTF("Failed to load config from: %s\n", config_file);
    }
    else if (!config_load_config(ctx, b.head))
    {
        DPRINTF("Failed to load config blob\n");
    }

    blob_buf_free(&b);

done:
    return;
}

void
config_init(interface_tester_shared_st * const ctx)
{
    struct vlist_tree * const interfaces = &ctx->interfaces;

    vlist_init(interfaces, avl_strcmp, interface_update_cb);
    interfaces->keep_old = true;
}

