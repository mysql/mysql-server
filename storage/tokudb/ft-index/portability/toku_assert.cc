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

#include "toku_config.h"

#include <toku_portability.h>
#include "toku_assert.h"

#include <stdlib.h>
#include <stdio.h>
#if defined(HAVE_MALLOC_H)
# include <malloc.h>
#elif defined(HAVE_SYS_MALLOC_H)
# include <sys/malloc.h>
#endif
#include <dlfcn.h>
#if !TOKU_WINDOWS
#include <execinfo.h>
#endif


#if !TOKU_WINDOWS
#define N_POINTERS 1000
// These are statically allocated so that the backtrace can run without any calls to malloc()
static void *backtrace_pointers[N_POINTERS];
#endif

static uint64_t engine_status_num_rows = 0;

typedef void (*malloc_stats_fun_t)(void);
static malloc_stats_fun_t malloc_stats_f;

void
toku_assert_init(void)
{
    malloc_stats_f = (malloc_stats_fun_t) dlsym(RTLD_DEFAULT, "malloc_stats");
}

// Function pointers are zero by default so asserts can be used by brt-layer tests without an environment.
static int (*toku_maybe_get_engine_status_text_p)(char* buff, int buffsize) = 0;
static void (*toku_maybe_set_env_panic_p)(int code, const char* msg) = 0;

void toku_assert_set_fpointers(int (*toku_maybe_get_engine_status_text_pointer)(char*, int), 
			       void (*toku_maybe_set_env_panic_pointer)(int, const char*),
                               uint64_t num_rows) {
    toku_maybe_get_engine_status_text_p = toku_maybe_get_engine_status_text_pointer;
    toku_maybe_set_env_panic_p = toku_maybe_set_env_panic_pointer;
    engine_status_num_rows = num_rows;
}

bool toku_gdb_dump_on_assert = false;
void (*do_assert_hook)(void) = NULL;

static void toku_do_backtrace_abort(void) __attribute__((noreturn));

static void 
toku_do_backtrace_abort(void) {

    // backtrace
#if !TOKU_WINDOWS
    int n = backtrace(backtrace_pointers, N_POINTERS);
    fprintf(stderr, "Backtrace: (Note: toku_do_assert=0x%p)\n", toku_do_assert); fflush(stderr);
    backtrace_symbols_fd(backtrace_pointers, n, fileno(stderr));
#endif

    fflush(stderr);
    
    if (engine_status_num_rows && toku_maybe_get_engine_status_text_p) {
	int buffsize = engine_status_num_rows * 128;  // assume 128 characters per row (gross overestimate, should be safe)
	char buff[buffsize];	
	toku_maybe_get_engine_status_text_p(buff, buffsize);  
	fprintf(stderr, "Engine status:\n%s\n", buff);
    }
    else
	fprintf(stderr, "Engine status function not available\n");
    fprintf(stderr, "Memory usage:\n");
    fflush(stderr);	    // just in case malloc_stats() crashes, we still want engine status (and to know that malloc_stats() failed)
    if (malloc_stats_f) {
        malloc_stats_f();
    }
    fflush(stderr);	    

    if (do_assert_hook) do_assert_hook();
    if (toku_gdb_dump_on_assert) {
        toku_try_gdb_stack_trace(nullptr);
    }

#if TOKU_WINDOWS
    //Following commented methods will not always end the process (could hang).
    //They could be unacceptable for other reasons as well (popups,
    //flush buffers before quitting, etc)
    //  abort()
    //  assert(false) (assert.h assert)
    //  raise(SIGABRT)
    //  divide by 0
    //  null dereference
    //  _exit
    //  exit
    //  ExitProcess
    TerminateProcess(GetCurrentProcess(), 134); //Only way found so far to unconditionally
    //Terminate the process
#endif

    abort();
}


static void
set_panic_if_not_panicked(int caller_errno, char * msg) {
    int code = caller_errno ? caller_errno : -1;
    if (toku_maybe_set_env_panic_p) {
	toku_maybe_set_env_panic_p(code, msg);
    }
}


#define MSGLEN 1024

void 
toku_do_assert_fail (const char *expr_as_string, const char *function, const char *file, int line, int caller_errno) {
    char msg[MSGLEN];
    snprintf(msg, MSGLEN, "%s:%d %s: Assertion `%s' failed (errno=%d)\n", file, line, function, expr_as_string, caller_errno);
    perror(msg);
    set_panic_if_not_panicked(caller_errno, msg);
    toku_do_backtrace_abort();
}

void 
toku_do_assert_zero_fail (uintptr_t expr, const char *expr_as_string, const char *function, const char *file, int line, int caller_errno) {
    char msg[MSGLEN];
    snprintf(msg, MSGLEN, "%s:%d %s: Assertion `%s == 0' failed (errno=%d) (%s=%" PRIuPTR ")\n", file, line, function, expr_as_string, caller_errno, expr_as_string, expr);
    perror(msg);
    set_panic_if_not_panicked(caller_errno, msg);
    toku_do_backtrace_abort();
}

void
toku_do_assert_expected_fail (uintptr_t expr, uintptr_t expected, const char *expr_as_string, const char *function, const char *file, int line, int caller_errno) {
    char msg[MSGLEN];
    snprintf(msg, MSGLEN, "%s:%d %s: Assertion `%s == %" PRIuPTR "' failed (errno=%d) (%s=%" PRIuPTR ")\n", file, line, function, expr_as_string, expected, caller_errno, expr_as_string, expr);
    perror(msg);
    set_panic_if_not_panicked(caller_errno, msg);
    toku_do_backtrace_abort();
}

void 
toku_do_assert(int expr, const char *expr_as_string, const char *function, const char* file, int line, int caller_errno) {
    if (expr == 0)
        toku_do_assert_fail(expr_as_string, function, file, line, caller_errno);
}

