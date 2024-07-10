#include "interface_tester.h"
#include "debug.h"
#include "interface_connection.h"
#include "ubus.h"
#include "utils.h"

#include <libubox/blobmsg_json.h>

#ifdef DEBUG
#include <assert.h>
#endif

static void
interface_test_passed(interface_tester_st * tester);

static void
interface_test_failed(interface_tester_st * tester);

char const *
interface_tester_state_to_str(interface_tester_state_t const state)
{
    static char const * states[TESTER_STATE_COUNT__] =
    {
    [TESTER_STATE_STOPPED] = "stopped",
    [TESTER_STATE_SLEEPING] = "sleeping",
    [TESTER_STATE_TESTING] = "testing",
    [TESTER_STATE_RECOVERING] = "recovering",
    };

#ifdef DEBUG
    assert(state < ARRAY_SIZE(states));
    assert(states[state] != NULL);
#endif

    return states[state];
}

char const *
interface_recovery_state_to_str(interface_recovery_state_t const state)
{
    static char const * states[RECOVERY_STATE_COUNT__] =
    {
    [RECOVERY_STATE_OPERATIONAL] = "operational",
    [RECOVERY_STATE_BROKEN] = "broken",
    };

#ifdef DEBUG
    assert(state < ARRAY_SIZE(states));
    assert(states[state] != NULL);
#endif

    return states[state];
}

static size_t
next_recovery_task_index(interface_recovery_st * const recovery)
{
    interface_st * const iface = container_of(recovery, interface_st, recovery);
    size_t next_task = recovery->recovery_index;

    recovery->recovery_index++;
    if (recovery->recovery_index >= iface->config.num_recoverys)
    {
        recovery->recovery_index = 0;
    }

    /* Shouldn't happen, but ensure the next_task index isn't out of bounds. */
    if (next_task >= iface->config.num_recoverys)
    {
        DLOG("%s: task index out of bounds (%zu) - max is: %zu",
             iface->name, next_task, iface->config.num_recoverys);
#ifdef DEBUG
        assert(false);
#endif
        next_task = 0;
    }

    return next_task;
}

#if WITH_METRICS_ADJUSTMENT
static void
interface_adjust_route_metrics(interface_st * const iface, uint32_t const amount)
{
    ubus_send_metrics_adjust_request(
        &iface->ctx->ubus_conn.ctx, iface->name, amount);
}
#endif

static void
recovery_state_transition(
    interface_recovery_st * const recovery, interface_recovery_state_t const new_state)
{
#ifdef DEBUG
    interface_st * const iface = container_of(recovery, interface_st, recovery);
#endif

    ILOG("%s: %s: change state from %s -> %s",
         __func__,
         iface->name,
         interface_recovery_state_to_str(recovery->state),
         interface_recovery_state_to_str(new_state));

    recovery->state = new_state;
}

static void
tester_state_transition(
    interface_tester_st * const tester, interface_tester_state_t const new_state)
{
#ifdef DEBUG
    interface_st * const iface = container_of(tester, interface_st, tester);
#endif

    ILOG("%s: %s: change state from %s -> %s",
         __func__,
         iface->name,
         interface_tester_state_to_str(tester->state),
         interface_tester_state_to_str(new_state));

    tester->state = new_state;
}

static void
test_interval_timer_expired(timer_st * const t)
{
    interface_tester_st * const tester =
        container_of(t, interface_tester_st, test_interval_timer);
    interface_st * const iface = container_of(tester, interface_st, tester);

    ILOG("%s: %s", __func__, iface->name);

    interface_tester_send_event(iface, TESTER_EVENT_INTERVAL_TIMER_ELAPSED);
}

static void
tester_interval_timer_start(
    interface_tester_st * const tester, uint32_t const timeout_msecs)
{
    timer_st * const t = &tester->test_interval_timer;
#ifdef DEBUG
    interface_st * const iface = container_of(tester, interface_st, tester);
#endif

    ILOG("%s: %s: %" PRIu32 " msecs", __func__, iface->name, timeout_msecs);

    timer_start(t, timeout_msecs);
}

