/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id: cachetable-simple-verify.cc 52748 2013-01-31 21:12:42Z leifwalsh $"
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

#include "test.h"



//
// This test verifies the behavior that originally caused
// #5978 is fixed. Here is what we do. We have four pairs with
// blocknums and fullhashes of 1,2,3,4. The cachetable has only
// two bucket mutexes, so 1 and 3 share a pair mutex, as do 2 and 4.
// We pin all four with expensive write locks. Then, on backgroud threads,
// we call get_and_pin_nonblocking on 3, where the unlockers unpins 2, and
// we call get_and_pin_nonblocking on 4, where the unlockers unpins 1. Run this
// enough times, and we should see a deadlock before the fix, and no deadlock
// after the fix.
//

CACHEFILE f1;
PAIR p3;
PAIR p4;


static int
fetch_three (CACHEFILE f        __attribute__((__unused__)),
       PAIR UU(p),
       int UU(fd),
       CACHEKEY k         __attribute__((__unused__)),
       uint32_t fullhash __attribute__((__unused__)),
       void **value       __attribute__((__unused__)),
       void **dd     __attribute__((__unused__)),
       PAIR_ATTR *sizep        __attribute__((__unused__)),
       int  *dirtyp,
       void *extraargs    __attribute__((__unused__))
       ) {
    *dirtyp = 0;
    *value = NULL;
    *sizep = make_pair_attr(8);
    assert(k.b == 3);
    p3 = p;
    return 0;
}

static int
fetch_four (CACHEFILE f        __attribute__((__unused__)),
       PAIR UU(p),
       int UU(fd),
       CACHEKEY k         __attribute__((__unused__)),
       uint32_t fullhash __attribute__((__unused__)),
       void **value       __attribute__((__unused__)),
       void **dd     __attribute__((__unused__)),
       PAIR_ATTR *sizep        __attribute__((__unused__)),
       int  *dirtyp,
       void *extraargs    __attribute__((__unused__))
       ) {
    *dirtyp = 0;
    *value = NULL;
    *sizep = make_pair_attr(8);
    assert(k.b == 4);
    p4 = p;
    return 0;
}



static void
unpin_four (void* UU(v)) {
    int r = toku_cachetable_unpin_ct_prelocked_no_flush(
        f1,
        p3,
        CACHETABLE_DIRTY,
        make_pair_attr(8)
        );
    assert_zero(r);
}

static void
unpin_three (void* UU(v)) {
    int r = toku_cachetable_unpin_ct_prelocked_no_flush(
        f1,
        p4,
        CACHETABLE_DIRTY,
        make_pair_attr(8)
        );
    assert_zero(r);
}

static void *repin_one(void *UU(arg)) {
    CACHETABLE_WRITE_CALLBACK wc = def_write_callback(NULL);
    struct unlockers unlockers = {true, unpin_four, NULL, NULL};
    void* v1;
    long s1;
    int r = toku_cachetable_get_and_pin_nonblocking(
        f1,
        make_blocknum(1),
        1,
        &v1,
        &s1,
        wc,
        def_fetch,
        def_pf_req_callback,
        def_pf_callback,
        PL_WRITE_EXPENSIVE,
        NULL,
        &unlockers
        );
    assert(r == TOKUDB_TRY_AGAIN);
    return arg;
}


static void *repin_two(void *UU(arg)) {
    CACHETABLE_WRITE_CALLBACK wc = def_write_callback(NULL);
    struct unlockers unlockers = {true, unpin_three, NULL, NULL};
    void* v1;
    long s1;
    int r = toku_cachetable_get_and_pin_nonblocking(
        f1,
        make_blocknum(2),
        2,
        &v1,
        &s1,
        wc,
        def_fetch,
        def_pf_req_callback,
        def_pf_callback,
        PL_WRITE_EXPENSIVE,
        NULL,
        &unlockers
        );
    assert(r == TOKUDB_TRY_AGAIN);
    return arg;
}


static void
cachetable_test (void) {
    const int test_limit = 1000;
    int r;
    toku_pair_list_set_lock_size(2); // set two bucket mutexes
    CACHETABLE ct;
    toku_cachetable_create(&ct, test_limit, ZERO_LSN, NULL_LOGGER);
    const char *fname1 = TOKU_TEST_FILENAME;
    unlink(fname1);
    r = toku_cachetable_openf(&f1, ct, fname1, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); assert(r == 0);

    void* v1;
    long s1;
    CACHETABLE_WRITE_CALLBACK wc = def_write_callback(NULL);

    // bring pairs 1 and 2 into memory, then unpin
    r = toku_cachetable_get_and_pin(f1, make_blocknum(1), 1, &v1, &s1, wc, def_fetch, def_pf_req_callback, def_pf_callback, true, NULL);
    assert_zero(r);
    r = toku_cachetable_get_and_pin(f1, make_blocknum(2), 2, &v1, &s1, wc, def_fetch, def_pf_req_callback, def_pf_callback, true, NULL);
    assert_zero(r);


    // now pin pairs 3 and 4
    r = toku_cachetable_get_and_pin(f1, make_blocknum(3), 3, &v1, &s1, wc, fetch_three, def_pf_req_callback, def_pf_callback, true, NULL);
    assert_zero(r);
    r = toku_cachetable_get_and_pin(f1, make_blocknum(4), 4, &v1, &s1, wc, fetch_four, def_pf_req_callback, def_pf_callback, true, NULL);
    assert_zero(r);

    toku_pthread_t tid1;
    toku_pthread_t tid2;
    r = toku_pthread_create(&tid1, NULL, repin_one, NULL); 
    assert_zero(r);
    r = toku_pthread_create(&tid2, NULL, repin_two, NULL); 
    assert_zero(r);

    // unpin 1 and 2 so tid1 and tid2 can make progress
    usleep(512*1024);
    r = toku_test_cachetable_unpin(f1, make_blocknum(1), 1, CACHETABLE_DIRTY, make_pair_attr(8));
    assert_zero(r);
    r = toku_test_cachetable_unpin(f1, make_blocknum(2), 2, CACHETABLE_DIRTY, make_pair_attr(8));
    assert_zero(r);


    void *ret;
    r = toku_pthread_join(tid1, &ret); 
    assert_zero(r);
    r = toku_pthread_join(tid2, &ret); 
    assert_zero(r);    

    toku_cachetable_verify(ct);
    toku_cachefile_close(&f1, false, ZERO_LSN);
    toku_cachetable_close(&ct);
}

int
test_main(int argc, const char *argv[]) {
    default_parse_args(argc, argv);
    // test ought to run bunch of times in hope of hitting bug
    uint32_t num_test_runs = 30;
    for (uint32_t i = 0; i < num_test_runs; i++) {
        if (verbose) {
            printf("starting test run %" PRIu32 " \n", i);
        }
        cachetable_test();
    }
    return 0;
}
