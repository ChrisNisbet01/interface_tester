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

static void
interface_tester_process_cb(struct uloop_process * const proc, int const ret)
{
    tester_process_st * const np =
        container_of(proc, struct tester_process_st, uloop);

    if (np->cb != NULL)
    {
        np->cb(np, ret);
    }
}

void
interface_tester_kill_process(tester_process_st * const proc)
{
    if (!proc->uloop.pending)
    {
        goto done;
    }

	kill(proc->uloop.pid, SIGKILL);
	uloop_process_delete(&proc->uloop);

done:
    return;
}

bool
interface_tester_start_process(
    tester_process_st * const proc, char * * const argv, char const * const working_dir)
{
    bool success;

    interface_tester_kill_process(proc);

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
                DLOG("chdir to: %s failed: %s",
                     working_dir, strerror(errno)); _exit(EXIT_FAILURE);
            }
        }
        redirect_fd(-1, STDIN_FILENO, O_RDONLY);
        redirect_fd(-1, STDOUT_FILENO, O_WRONLY);
        redirect_fd(-1, STDERR_FILENO, O_WRONLY);

        char * env[1] = { NULL };

        execvpe(argv[0], (char **)argv, env);

        _exit(127);
    }

    proc->uloop.cb = interface_tester_process_cb;
    proc->uloop.pid = pid;
    uloop_process_add(&proc->uloop);

    success = true;

done:
    return success;
}