static void
tester_interval_timer_stop(interface_tester_st * const tester)
{
    timer_st * const t = &tester->test_interval_timer;
#if DEBUG
    interface_st * const iface = container_of(tester, interface_st, tester);
#endif

    DLOG("%s: %s", __func__, iface->name);

    timer_stop(t);
}

static void
tester_response_timer_stop(interface_tester_st * const tester)
{
    timer_st * const t = &tester->test_response_timeout_timer;
#if DEBUG
    interface_st * const iface = container_of(tester, interface_st, tester);
#endif

    DLOG("%s: %s", __func__, iface->name);

    timer_stop(t);
}

static void
test_response_timer_expired(timer_st * const t)
{
    interface_tester_st * const tester =
        container_of(t, interface_tester_st, test_response_timeout_timer);
    interface_st * const iface = container_of(tester, interface_st, tester);

    DLOG("%s: %s", __func__, iface->name);

    interface_tester_send_event(iface, TESTER_EVENT_TEST_TIMED_OUT);
}

static void
test_response_timer_start(
    interface_tester_st * const tester, uint32_t const timeout_secs)
{
    timer_st * const tmr = &tester->test_response_timeout_timer;
#if DEBUG
    interface_st * const iface = container_of(tester, interface_st, tester);
#endif
    uint32_t const timeout_msecs = timeout_secs * msecs_per_sec;

    DLOG("%s: %s: %" PRIu32 " msecs", __func__, iface->name, timeout_msecs);

    timer_start(tmr, timeout_msecs);
}

static bool
get_test_result_from_exit_status(int const exit_status)
{
    bool const test_passed =
        WIFEXITED(exit_status) && WEXITSTATUS(exit_status) == EXIT_SUCCESS;

    return test_passed;
}

static void
test_completed(tester_process_st * const tester_proc, int const status)
{
    interface_tester_st * const tester =
        container_of(tester_proc, interface_tester_st, test_proc);
    interface_st * const iface = container_of(tester, interface_st, tester);
    bool const test_passed = get_test_result_from_exit_status(status);

    ILOG("%s: %s", __func__, iface->name);

    tester->last_test_exit_code = status;
    tester->last_test_passed = test_passed;

    tester_event_t const event =
        test_passed ? TESTER_EVENT_TEST_PASSED : TESTER_EVENT_TEST_FAILED;

    interface_tester_send_event(iface, event);
}

static bool
run_test(
    interface_tester_st * const tester,
    char const * const interface_name,
    char const * const working_dir,
    interface_config_st const * const iface_config,
    size_t const test_index)
{
    test_config_st const * const test_config = &iface_config->tests[test_index];
    bool started_test;
    int argc = 0;
    char * argv[10];
    char * const params = blobmsg_format_json(test_config->params, true);
    char * exe_name = NULL;

    if (asprintf(&exe_name, "./%s", test_config->executable_name) < 0)
    {
        started_test = false;
        goto done;
    }
    argv[argc++] = exe_name;
    argv[argc++] = (char *)interface_name;
    argv[argc++] = (char *)test_config->executable_name;
    argv[argc++] = params;
    argv[argc++] = NULL;

    DLOG("running %s: test: %s (%zu)",
         test_config->label, test_config->executable_name, test_config->index);

    tester->test_proc.cb = test_completed;
    if (!interface_tester_start_process(&tester->test_proc, argv, working_dir))
    {
        DLOG("%s: failed to run test", interface_name);

        started_test = false;
        goto done;
    }

    uint32_t const timeout_secs = test_config->response_timeout_secs > 0
        ? test_config->response_timeout_secs
        : iface_config->response_timeout_secs;

    test_response_timer_start(tester, timeout_secs);
    started_test = true;

done:
    free(exe_name);
    free(params);

    return started_test;
}

