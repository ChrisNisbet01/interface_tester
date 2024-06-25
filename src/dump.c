#include "dump.h"
#include "interface_connection.h"
#include "interface_tester.h"
#include "strings.h"
#include "utils.h"

static void
dump_timer_state(struct blob_buf * const b, timer_st const * const t)
{
    void * cky = blobmsg_open_table(b, timer_label(t));

    blobmsg_add_u8(b, "running", timer_is_running(t));
    blobmsg_add_u64(b, "remaining", timer_remaining(t));

    blobmsg_close_table(b, cky);
}

static void
dump_connection_state(struct blob_buf * const b, interface_st const * const iface)
{
    interface_connection_st const * const connection = &iface->connection;
    void * const cky = blobmsg_open_table(b, "interface");

    blobmsg_add_string(
        b, "connected", (connection->state != CONNECTION_STATE_DISCONNECTED) ? "yes" : "no");
    blobmsg_add_string(
        b, "state", interface_connection_state_to_str(connection->state));
    dump_timer_state(b, &connection->settling_delay_timer);

    blobmsg_close_table(b, cky);
}

static void
dump_test_run_statistics(
    struct blob_buf * const b, test_run_statistics_st const * const stats)
{
    void * const cky = blobmsg_open_table(b, "test_runs");

    blobmsg_add_u64(b, "consecutive_passes", stats->consecutive_passes);
    blobmsg_add_u64(
        b, "total_passes_this_connection", stats->total_passes_this_connection);
    blobmsg_add_u64(b, "total_passes", stats->total_passes);
    blobmsg_add_u64(b, "consecutive_failures", stats->consecutive_failures);
    blobmsg_add_u64(
        b, "total_failures_this_connection", stats->total_failures_this_connection);
    blobmsg_add_u64(b, "total_failures", stats->total_failures);

    blobmsg_close_table(b, cky);
}

static void
dump_test_statistics(
    struct blob_buf * const b, test_statistics_st const * const stats)
{
    void * const cky = blobmsg_open_table(b, "tests");

    blobmsg_add_u64(
        b, "total_passes_this_connection", stats->total_passes_this_connection);
    blobmsg_add_u64(b, "total_passes", stats->total_passes);
    blobmsg_add_u64(
        b, "total_failures_this_connection", stats->total_failures_this_connection);
    blobmsg_add_u64(b, "total_failures", stats->total_failures);

    blobmsg_close_table(b, cky);
}

static void dump_recovery_statistics(
    struct blob_buf * const b, test_recovery_statistics_st const * const stats)
{
    void * const cky = blobmsg_open_table(b, "recovery");

    blobmsg_add_u64(b, "total_this_connection", stats->total_this_connection);
    blobmsg_add_u64(b, "total", stats->total);

    blobmsg_close_table(b, cky);
}

static void
dump_tester_stats(struct blob_buf * const b, interface_tester_st const * const tester)
{
    void * const stats_cky = blobmsg_open_table(b, "stats");

    dump_test_run_statistics(b, &tester->stats.test_runs);
    dump_test_statistics(b, &tester->stats.tests);
    dump_recovery_statistics(b, &tester->stats.recovery);

    blobmsg_close_table(b, stats_cky);
}

static void
dump_tester_state(struct blob_buf * const b, interface_st const * const iface)
{
    interface_tester_st const * const tester = &iface->tester;
    interface_recovery_st const * const recovery = &iface->recovery;
    interface_config_st const * const config = &iface->config;

    void * const cky = blobmsg_open_table(b, "tester");

    blobmsg_add_u32(b, "test_index", (uint32_t)tester->test_index);

    blobmsg_add_string(
        b, "state", interface_tester_state_to_str(tester->state));
    blobmsg_add_string(
        b, "operational_state", interface_recovery_state_to_str(recovery->state));

    blobmsg_add_u8(b, "metrics_are_adjusted", recovery->metrics_are_adjusted);

    dump_timer_state(b, &tester->test_response_timeout_timer);
    dump_timer_state(b, &tester->test_interval_timer);
    dump_timer_state(b, &recovery->response_timeout_timer);

    if (config->num_recoverys > 0)
    {
        blobmsg_add_u32(b, "next_recovery_task", recovery->recovery_index);
        blobmsg_add_string(
            b, "next_recovery_label", config->recoverys[recovery->recovery_index].label);
    }

    blobmsg_add_u8(
        b, "test_process_running", tester->test_proc.uloop.pending);
    if (tester->test_proc.uloop.pending)
    {
        blobmsg_add_u32(b, "test_process_pid", tester->test_proc.uloop.pid);
    }
    blobmsg_add_u32(b, "last_test_exit_code", tester->last_test_exit_code);
    blobmsg_add_u8(b, "last_test_passed", tester->last_test_passed);

    blobmsg_add_u8(
        b, "recovery_task_running", recovery->proc.uloop.pending);
    if (recovery->proc.uloop.pending)
    {
        blobmsg_add_u32(b, "recovery_task_process_pid", recovery->proc.uloop.pid);
    }

    dump_tester_stats(b, tester);

    blobmsg_close_table(b, cky);
}

