/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."

#ifndef PORTABILITY_TOKU_CRASH_H
#define PORTABILITY_TOKU_CRASH_H

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include "toku_assert.h"

//Simulate as hard a crash as possible.
//Choices:
//  raise(SIGABRT)
//  kill -SIGKILL $pid
//  divide by 0
//  null dereference
//  abort()
//  assert(false) (from <assert.h>)
//  assert(false) (from <toku_assert.h>)
//
//Linux:
//  abort() and both assert(false) cause FILE buffers to be flushed and written to disk: Unacceptable
//Windows:
//  None of them cause file buffers to be flushed/written to disk, however
//  abort(), assert(false) <assert.h>, null dereference, and divide by 0 cause popups requiring user intervention during tests: Unacceptable
//
//kill -SIGKILL $pid is annoying (and so far untested)
//
//raise(SIGABRT) has the downside that perhaps it could be caught?
//I'm choosing raise(SIGABRT), followed by divide by 0, followed by null dereference, followed by all the others just in case one gets caught.
static void __attribute__((unused, noreturn))
toku_hard_crash_on_purpose(void) {
#if TOKU_WINDOWS
    TerminateProcess(GetCurrentProcess(), 137);
#else
    raise(SIGKILL); //Does not flush buffers on linux; cannot be caught.
#endif
    {
        int zero = 0;
        int infinity = 1/zero;
        fprintf(stderr, "Force use of %d\n", infinity);
        fflush(stderr); //Make certain the string is calculated.
    }
    {
        void * intothevoid = NULL;
        (*(int*)intothevoid)++;
        fprintf(stderr, "Force use of *(%p) = %d\n", intothevoid, *(int*)intothevoid);
        fflush(stderr);
    }
    abort();
    fprintf(stderr, "This line should never be printed\n");
    fflush(stderr);
}

// Similar to toku_hard_crash_on_purpose, but the goal isn't to crash hard, the primary goal is to get a corefile, the secondary goal is to terminate in any way possible.
// We don't really care if buffers get flushed etc, in fact they may as well flush since there may be useful output in stdout or stderr.
//
// By default, the following signals generate cores:
//  Linux, from signal(7):
//     SIGQUIT       3       Core
//     SIGILL        4       Core
//     SIGABRT       6       Core
//     SIGFPE        8       Core
//     SIGSEGV      11       Core
//
//  Darwin and FreeBSD, from signal(3):
//     3     SIGQUIT      create core image
//     4     SIGILL       create core image
//     5     SIGTRAP      create core image
//     6     SIGABRT      create core image
//     7     SIGEMT       create core image
//     8     SIGFPE       create core image
//     10    SIGBUS       create core image
//     11    SIGSEGV      create core image
//     12    SIGSYS       create core image
//
// We'll raise these in some sequence (common ones first), then try emulating the things that would cause these signals to be raised, then eventually just try to die normally and then loop like abort does.
// Start with a toku assert because that hopefully prints a stacktrace.
static void __attribute__((unused, noreturn))
toku_crash_and_dump_core_on_purpose(void) {
    assert(false);
    invariant(0);
    raise(SIGQUIT);
    raise(SIGILL);
    raise(SIGABRT);
    raise(SIGFPE);
    raise(SIGSEGV);
#if defined(__FreeBSD__) || defined(__APPLE__)
    raise(SIGTRAP);
    raise(SIGEMT);
    raise(SIGBUS);
    raise(SIGSYS);
#endif
    abort();
    {
        int zero = 0;
        int infinity = 1/zero;
        fprintf(stderr, "Force use of %d\n", infinity);
        fflush(stderr); //Make certain the string is calculated.
    }
    {
        void * intothevoid = NULL;
        (*(int*)intothevoid)++;
        fprintf(stderr, "Force use of *(%p) = %d\n", intothevoid, *(int*)intothevoid);
        fflush(stderr);
    }
    raise(SIGKILL);
    while (true) {
        // don't return
    }
}

#endif // PORTABILITY_TOKU_CRASH_H