static void
tester_sleep(interface_tester_st * const tester)
{
    interface_st * const iface = container_of(tester, interface_st, tester);
    interface_config_st * const config = &iface->config;
    interface_recovery_st * const recovery = &iface->recovery;
    uint32_t const interval_secs =
        (recovery->state == RECOVERY_STATE_OPERATIONAL
         && tester->stats.test_runs.consecutive_failures == 0)
            ? config->test_passing_interval_secs
            : config->test_failing_interval_secs;
    uint32_t const timeout_msecs = interval_secs * msecs_per_sec;

    tester_state_transition(tester, TESTER_STATE_SLEEPING);
    tester_interval_timer_start(tester, timeout_msecs);
}

static void
recovery_response_timer_stop(interface_recovery_st * const recovery)
{
    timer_stop(&recovery->response_timeout_timer);
}

static void
recovery_task_timer_expired(timer_st * const t)
{
    interface_recovery_st * const recovery =
        container_of(t, interface_recovery_st, response_timeout_timer);
    interface_st * const iface = container_of(recovery, interface_st, recovery);

    ILOG("%s: %s:", __func__, iface->name);

    interface_tester_send_event(iface, TESTER_EVENT_RECOVERY_TASK_TIMED_OUT);
}

static void
recovery_response_timer_start(
    interface_recovery_st * const recovery, uint32_t const timeout_secs)
{
    timer_st * const tmr = &recovery->response_timeout_timer;
#if DEBUG
    interface_st * const iface = container_of(recovery, interface_st, recovery);
#endif
    uint32_t const timeout_msecs = timeout_secs * msecs_per_sec;

    DLOG("%s: %s: %" PRIu32 " msecs", __func__, iface->name, timeout_msecs);

    timer_start(tmr, timeout_msecs);
}

static void
recovery_task_completed(tester_process_st * const tester_proc, int const status)
{
    UNUSED(status);
    interface_recovery_st * const recovery =
        container_of(tester_proc, interface_recovery_st, proc);
    interface_st * const iface = container_of(recovery, interface_st, recovery);

    ILOG("%s: %s:", __func__, iface->name);

    interface_tester_send_event(iface, TESTER_EVENT_RECOVERY_TASK_ENDED);
}

static bool
run_recovery_task(
    interface_recovery_st * const recovery,
    char const * const interface_name,
    char const * const working_dir,
    interface_config_st const * const iface_config,
    size_t const recovery_index)
{
    recovery_config_st const * const recovery_config
        = &iface_config->recoverys[recovery_index];
    bool started_recovery;
    int argc = 0;
    char * argv[10];
    char * const params = blobmsg_format_json(recovery_config->params, true);
    char * exe_name = NULL;

    if (asprintf(&exe_name, "./%s", recovery_config->executable_name) < 0)
    {
        started_recovery = false;
        goto done;
    }
    argv[argc++] = exe_name;
    argv[argc++] = (char *)interface_name;
    argv[argc++] = (char *)recovery_config->executable_name;
    argv[argc++] = params;
    argv[argc++] = NULL;

    DLOG("running %s: task: %s (%zu)",
         recovery_config->label, recovery_config->executable_name, recovery_config->index);

    recovery->proc.cb = recovery_task_completed;
    if (!interface_tester_start_process(&recovery->proc, argv, working_dir))
    {
        started_recovery = false;
        goto done;
    }

    uint32_t const timeout_secs = recovery_config->response_timeout_secs > 0
        ? recovery_config->response_timeout_secs
        : iface_config->response_timeout_secs;

    recovery_response_timer_start(recovery, timeout_secs);
    started_recovery = true;

done:
    free(exe_name);
    free(params);

    return started_recovery;
}

static void
transition_to_operational_state(interface_st * const iface)
{
    interface_recovery_st * const recovery = &iface->recovery;

    ILOG("%s: %s", __func__, iface->name);

    recovery_state_transition(recovery, RECOVERY_STATE_OPERATIONAL);
    recovery->recovery_index = 0;

#if WITH_METRICS_ADJUSTMENT
    interface_adjust_route_metrics(iface, 0);
    recovery->metrics_are_adjusted = false;
#endif

    bool const are_operational = true;

    ubus_send_interface_operational_event(
        &iface->ctx->ubus_conn.ctx, iface->name, are_operational);
}

