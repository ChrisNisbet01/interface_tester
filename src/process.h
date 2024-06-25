#pragma once

#include <libubox/uloop.h>
#include <libubox/blob.h>

#include <stdbool.h>

typedef struct tester_process_st tester_process_st;

typedef void (*tester_process_cb)(tester_process_st * proc, int status);

struct tester_process_st
{
    struct uloop_process uloop;
    tester_process_cb cb; /* Called when the process exits. */
};

void
interface_tester_kill_process(tester_process_st * proc);

bool
interface_tester_start_process(
    tester_process_st * proc, char * * argv, char const * working_dir);

