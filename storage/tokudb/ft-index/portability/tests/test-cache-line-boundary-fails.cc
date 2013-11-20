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
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "toku_config.h"
#include <memory.h>
#include <portability/toku_atomic.h>
#include "test.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

int verbose = 0;

static const size_t cachelinesize = 64;

// cache line is 64 bytes
// nine 7-byte structs fill 63 bytes
// the tenth spans one byte of the first cache line and six of the next cache line
// we first SFAA the first 9 structs and ensure we don't crash, then we set a signal handler and SFAA the 10th and ensure we do crash

struct unpackedsevenbytestruct {
    uint32_t i;
    char pad[3];
};
struct __attribute__((packed)) packedsevenbytestruct {
    uint32_t i;
    char pad[3];
};

struct packedsevenbytestruct *psevenbytestructs;
static __attribute__((__noreturn__)) void catch_abort (int sig __attribute__((__unused__))) {
    toku_free(psevenbytestructs);
#ifdef TOKU_DEBUG_PARANOID
    exit(EXIT_SUCCESS);  // with paranoid asserts, we expect to assert and reach this handler
#else
    exit(EXIT_FAILURE);  // we should not have crashed without paranoid asserts
#endif
}

int test_main(int UU(argc), char *const argv[] UU()) {
    if (sizeof(unpackedsevenbytestruct) != 8) {
        exit(EXIT_FAILURE);
    }
    if (sizeof(packedsevenbytestruct) != 7) {
        exit(EXIT_FAILURE);
    }

    {
        struct unpackedsevenbytestruct *MALLOC_N_ALIGNED(cachelinesize, 10, usevenbytestructs);
        if (usevenbytestructs == NULL) {
            // this test is supposed to crash, so exiting cleanly is a failure
            perror("posix_memalign");
            exit(EXIT_FAILURE);
        }

        for (int idx = 0; idx < 10; ++idx) {
            usevenbytestructs[idx].i = idx + 1;
            (void) toku_sync_fetch_and_add(&usevenbytestructs[idx].i, 32U - idx);
        }
        toku_free(usevenbytestructs);
    }

    
    MALLOC_N_ALIGNED(cachelinesize, 10, psevenbytestructs);
    if (psevenbytestructs == NULL) {
        // this test is supposed to crash, so exiting cleanly is a failure
        perror("posix_memalign");
        exit(EXIT_FAILURE);
    }

    for (int idx = 0; idx < 9; ++idx) {
        psevenbytestructs[idx].i = idx + 1;
        (void) toku_sync_fetch_and_add(&psevenbytestructs[idx].i, 32U - idx);
    }
    psevenbytestructs[9].i = 10;
    signal(SIGABRT, catch_abort);
    (void) toku_sync_fetch_and_add(&psevenbytestructs[9].i, 32U);

#ifdef TOKU_DEBUG_PARANOID
    exit(EXIT_FAILURE);  // with paranoid asserts, we should already have crashed
#else
    exit(EXIT_SUCCESS);  // without them, we should make it here
#endif
}