static void
transition_to_broken_state(interface_st * const iface)
{
    interface_recovery_st * const recovery = &iface->recovery;

    ILOG("%s: %s", __func__, iface->name);

    recovery_state_transition(recovery, RECOVERY_STATE_BROKEN);

#if WITH_METRICS_ADJUSTMENT
    {
    interface_config_st * const config = &iface->config;

    if (config->failing_tests_metrics_increase > 0)
    {
        interface_adjust_route_metrics(iface, config->failing_tests_metrics_increase);
        recovery->metrics_are_adjusted = true;
    }
    }
#endif

    bool const are_operational = false;

    ubus_send_interface_operational_event(
        &iface->ctx->ubus_conn.ctx, iface->name, are_operational);
}

static void
interface_test_run_passed(interface_recovery_st * const recovery)
{
    interface_st * const iface = container_of(recovery, interface_st, recovery);
    interface_tester_st * const tester = &iface->tester;
    interface_config_st * const config = &iface->config;
    test_run_statistics_st * const stats = &tester->stats.test_runs;

    stats->consecutive_failures = 0;
    stats->consecutive_passes++;
    stats->total_passes_this_connection++;
    stats->total_passes++;

    ILOG("%s: %s: consecutive test run passes: %"PRIu64,
         __func__, iface->name, tester->stats.test_runs.consecutive_passes);

    if (recovery->state == RECOVERY_STATE_BROKEN
        && tester->stats.test_runs.consecutive_passes == config->pass_threshold)
    {
        ILOG("%s: Pass threshold reached", iface->name);
        transition_to_operational_state(iface);
    }
}

static void
interface_test_run_failed(interface_recovery_st * const recovery)
{
    interface_st * const iface = container_of(recovery, interface_st, recovery);
    interface_tester_st * const tester = &iface->tester;
    interface_config_st * const config = &iface->config;
    test_run_statistics_st * const stats = &tester->stats.test_runs;

    stats->consecutive_passes = 0;
    stats->consecutive_failures++;
    stats->total_failures_this_connection++;
    stats->total_failures++;

    ILOG("%s: %s: consecutive test run failures: %"PRIu64,
         __func__, iface->name, tester->stats.test_runs.consecutive_failures);

    bool have_reached_failure_threshold =
        config->fail_threshold == 0
        ||(tester->stats.test_runs.consecutive_failures > 0
           && (tester->stats.test_runs.consecutive_failures % config->fail_threshold) == 0);

    if (have_reached_failure_threshold)
    {
        if (recovery->state == RECOVERY_STATE_OPERATIONAL)
        {
            ILOG("%s: Failure threshold reached", iface->name);
            transition_to_broken_state(iface);
        }

        /* Perform the next recovery action if any have been configured. */
        if (iface->config.num_recoverys > 0)
        {
            size_t const recovery_task_index = next_recovery_task_index(recovery);

            bool const have_started_recovery_task =
                run_recovery_task(
                    recovery,
                    iface->name,
                    iface->ctx->recovery_directory,
                    &iface->config,
                    recovery_task_index);

            if (have_started_recovery_task)
            {
                test_recovery_statistics_st * const recovery_stats =
                    &tester->stats.recovery;

                recovery_stats->total_this_connection++;
                recovery_stats->total++;

                tester_state_transition(tester, TESTER_STATE_RECOVERING);
            }
        }
    }
}

static void
interface_test_run_completed(interface_tester_st * const tester, bool const passed)
{
    interface_st * const iface = container_of(tester, interface_st, tester);
    interface_recovery_st * const recovery = &iface->recovery;

    ILOG("%s: %s", __func__, iface->name);

    tester->test_index = 0;
    ubus_send_interface_test_run_event(
        &iface->ctx->ubus_conn.ctx, iface->name, passed);

    if (passed)
    {
        interface_test_run_passed(recovery);
    }
    else /* Failed. */
    {
        interface_test_run_failed(recovery);
    }
    if (tester->state != TESTER_STATE_RECOVERING)
    {
        tester_sleep(tester);
    }
}