static void
interface_dump_state(interface_st const * const iface, struct blob_buf * const b)
{
    void * const cky = blobmsg_open_table(b, "state");

    dump_connection_state(b, iface);
    dump_tester_state(b, iface);

    blobmsg_close_table(b, cky);
}

static void
interface_dump_test_config(
    interface_config_st const * const config, struct blob_buf * const b)
{
    void * const cky = blobmsg_open_array(b, Stests);

    for (size_t i = 0; i < config->num_tests; i++)
    {
        test_config_st const * const test = &config->tests[i];
        void * const test_cky = blobmsg_open_table(b, NULL);

        blobmsg_add_string(b, Sexecutable, test->executable_name);
        blobmsg_add_string(b, Slabel, test->label);
        blobmsg_add_u32(b, Sresponse_timeout_secs, test->response_timeout_secs);
        blobmsg_add_blob(b, test->params);

        blobmsg_close_table(b, test_cky);
    }

    blobmsg_close_array(b, cky);
}

static void
interface_dump_recovery_config(
    interface_config_st const * const config, struct blob_buf * const b)
{
    void * const cky = blobmsg_open_array(b, Srecovery_tasks);

    for (size_t i = 0; i < config->num_recoverys; i++)
    {
        recovery_config_st const * const recovery = &config->recoverys[i];
        void * const recoverys_cky = blobmsg_open_table(b, NULL);

        blobmsg_add_string(b, Sexecutable, recovery->executable_name);
        blobmsg_add_string(b, Slabel, recovery->label);
        blobmsg_add_u32(b, Sresponse_timeout_secs, recovery->response_timeout_secs);
        blobmsg_add_blob(b, recovery->params);

        blobmsg_close_table(b, recoverys_cky);
    }

    blobmsg_close_array(b, cky);
}

static void
interface_dump_config(interface_st * const iface, struct blob_buf * const b)
{
    interface_config_st const * const config = &iface->config;
    void * const cky = blobmsg_open_table(b, Sconfig);

    blobmsg_add_string(b, Ssuccess_condition, config->success_condition->name);
    blobmsg_add_u32(b, Ssettling_delay_secs, config->settling_delay_secs);
    blobmsg_add_u32(b, Spassing_interval_secs, config->test_passing_interval_secs);
    blobmsg_add_u32(b, Sfailing_interval_secs, config->test_failing_interval_secs);
    blobmsg_add_u32(b, Spass_threshold, config->fail_threshold);
    blobmsg_add_u32(b, Sfail_threshold, config->pass_threshold);
    blobmsg_add_u32(b, Sresponse_timeout_secs, config->response_timeout_secs);
    blobmsg_add_u32(b, Sfailing_tests_metrics_increase, config->failing_tests_metrics_increase);

    interface_dump_test_config(config, b);
    interface_dump_recovery_config(config, b);

    blobmsg_close_table(b, cky);
}

void
interface_state_dump(interface_st * const iface, struct blob_buf * const b)
{
    interface_dump_state(iface, b);
    interface_dump_config(iface, b);
}

void
interface_states_dump(
    interface_tester_shared_st * const ctx, struct blob_buf * const b)
{
    interface_st * iface;

    vlist_for_each_element(&ctx->interfaces, iface, node)
    {
        void * const iface_cky = blobmsg_open_table(b, iface->name);

        interface_state_dump(iface, b);

        blobmsg_close_table(b, iface_cky);
    }
}

