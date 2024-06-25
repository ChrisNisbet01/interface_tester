#include "event_processor.h"
#include "debug.h"
#include "process.h"

#include <libgen.h>
#include <string.h>

void
run_event_processor(
    char const * const event_processor,
    char const * const interface_name,
    bool const is_operational)
{
    int argc = 0;
    char * argv[10];
    char * temp_exename = strdup(event_processor);
    char * temp_dirname = strdup(event_processor);
    char * exe_name = basename(temp_exename);
    char * dir_name = dirname(temp_dirname);
    char * full_exe_name = NULL;

    if (asprintf(&full_exe_name, "./%s", exe_name) < 0)
    {
        goto done;
    }

    argv[argc++] = full_exe_name;
    argv[argc++] = (char *)interface_name;
    argv[argc++] = is_operational ? "operational" : "broken";
    argv[argc++] = NULL;

    DPRINTF("%s: %d\n", interface_name, is_operational);

    if (!event_processor_start_process(argv, dir_name))
    {
        DPRINTF("%s: failed to run event processor\n", interface_name);
    }

done:
    free(full_exe_name);
    free(temp_exename);
    free(temp_dirname);
}