static void
interface_test_passed(interface_tester_st * const tester)
{
    interface_st * const iface = container_of(tester, interface_st, tester);

    ILOG("%s: %s", __func__, iface->name);

    test_statistics_st * const stats = &tester->stats.tests;

    stats->total_passes_this_connection++;
    stats->total_passes++;

    if (iface->config.success_condition->condition == test_run_success_condition_one)
    {
        /*
         * One test must pass.
         * The means the test run passed.
         */
        bool const test_run_passed = true;

        interface_test_run_completed(tester, test_run_passed);
    }
    else if (iface->config.success_condition->condition == test_run_success_condition_all)
    {
        tester->test_index++;

        if (tester->test_index >= iface->config.num_tests)
        {
            /*
             * All test must pass, and the last test in the list passed.
             * This means the test run passed.
             */
            bool const test_run_passed = true;

            interface_test_run_completed(tester, test_run_passed);
        }
        else
        {
            run_test(
                tester,
                iface->name,
                iface->ctx->test_directory,
                &iface->config,
                tester->test_index);
        }
    }
    else
    {
        /* Shouldn't happen. */
        DLOG("%s: unexpected test run success condition (%d)",
             iface->name, iface->config.success_condition->condition);
#if DEBUG
        assert(false);
#endif
    }
}

static void
interface_test_failed(interface_tester_st * const tester)
{
    interface_st * const iface = container_of(tester, interface_st, tester);

    ILOG("%s: %s", __func__, iface->name);

    test_statistics_st * const stats = &tester->stats.tests;

    stats->total_failures_this_connection++;
    stats->total_failures++;

    if (iface->config.success_condition->condition == test_run_success_condition_one)
    {
        tester->test_index++;

        if (tester->test_index >= iface->config.num_tests)
        {
            /*
             * One test must pass, but the last test in the list has failed.
             * This means the test run failed.
             */
            bool const test_run_passed = false;

            interface_test_run_completed(tester, test_run_passed);
        }
        else
        {
            run_test(
                tester,
                iface->name,
                iface->ctx->test_directory,
                &iface->config,
                tester->test_index);
        }
    }
    else if (iface->config.success_condition->condition == test_run_success_condition_all)
    {
        /*
         * All tests must pass, but one failed.
         * The means the test run failed.
         */
        bool const test_run_passed = false;

        interface_test_run_completed(tester, test_run_passed);
    }
    else
    {
        /* Shouldn't happen. */
        DLOG("%s: unexpected test run success condition (%d)",
             iface->name, iface->config.success_condition->condition);
#if DEBUG
        assert(false);
#endif
    }
}

static void
tester_initialise_per_connection_statistics(interface_tester_st * const tester)
{
#if DEBUG
    interface_st * const iface = container_of(tester, interface_st, tester);
#endif

    DLOG("%s: setting initial test state", iface->name);

    tester->stats.tests.total_passes_this_connection = 0;
    tester->stats.tests.total_failures_this_connection = 0;
    tester->stats.test_runs.total_passes_this_connection = 0;
    tester->stats.test_runs.total_failures_this_connection = 0;
    tester->stats.recovery.total_this_connection = 0;
}

static void
tester_stop(interface_tester_st * const tester)
{
    tester_state_transition(tester, TESTER_STATE_STOPPED);
    interface_tester_kill_process(&tester->test_proc);
    tester_response_timer_stop(tester);
    tester_interval_timer_stop(tester);
    tester->test_index = 0;
    /*
     * Note that the recovery task isn't stopped (if one was running).
     * It may be that the interface disconnects as a normal part of the recovery
     * process and that process might not have completed its tasks when this
     * occurs.
     */
}

static void tester_start_disconnected(interface_tester_st * const tester)
{
#if DEBUG
    interface_st * const iface = container_of(tester, interface_st, tester);
#else
    UNUSED(tester);
#endif

    DLOG("%s: isn't testable (connected), so won't start testing", iface->name);
}

