#include "config.h"
#include "debug.h"
#include "ubus.h"
#include "shared.h"

#include <libubox/vlist.h>

#include <stdio.h>
#include <stdlib.h>

static bool
publish_objects(interface_tester_shared_st * const ctx)
{
    bool success;
    struct ubus_context * const ubus = &ctx->ubus_conn.ctx;

    DPRINTF("publishing objects\n");

    if (!ubus_add_main_object(ubus))
    {
        success = false;
        goto done;
    }

    ubus_subscribe_to_interface_events(&ctx->ubus_conn.ctx, &ctx->interface_events);
    ubus_subscribe_to_interface_state_events(&ctx->ubus_conn.ctx, &ctx->interface_state_events);

    success = true;

done:
    return success;
}

typedef void (*ubus_connect_cb)(struct ubus_context * ubus);

static void
ubus_connect_handler(struct ubus_context * const ubus)
{
    interface_tester_shared_st * const ctx =
            container_of(ubus, interface_tester_shared_st, ubus_conn.ctx);

    DPRINTF("connected to ubus\n");

    publish_objects(ctx);
    config_load_from_file_check(ctx);

    bool const are_connected = true;

    interface_tester_send_up_down_event(ubus, are_connected);
}

static void
context_shutdown(interface_tester_shared_st * const ctx)
{
    struct vlist_tree * const interfaces = &ctx->interfaces;
    struct ubus_context * const ubus = &ctx->ubus_conn.ctx;
    bool const are_connected = false;

    interface_tester_send_up_down_event(ubus, are_connected);
    interface_testers_free(interfaces);
}

static void
context_init(
    interface_tester_shared_st * const ctx,
    char const * const test_directory,
    char const * const recovery_directory,
    char const * const config_file)
{
    ctx->test_directory = test_directory;
    ctx->recovery_directory = recovery_directory;
    ctx->config_file = config_file;
    config_init(ctx);
}

static void
usage(FILE * const fp, const char * const progname)
{
    fprintf(fp, "Usage: %s [options]\n"
            "Options:\n"
            " -c <path>:        Path to the configuration file\n"
            " -s <path>:        Path to the ubus socket\n"
            " -S <path>:        Path to the test executable directory\n"
            " -r <path>:        Path to the recovery executable directory\n"
            "\n",
            progname);
}

int
main(int const argc, char * * const argv)
{
    interface_tester_shared_st ctx = {0};
    const char * ubus_path = NULL;
    const char * test_directory = NULL;
    const char * recovery_directory = NULL;
    const char * config_file = NULL;
    int ch;

    while ((ch = getopt(argc, argv, "s:S:r:c:")) != -1)
    {
        switch(ch)
        {
        case 'c':
            config_file = optarg;
            break;

        case 's':
            ubus_path = optarg;
            break;

        case 'S':
            test_directory = optarg;
            break;

        case 'r':
            recovery_directory = optarg;
            break;

        default:
            usage(stderr, argv[0]);
            return EXIT_FAILURE;
        }
    }

    uloop_init();

    context_init(&ctx, test_directory, recovery_directory, config_file);
    ubus_init(&ctx.ubus_conn, ubus_path, ubus_connect_handler);

    printf("Interface tester started\n");
    fflush(stdout);

    uloop_run();

    context_shutdown(&ctx);
    ubus_auto_shutdown(&ctx.ubus_conn);
    uloop_done();

    return EXIT_SUCCESS;
}

