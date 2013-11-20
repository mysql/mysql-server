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

// Demonstrate a race if #5833 isn't fixed.

#include <pthread.h>
#include <toku_portability.h>
#include <util/partitioned_counter.h>
#include "test.h"


static void pt_create (pthread_t *thread, void *(*f)(void*), void *extra) {
    int r = pthread_create(thread, NULL, f, extra);
    assert(r==0);
}

static void pt_join (pthread_t thread, void *expect_extra) {
    void *result;
    int r = pthread_join(thread, &result);
    assert(r==0);
    assert(result==expect_extra);
}

static int verboseness_cmdarg=0;

static void parse_args (int argc, const char *argv[]) {
    const char *progname = argv[1];
    argc--; argv++;
    while (argc>0) {
	if (strcmp(argv[0], "-v")==0) verboseness_cmdarg++;
	else {
	    printf("Usage: %s [-v]\n", progname);
	    exit(1);
	}
	argc--; argv++;
    }
}

#define NCOUNTERS 2
PARTITIONED_COUNTER array_of_counters[NCOUNTERS];

static void *counter_init_fun(void *tnum_pv) {
    int *tnum_p = (int*)tnum_pv;
    int tnum = *tnum_p;
    assert(0<=tnum  && tnum<NCOUNTERS);
    array_of_counters[tnum] = create_partitioned_counter();
    return tnum_pv;
}

static void do_test_5833(void) {
    pthread_t threads[NCOUNTERS];
    int       tids[NCOUNTERS];
    for (int i=0; i<NCOUNTERS; i++) {
        tids[i] = i;
        pt_create(&threads[i], counter_init_fun, &tids[i]);
    }
    for (int i=0; i<NCOUNTERS; i++) {
        pt_join(threads[i], &tids[i]);
        destroy_partitioned_counter(array_of_counters[i]);
    }
}

int test_main(int argc, const char *argv[]) {
    parse_args(argc, argv);
    do_test_5833();
    return 0;
}