static void tester_start_connected(interface_tester_st * const tester)
{
    interface_st * const iface = container_of(tester, interface_st, tester);

    tester_state_transition(tester, TESTER_STATE_TESTING);

    tester->test_index = 0;
    /*
     * Note that the recovery index is _not_ set back to 0 because the interface
     * may have been restarted as a result of a recovery task, and there may be
     * more tasks to perform if the interface remains broken after this
     * connection is made.
     * This allows the tester to cycle through all recovery tasks across many
     * connection instances.
     */
    run_test(
        tester, iface->name, iface->ctx->test_directory, &iface->config, tester->test_index);
}

static void
tester_start(interface_tester_st * const tester)
{
    tester->starter(tester);
}

static void
recovery_cleanup(interface_recovery_st * const recovery)
{
    interface_tester_kill_process(&recovery->proc);
    recovery_response_timer_stop(recovery);
#if WITH_METRICS_ADJUSTMENT
    {
    interface_st * const iface = container_of(recovery, interface_st, recovery);

    if (recovery->metrics_are_adjusted)
    {
        interface_adjust_route_metrics(iface, 0);
    }
    }
#endif
    recovery->recovery_index = 0;
}

static void
recovery_init(interface_recovery_st * const recovery)
{
    timer_init(
        &recovery->response_timeout_timer, "recovery_task_timer", recovery_task_timer_expired);
}

static void tester_init(interface_tester_st * const tester)
{
    tester->starter = tester_start_disconnected;
    timer_init(
        &tester->test_interval_timer, "test_interval_timer", test_interval_timer_expired);
    timer_init(
        &tester->test_response_timeout_timer, "test_response_timer", test_response_timer_expired);
    tester_state_transition(tester, TESTER_STATE_STOPPED);
}

void
interface_tester_initialise(interface_st * const iface)
{
    DLOG("%s: %s", __func__, iface->name);

    event_queue_init(&iface->event_queue);
    interface_connection_init(&iface->connection);
    tester_init(&iface->tester);
    recovery_init(&iface->recovery);
}

void
interface_tester_begin(interface_st * const iface)
{
    DLOG("%s: %s", __func__, iface->name);

    transition_to_operational_state(iface);
    interface_connection_begin(&iface->connection);
}

void
interface_tester_cleanup(interface_st * const iface)
{
    DLOG("%s: %s", __func__, iface->name);

    recovery_cleanup(&iface->recovery);
    interface_connection_cleanup(&iface->connection);
    tester_stop(&iface->tester);
}

void
interface_tester_stop(interface_st * const iface)
{
    /* Occurs with a context configuration change. */
    interface_tester_st * const tester = &iface->tester;

    ILOG("%s: %s", __func__, iface->name);

    tester_stop(tester);
}

void
interface_tester_start(interface_st * const iface)
{
    /*
     * Occurs with a context configuration change.
     * As the configuration has changed the number of recovery tasks may have
     * changed, so ensure recoverys start afresh.
     */
    interface_tester_st * const tester = &iface->tester;

    ILOG("%s: %s", __func__, iface->name);

    iface->recovery.recovery_index = 0;
    tester_start(tester);
}

static void
tester_disconnected_handler(interface_tester_st * const tester)
{
    tester->starter = tester_start_disconnected;
    tester_stop(tester);
}

static void
tester_connected_handler(interface_tester_st * const tester)
{
    tester->starter = tester_start_connected;
    tester_initialise_per_connection_statistics(tester);
    tester_start(tester);
}

static bool
stopped_state_event_handler(
    interface_tester_st * const tester, tester_event_t const event)
{
    bool handled_event = true;

    switch (event)
    {
    case TESTER_EVENT_INTERFACE_SETTLED:
        tester_connected_handler(tester);
        break;

    case TESTER_EVENT_RECOVERY_TASK_TIMED_OUT:
    case TESTER_EVENT_RECOVERY_TASK_ENDED:
        /*
         * Ignore these events. They can occur if an interface disconnects while
         * a recovery task is running.
         * Recovery tasks aren't stopped after an interface disconnects, as that
         * is possibly exactly what the recovery task is trying to do.
         */
        break;

    default:
        handled_event = false;
        break;
    }

    return handled_event;
}

