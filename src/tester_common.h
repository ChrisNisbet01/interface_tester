#pragma once

#include "configure.h"
#include "event_queue.h"
#include "interface_tester_events.h"
#include "process.h"
#include "shared.h"
#include "timers.h"

#include <libubus.h>
#include <libubox/vlist.h>

extern const unsigned int msecs_per_sec;

typedef struct test_config_st
{
    size_t index;
    /*
     * The name of the executable that will be called to execute the configured
     * test.
     */
    char const * executable_name;
    char const * label;

    /*
     * The default maximum time to wait for an individual test to complete.
     * This overrides the default response timeout.
     */
    uint32_t response_timeout_secs;
    struct blob_attr * params;
} test_config_st;

typedef struct recovery_config_st
{
    size_t index;
    /*
     * The name of the executable that will be called to execute the configured
     * recovery task.
     */
    char const * executable_name;
    char const * label;

    /*
     * The default maximum time to wait for an individual test to complete.
     * This overrides the default response timeout.
     */
    uint32_t response_timeout_secs;
    struct blob_attr * params;
} recovery_config_st;

typedef enum test_run_success_condition_t
{
    test_run_success_condition_one, /* One test in the list of tests must pass. */
    test_run_success_condition_all, /* All tests must pass. */
    test_run_success_condition_COUNT, /* Must be last in the list. */
} test_run_success_condition_t;

typedef struct success_condition_st
{
    char const * name;
    test_run_success_condition_t condition;
} success_condition_st;

typedef struct interface_config_st
{
    success_condition_st const * success_condition;

    /* Delay after interface connects before initiating a test run.  */
    uint32_t settling_delay_secs;

    /* The interval between test runs when the tests are passing. */
    uint32_t test_passing_interval_secs;

    /* The interval between test runs when the tests are failing. */
    uint32_t test_failing_interval_secs;

    /*
     * The number of times a test run must pass before declaring the interface
     * operational.
     */
    uint32_t pass_threshold;

    /*
     * The number of times a test run must fail before delcaring the interface
     * not operational.
     */
    uint32_t fail_threshold;

    /* The default maximum time to wait for an individual test to complete. */
    uint32_t response_timeout_secs;

#if WITH_METRICS_ADJUSTMENT
    /*
     * The amount by which to increase the metrics of routes attached to this
     * interface when the tests are failing.
     */
    uint32_t failing_tests_metrics_increase;
#endif

    size_t num_tests;
    test_config_st * tests;

    size_t num_recoverys;
    recovery_config_st * recoverys;
} interface_config_st;

typedef enum interface_recovery_state_t
{
    RECOVERY_STATE_OPERATIONAL,
    RECOVERY_STATE_BROKEN,
    RECOVERY_STATE_COUNT__, /* Must be last in the list. */
} interface_recovery_state_t;

typedef struct interface_recovery_st
{
    interface_recovery_state_t state;
#if WITH_METRICS_ADJUSTMENT
    bool metrics_are_adjusted;
#endif
    timer_st response_timeout_timer;
    size_t recovery_index;
    tester_process_st proc;
} interface_recovery_st;

typedef enum interface_connection_state_t
{
    CONNECTION_STATE_DISCONNECTED,
    CONNECTION_STATE_SETTLING,
    CONNECTION_STATE_CONNECTED,
    CONNECTION_STATE_COUNT__,
} interface_connection_state_t;

typedef struct interface_connection_st
{
    interface_connection_state_t state;
    timer_st settling_delay_timer;
} interface_connection_st;

typedef enum interface_tester_state_t
{
    TESTER_STATE_STOPPED,
    TESTER_STATE_SLEEPING,
    TESTER_STATE_TESTING,
    TESTER_STATE_RECOVERING,
    TESTER_STATE_COUNT__,
} interface_tester_state_t;

typedef struct interface_tester_st interface_tester_st;
typedef void (*tester_start_fn)(interface_tester_st * tester);

typedef struct test_statistics_st
{
    uint64_t total_passes_this_connection;
    uint64_t total_failures_this_connection;
    uint64_t total_passes;
    uint64_t total_failures;
} test_statistics_st;

typedef struct test_run_statistics_st
{
    uint64_t consecutive_passes;
    uint64_t total_passes_this_connection;
    uint64_t total_passes;
    uint64_t consecutive_failures;
    uint64_t total_failures_this_connection;
    uint64_t total_failures;
} test_run_statistics_st;

typedef struct test_recovery_statistics_st
{
    uint64_t total_this_connection;
    uint64_t total;
} test_recovery_statistics_st;

typedef struct tester_statistics_st
{
    test_statistics_st tests;
    test_run_statistics_st test_runs;
    test_recovery_statistics_st recovery;
} tester_statistics_st;

struct interface_tester_st
{
    interface_tester_state_t state;
    size_t test_index;
    tester_start_fn starter;
    tester_process_st test_proc;
    timer_st test_response_timeout_timer;
    timer_st test_interval_timer;

    int last_test_exit_code;
    bool last_test_passed;

    tester_statistics_st stats;
};

typedef struct interface_st
{
    interface_tester_shared_st * ctx;
    struct vlist_node node;
    const char * name;
    struct ubus_object ubus_object;
    event_q_st event_queue;
    interface_config_st config;
    interface_connection_st connection;
    interface_recovery_st recovery;
    interface_tester_st tester;
} interface_st;

void
interface_tester_config_free(interface_config_st * config);

void
interface_tester_free(interface_st * iface);

interface_st *
interface_tester_alloc(interface_tester_shared_st * ctx, const char * name);

interface_st *
interface_tester_lookup_by_name(
    interface_tester_shared_st * ctx, char const * interface_name);

void
interface_testers_free(struct vlist_tree * interfaces);

char const *
tester_event_to_str(tester_event_t event);

