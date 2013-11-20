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

#include "test.h"

bool pf_called;
bool fetch_called;
CACHEFILE f1;

static int
sleep_fetch (CACHEFILE f        __attribute__((__unused__)),
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
    sleep(2);
    *dirtyp = 0;
    *value = NULL;
    *sizep = make_pair_attr(8);
    fetch_called = true;
    return 0;
}

static bool sleep_pf_req_callback(void* UU(ftnode_pv), void* UU(read_extraargs)) {
    if (pf_called || fetch_called) return false;
    return true;
  return true;
}

static int sleep_pf_callback(void* UU(ftnode_pv), void* UU(disk_data), void* UU(read_extraargs), int UU(fd), PAIR_ATTR* sizep) {
   sleep(2);
  *sizep = make_pair_attr(8);
  pf_called = true;
  return 0;
}

static void *run_expensive_pf(void *arg) {
    void* v1;
    long s1;
    CACHETABLE_WRITE_CALLBACK wc = def_write_callback(NULL);
    pf_called = false;
    fetch_called = false;
    int r = toku_cachetable_get_and_pin(f1, make_blocknum(1), 1, &v1, &s1, wc, sleep_fetch, sleep_pf_req_callback, sleep_pf_callback, false, NULL);
    assert_zero(r);
    assert(pf_called);
    return arg;
}

static void *run_expensive_fetch(void *arg) {
    void* v1;
    long s1;
    CACHETABLE_WRITE_CALLBACK wc = def_write_callback(NULL);
    pf_called = false;
    fetch_called = false;
    int r = toku_cachetable_get_and_pin(f1, make_blocknum(1), 1, &v1, &s1, wc, sleep_fetch, sleep_pf_req_callback, sleep_pf_callback, false, NULL);
    assert_zero(r);
    assert(fetch_called);
    return arg;
}


static void
run_test (void) {
    const int test_limit = 12;
    int r;
    void *ret;
    CACHETABLE ct;
    toku_cachetable_create(&ct, test_limit, ZERO_LSN, NULL_LOGGER);
    const char *fname1 = TOKU_TEST_FILENAME;
    unlink(fname1);
    r = toku_cachetable_openf(&f1, ct, fname1, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); assert(r == 0);
    
    void* v1;
    long s1;
    CACHETABLE_WRITE_CALLBACK wc = def_write_callback(NULL);

    toku_pthread_t fetch_tid;
    fetch_called = false;
    r = toku_pthread_create(&fetch_tid, NULL, run_expensive_fetch, NULL); 
    sleep(1);
    r = toku_cachetable_get_and_pin(f1, make_blocknum(1), 1, &v1, &s1, wc, sleep_fetch, def_pf_req_callback, def_pf_callback, false, NULL);
    assert_zero(r);
    assert(fetch_called);
    r = toku_test_cachetable_unpin(f1, make_blocknum(1), 1, CACHETABLE_CLEAN, make_pair_attr(8));
    assert(r==0);
    r = toku_test_cachetable_unpin(f1, make_blocknum(1), 1, CACHETABLE_CLEAN, make_pair_attr(8));
    assert(r==0);
    r = toku_pthread_join(fetch_tid, &ret); 
    assert_zero(r);

    // call with may_modify_node = false twice, make sure we can get it
    r = toku_cachetable_get_and_pin(f1, make_blocknum(1), 1, &v1, &s1, wc, sleep_fetch, def_pf_req_callback, def_pf_callback, false, NULL);
    assert_zero(r);
    r = toku_cachetable_get_and_pin(f1, make_blocknum(1), 1, &v1, &s1, wc, sleep_fetch, def_pf_req_callback, def_pf_callback, false, NULL);
    assert_zero(r);
    r = toku_test_cachetable_unpin(f1, make_blocknum(1), 1, CACHETABLE_CLEAN, make_pair_attr(8));
    assert(r==0);
    r = toku_test_cachetable_unpin(f1, make_blocknum(1), 1, CACHETABLE_CLEAN, make_pair_attr(8));
    assert(r==0);

    toku_pthread_t pf_tid;
    pf_called = false;
    r = toku_pthread_create(&pf_tid, NULL, run_expensive_pf, NULL); 
    sleep(1);
    r = toku_cachetable_get_and_pin(f1, make_blocknum(1), 1, &v1, &s1, wc, sleep_fetch, def_pf_req_callback, def_pf_callback, false, NULL);
    assert_zero(r);
    assert(pf_called);
    r = toku_test_cachetable_unpin(f1, make_blocknum(1), 1, CACHETABLE_CLEAN, make_pair_attr(8));
    assert(r==0);
    r = toku_test_cachetable_unpin(f1, make_blocknum(1), 1, CACHETABLE_CLEAN, make_pair_attr(8));
    assert(r==0);
    
    r = toku_pthread_join(pf_tid, &ret); 
    assert_zero(r);

    toku_cachetable_verify(ct);
    toku_cachefile_close(&f1, false, ZERO_LSN);
    toku_cachetable_close(&ct);
}

int
test_main(int argc, const char *argv[]) {
  default_parse_args(argc, argv);
  run_test();
  return 0;
}