static bool
sleeping_state_event_handler(
    interface_tester_st * const tester, tester_event_t const event)
{
    bool handled_event = true;

    switch (event)
    {
    case TESTER_EVENT_TEST_RUN_REQUESTED:
        tester_interval_timer_stop(tester);
        tester_start(tester);
        break;

    case TESTER_EVENT_INTERVAL_TIMER_ELAPSED:
        tester_start(tester);
        break;

    case TESTER_EVENT_INTERFACE_DISCONNECTED:
        tester_disconnected_handler(tester);
        break;

    default:
        handled_event = false;
        break;
    }

    return handled_event;
}

static bool
testing_state_event_handler(
    interface_tester_st * const tester, tester_event_t const event)
{
    bool handled_event = true;

    switch (event)
    {
    case TESTER_EVENT_TEST_PASSED:
        tester_response_timer_stop(tester);
        interface_test_passed(tester);
        break;

    case TESTER_EVENT_TEST_FAILED:
        tester_response_timer_stop(tester);
        interface_test_failed(tester);
        break;

    case TESTER_EVENT_TEST_TIMED_OUT:
        /* The test took too long to complete. Call this a failure. */
        interface_tester_kill_process(&tester->test_proc);
        interface_test_failed(tester);
        break;

    case TESTER_EVENT_INTERFACE_DISCONNECTED:
        tester_disconnected_handler(tester);
        break;

    default:
        handled_event = false;
        break;
    }

    return handled_event;
}

static bool
recovering_state_event_handler(
    interface_tester_st * const tester, tester_event_t const event)
{
    bool handled_event = true;
    interface_st * const iface = container_of(tester, interface_st, tester);
    interface_recovery_st * const recovery = &iface->recovery;

    switch (event)
    {
    case TESTER_EVENT_RECOVERY_TASK_TIMED_OUT:
        interface_tester_kill_process(&recovery->proc);
        tester_sleep(tester);
        break;

    case TESTER_EVENT_RECOVERY_TASK_ENDED:
        recovery_response_timer_stop(recovery);
        tester_sleep(tester);
        break;

    case TESTER_EVENT_INTERFACE_DISCONNECTED:
        tester_disconnected_handler(tester);
        break;

    default:
        handled_event = false;
        break;
    }

    return handled_event;
}

typedef bool (*tester_event_handler_fn)(
    interface_tester_st * tester, tester_event_t event);

static const tester_event_handler_fn tester_event_handler_fns[] =
{
    [TESTER_STATE_STOPPED] = stopped_state_event_handler,
    [TESTER_STATE_SLEEPING] = sleeping_state_event_handler,
    [TESTER_STATE_TESTING] = testing_state_event_handler,
    [TESTER_STATE_RECOVERING] = recovering_state_event_handler,
};

static void
tester_event_handler(void * const event_ctx, tester_event_t const event)
{
    interface_tester_st * const tester = event_ctx;
    bool handled_event = false;
#if DEBUG
    interface_st * const iface = container_of(tester, interface_st, tester);
#endif

    DLOG("%s: handling event: %s in state %s",
         iface->name,
         tester_event_to_str(event),
         interface_tester_state_to_str(tester->state));

#if DEBUG
    assert(tester->state < ARRAY_SIZE(tester_event_handler_fns));
    assert(tester_event_handler_fns[tester->state] != NULL);
#endif

    handled_event = tester_event_handler_fns[tester->state](tester, event);

    if (handled_event)
    {
        DLOG("%s: handled event: %s - state now: %s",
             iface->name,
             tester_event_to_str(event),
             interface_tester_state_to_str(tester->state));
    }
    else
    {
        DLOG("%s: unhandled event: %s in state: %s",
             iface->name,
             tester_event_to_str(event),
             interface_tester_state_to_str(tester->state));
    }
}

void
interface_tester_send_event(interface_st * const iface, tester_event_t const event)
{
    event_queue_add_event(
        &iface->event_queue, tester_event_handler, &iface->tester, event);
}

