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

#include <unistd.h>
#ifdef HAVE_SYS_PRCTL_H
#include <sys/prctl.h>
#endif

#include <sys/wait.h>
#include <toku_race_tools.h>
#include "toku_crash.h"
#include "toku_atomic.h"

enum { MAX_GDB_ARGS = 128 };

static void
run_gdb(pid_t parent_pid, const char *gdb_path) {
    // 3 bytes per intbyte, null byte
    char pid_buf[sizeof(pid_t) * 3 + 1];
    char exe_buf[sizeof(pid_buf) + sizeof("/proc//exe")];

    // Get pid and path to executable.
    int n;
    n = snprintf(pid_buf, sizeof(pid_buf), "%d", parent_pid);
    invariant(n >= 0 && n < (int)sizeof(pid_buf));
    n = snprintf(exe_buf, sizeof(exe_buf), "/proc/%d/exe", parent_pid);
    invariant(n >= 0 && n < (int)sizeof(exe_buf));

    dup2(2, 1); // redirect output to stderr
    // Arguments are not dynamic due to possible security holes.
    execlp(gdb_path, gdb_path, "--batch", "-n",
           "-ex", "thread",
           "-ex", "bt",
           "-ex", "bt full",
           "-ex", "thread apply all bt",
           "-ex", "thread apply all bt full",
           exe_buf, pid_buf,
           NULL);
}

static void
intermediate_process(pid_t parent_pid, const char *gdb_path) {
    // Disable generating of core dumps
#if defined(HAVE_SYS_PRCTL_H)
    prctl(PR_SET_DUMPABLE, 0, 0, 0);
#endif
    pid_t worker_pid = fork();
    if (worker_pid < 0) {
        perror("spawn gdb fork: ");
        goto failure;
    }
    if (worker_pid == 0) {
        // Child (debugger)
        run_gdb(parent_pid, gdb_path);
        // Normally run_gdb will not return.
        // In case it does, kill the process.
        goto failure;
    } else {
        pid_t timeout_pid = fork();
        if (timeout_pid < 0) {
            perror("spawn timeout fork: ");
            kill(worker_pid, SIGKILL);
            goto failure;
        }

        if (timeout_pid == 0) {
            sleep(5);  // Timeout of 5 seconds
            goto success;
        } else {
            pid_t exited_pid = wait(NULL);  // Wait for first child to exit
            if (exited_pid == worker_pid) {
                // Kill slower child
                kill(timeout_pid, SIGKILL);
                goto success;
            } else if (exited_pid == timeout_pid) {
                // Kill slower child
                kill(worker_pid, SIGKILL);
                goto failure;  // Timed out.
            } else {
                perror("error while waiting for gdb or timer to end: ");
                //Some failure.  Kill everything.
                kill(timeout_pid, SIGKILL);
                kill(worker_pid, SIGKILL);
                goto failure;
            }
        }
    }
success:
    _exit(EXIT_SUCCESS);
failure:
    _exit(EXIT_FAILURE);
}

static void
spawn_gdb(const char *gdb_path) {
    pid_t parent_pid = getpid();
#if defined(HAVE_SYS_PRCTL_H)
    // On systems that require permission for the same user to ptrace,
    // give permission for this process and (more importantly) all its children to debug this process.
    prctl(PR_SET_PTRACER, parent_pid, 0, 0, 0);
#endif
    fprintf(stderr, "Attempting to use gdb @[%s] on pid[%d]\n", gdb_path, parent_pid);
    fflush(stderr);
    int intermediate_pid = fork();
    if (intermediate_pid < 0) {
        perror("spawn_gdb intermediate process fork: ");
    } else if (intermediate_pid == 0) {
        intermediate_process(parent_pid, gdb_path);
    } else {
        waitpid(intermediate_pid, NULL, 0);
    }
}

void
toku_try_gdb_stack_trace(const char *gdb_path) {
    char default_gdb_path[] = "/usr/bin/gdb";
    static bool started = false;
    if (RUNNING_ON_VALGRIND) {
        fprintf(stderr, "gdb stack trace skipped due to running under valgrind\n");
        fflush(stderr);
    } else if (toku_sync_bool_compare_and_swap(&started, false, true)) {
        spawn_gdb(gdb_path ? gdb_path : default_gdb_path);
    }
}

