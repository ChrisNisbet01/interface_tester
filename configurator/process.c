#include "process.h"
#include "debug.h"
#include "utils.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>

static void
redirect_fd(int const from, int const to, int const o_flag)
{
    int const fd = (from != -1) ? from : open("/dev/null", o_flag);

    if (fd > -1)
    {
        TEMP_FAILURE_RETRY(dup2(fd, to));
        close(fd);
    }
}

bool
event_processor_start_process(char * * const argv, char const * const working_dir)
{
    bool success;

    DPRINTF("working dir: %s exe:%s\n", working_dir, argv[0]);

    int const pid = fork();

    if (pid < 0)
    {
        success = false;
        goto done;
    }
    if (!pid) /* Child process. */
    {
        if (working_dir != NULL)
        {
            if (chdir(working_dir) < 0)
            {
                DPRINTF("chdir to: %s failed: %s\n",
                        working_dir, strerror(errno)); _exit(EXIT_FAILURE);
            }
        }
        redirect_fd(-1, STDIN_FILENO, O_RDONLY);
        redirect_fd(-1, STDOUT_FILENO, O_WRONLY);
        redirect_fd(-1, STDERR_FILENO, O_WRONLY);

        char * env[1] = { NULL };

        execvpe(argv[0], (char **)argv, env);
        DPRINTF("execvpe failed\n");
        _exit(127);
    }

    success = true;

done:
    return success;
}

