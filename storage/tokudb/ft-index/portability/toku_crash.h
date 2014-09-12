/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
/*
COPYING CONDITIONS NOTICE:

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation, and provided that the
  following conditions are met:

      * Redistributions of source code must retain this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below).

      * Redistributions in binary form must reproduce this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below) in the documentation and/or other materials
        provided with the distribution.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301, USA.

COPYRIGHT NOTICE:

  TokuDB, Tokutek Fractal Tree Indexing Library.
  Copyright (C) 2007-2013 Tokutek, Inc.

DISCLAIMER:

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

UNIVERSITY PATENT NOTICE:

  The technology is licensed by the Massachusetts Institute of
  Technology, Rutgers State University of New Jersey, and the Research
  Foundation of State University of New York at Stony Brook under
  United States of America Serial No. 11/760379 and to the patents
  and/or patent applications resulting from it.

PATENT MARKING NOTICE:

  This software is covered by US Patent No. 8,185,551.
  This software is covered by US Patent No. 8,489,638.

PATENT RIGHTS GRANT:

  "THIS IMPLEMENTATION" means the copyrightable works distributed by
  Tokutek as part of the Fractal Tree project.

  "PATENT CLAIMS" means the claims of patents that are owned or
  licensable by Tokutek, both currently or in the future; and that in
  the absence of this license would be infringed by THIS
  IMPLEMENTATION or by using or running THIS IMPLEMENTATION.

  "PATENT CHALLENGE" shall mean a challenge to the validity,
  patentability, enforceability and/or non-infringement of any of the
  PATENT CLAIMS or otherwise opposing any of the PATENT CLAIMS.

  Tokutek hereby grants to you, for the term and geographical scope of
  the PATENT CLAIMS, a non-exclusive, no-charge, royalty-free,
  irrevocable (except as stated in this section) patent license to
  make, have made, use, offer to sell, sell, import, transfer, and
  otherwise run, modify, and propagate the contents of THIS
  IMPLEMENTATION, where such license applies only to the PATENT
  CLAIMS.  This grant does not include claims that would be infringed
  only as a consequence of further modifications of THIS
  IMPLEMENTATION.  If you or your agent or licensee institute or order
  or agree to the institution of patent litigation against any entity
  (including a cross-claim or counterclaim in a lawsuit) alleging that
  THIS IMPLEMENTATION constitutes direct or contributory patent
  infringement, or inducement of patent infringement, then any rights
  granted to you under this License shall terminate as of the date
  such litigation is filed.  If you or your agent or exclusive
  licensee institute or order or agree to the institution of a PATENT
  CHALLENGE, then Tokutek may terminate any rights granted to you
  under this License.
*/

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

void toku_try_gdb_stack_trace(const char *gdb_path);

#endif // PORTABILITY_TOKU_CRASH_H
