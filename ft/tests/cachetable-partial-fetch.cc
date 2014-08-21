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

  TokuFT, Tokutek Fractal Tree Indexing Library.
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
// This file contains some basic tests for partial fetch, ensuring that 
// it works correctly
//

uint32_t fetch_val = 0;
bool pf_req_called;

static int
fetch (CACHEFILE f        __attribute__((__unused__)),
       PAIR UU(p),
       int UU(fd),
       CACHEKEY k         __attribute__((__unused__)),
       uint32_t fullhash __attribute__((__unused__)),
       void **value       __attribute__((__unused__)),
       void** UU(dd),
       PAIR_ATTR *sizep        __attribute__((__unused__)),
       int  *dirtyp,
       void *extraargs    __attribute__((__unused__))
       ) {
  *dirtyp = 0;
  *value = &fetch_val;
  *sizep = make_pair_attr(sizeof(fetch_val));
  return 0;
}

static int
err_fetch (CACHEFILE f        __attribute__((__unused__)),
       PAIR UU(p),
       int UU(fd),
       CACHEKEY k         __attribute__((__unused__)),
       uint32_t fullhash __attribute__((__unused__)),
       void **value       __attribute__((__unused__)),
	   void** UU(dd),
       PAIR_ATTR *sizep        __attribute__((__unused__)),
       int  *dirtyp,
       void *extraargs    __attribute__((__unused__))
       ) {
  assert(false);
  *dirtyp = 0;
  return 0;
}

static bool pf_req_callback(void* UU(ftnode_pv), void* UU(read_extraargs)) {
  return false;
}

static bool true_pf_req_callback(void* UU(ftnode_pv), void* UU(read_extraargs)) {
    if (pf_req_called) return false;
    return true;
}

static int err_pf_callback(void* UU(ftnode_pv), void* UU(dd), void* UU(read_extraargs), int UU(fd), PAIR_ATTR* UU(sizep)) {
  assert(false);
  return 0; // gcov
}

static int pf_callback(void* UU(ftnode_pv), void* UU(dd), void* UU(read_extraargs), int UU(fd), PAIR_ATTR* UU(sizep)) {
  assert(false);
  return 0; // gcov
}

static int true_pf_callback(void* UU(ftnode_pv), void* UU(dd), void* read_extraargs, int UU(fd), PAIR_ATTR* sizep) {
    pf_req_called = true;
    *sizep = make_pair_attr(sizeof(fetch_val)+1);
    assert(read_extraargs == &fetch_val);
    return 0;
}


static void
cachetable_test (void) {
    const int test_limit = 12;
    int r;
    CACHETABLE ct;
    bool doing_prefetch = false;
    toku_cachetable_create(&ct, test_limit, ZERO_LSN, nullptr);
    const char *fname1 = TOKU_TEST_FILENAME;
    unlink(fname1);
    CACHEFILE f1;
    r = toku_cachetable_openf(&f1, ct, fname1, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); assert(r == 0);
    
    void* v1;
    //void* v2;
    long s1;
    //long s2;
    CACHETABLE_WRITE_CALLBACK wc = def_write_callback(NULL);
    r = toku_cachetable_get_and_pin(f1, make_blocknum(1), 1, &v1, &s1, wc, fetch, pf_req_callback, pf_callback, true, NULL);
    assert(&fetch_val == v1);
    //
    // verify that a prefetch of this node will fail
    //
    r = toku_cachefile_prefetch(
        f1,
        make_blocknum(1),
        1,
        wc,
        fetch,
        pf_req_callback,
        pf_callback,
        NULL,
        &doing_prefetch
        );
    assert(r == 0);
    // make sure that prefetch should not happen, because we have already pinned node
    assert(!doing_prefetch);
    r = toku_test_cachetable_unpin(f1, make_blocknum(1), 1, CACHETABLE_CLEAN, make_pair_attr(8));
    //
    // now get and pin node again, and make sure that partial fetch and fetch are not called
    //
    r = toku_cachetable_get_and_pin(f1, make_blocknum(1), 1, &v1, &s1, wc, err_fetch, pf_req_callback, err_pf_callback, true, NULL);
    r = toku_test_cachetable_unpin(f1, make_blocknum(1), 1, CACHETABLE_CLEAN, make_pair_attr(8));
    //
    // now make sure that if we say a partial fetch is required, that we get a partial fetch
    // and that read_extraargs properly passed down
    //
    pf_req_called = false;
    r = toku_cachetable_get_and_pin(f1, make_blocknum(1), 1, &v1, &s1, wc, err_fetch, true_pf_req_callback, true_pf_callback, true, &fetch_val);
    assert(pf_req_called);
    assert(s1 == sizeof(fetch_val)+1);
    r = toku_test_cachetable_unpin(f1, make_blocknum(1), 1, CACHETABLE_CLEAN, make_pair_attr(8));

    // close and reopen cachefile so we can do some simple prefetch tests
    toku_cachefile_close(&f1, false, ZERO_LSN);
    toku_cachetable_close(&ct);
    toku_cachetable_create(&ct, test_limit, ZERO_LSN, nullptr);
    r = toku_cachetable_openf(&f1, ct, fname1, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); assert(r == 0);
    //
    // verify that a prefetch of the node will succeed
    //
    r = toku_cachefile_prefetch(
        f1,
        make_blocknum(1),
        1,
        wc,
        fetch,
        pf_req_callback,
        pf_callback,
        NULL,
        &doing_prefetch
        );
    assert(r == 0);
    // make sure that prefetch should happen, because we have already pinned node
    assert(doing_prefetch);
    //
    // now verify we can pin it, and NO fetch callback should get called
    //
    r = toku_cachetable_get_and_pin(f1, make_blocknum(1), 1, &v1, &s1, wc, err_fetch, pf_req_callback, err_pf_callback, true, NULL);
    assert(&fetch_val == v1);
    r = toku_test_cachetable_unpin(f1, make_blocknum(1), 1, CACHETABLE_CLEAN, make_pair_attr(8));

    //
    // now verify a prefetch that requires a partial fetch works, and that we can then pin the node
    //
    pf_req_called = false;
    r = toku_cachefile_prefetch(
        f1,
        make_blocknum(1),
        1,
        wc,
        fetch,
        true_pf_req_callback,
        true_pf_callback,
        &fetch_val,
        &doing_prefetch
        );
    assert(doing_prefetch);
    r = toku_cachetable_get_and_pin(f1, make_blocknum(1), 1, &v1, &s1, wc, err_fetch, pf_req_callback, err_pf_callback, true, NULL);
    assert(&fetch_val == v1);
    r = toku_test_cachetable_unpin(f1, make_blocknum(1), 1, CACHETABLE_CLEAN, make_pair_attr(8));
    
    
    toku_cachetable_verify(ct);
    toku_cachefile_close(&f1, false, ZERO_LSN);
    toku_cachetable_close(&ct);
    
    
}

int
test_main(int argc, const char *argv[]) {
    default_parse_args(argc, argv);
    cachetable_test();
    return 0;
}
