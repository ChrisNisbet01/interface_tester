#include "configurator.h"
#include "debug.h"
#include "ubus.h"
#include "utils.h"

#include <libubox/vlist.h>

#include <stdio.h>
#include <stdlib.h>

typedef void (*ubus_connect_cb)(struct ubus_context * ubus);

static void
ubus_connect_handler(struct ubus_context * const ubus)
{
    configurator_st * const ctx =
            container_of(ubus, configurator_st, ubus_conn.ctx);

    DPRINTF("connected to ubus and ctx: %p\n", ctx);

    ubus_subscribe_to_tester_events(&ctx->ubus_conn.ctx, &ctx->interface_events);
}

static void
context_shutdown(configurator_st * const ctx)
{
    UNUSED(ctx);
}

static void
context_init(
    configurator_st * const ctx,
    char const * const config_path,
    char const * const event_processor_path)
{
    ctx->config_path = config_path;
    ctx->event_processor_path = event_processor_path;
}

static void
usage(FILE * const fp, const char * const progname)
{
    fprintf(fp, "Usage: %s [options] <config_path>\n"
            "Options:\n"
            " -s <path>:        Path to the ubus socket\n"
            " -c <path>:        Path to the configuration file\n"
            " -e <path>:        Path to the event processor\n"
            "\n",
            progname);
}

int
main(int const argc, char * * const argv)
{
    configurator_st ctx = {0};
    const char * ubus_path = NULL;
    const char * config_path = NULL;
    const char * event_processor_path = NULL;
    int ch;

    while ((ch = getopt(argc, argv, "s:c:e:")) != -1)
    {
        switch(ch)
        {
        case 's':
            ubus_path = optarg;
            break;

        case 'c':
         config_path = optarg;
         break;

        case 'e':
         event_processor_path = optarg;
         break;

        default:
            usage(stderr, argv[0]);
            return EXIT_FAILURE;
        }
    }

    uloop_init();

    context_init(&ctx, config_path, event_processor_path);
    ubus_init(&ctx.ubus_conn, ubus_path, ubus_connect_handler);

    printf("Configurator started\n");
    fflush(stdout);

    uloop_run();

    context_shutdown(&ctx);
    ubus_auto_shutdown(&ctx.ubus_conn);
    uloop_done();

    return EXIT_SUCCESS;
}
