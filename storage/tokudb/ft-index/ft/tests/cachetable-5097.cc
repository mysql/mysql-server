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

CACHEFILE f1;
CACHEFILE f2;

bool check_flush;
bool dirty_flush_called;
bool check_pe_callback;
bool pe_callback_called;

CACHETABLE ct;

static int 
pe_callback (
    void *ftnode_pv __attribute__((__unused__)), 
    PAIR_ATTR bytes_to_free __attribute__((__unused__)), 
    PAIR_ATTR* bytes_freed, 
    void* extraargs __attribute__((__unused__))
    ) 
{
    *bytes_freed = make_pair_attr(1);
    if (check_pe_callback) {
        pe_callback_called = true;
    }
    usleep(4*1024*1024);
    return 0;
}

static void
flush (CACHEFILE f __attribute__((__unused__)),
       int UU(fd),
       CACHEKEY k  __attribute__((__unused__)),
       void *v     __attribute__((__unused__)),
       void **dd     __attribute__((__unused__)),
       void *e     __attribute__((__unused__)),
       PAIR_ATTR s      __attribute__((__unused__)),
       PAIR_ATTR* new_size      __attribute__((__unused__)),
       bool w      __attribute__((__unused__)),
       bool keep   __attribute__((__unused__)),
       bool c      __attribute__((__unused__)),
       bool UU(is_clone)
       ) {
    if (check_flush && w) {
        dirty_flush_called = true;
    }
}

static void *f2_pin(void *arg) {
    int r;    
    void* v1;
    long s1;
    CACHETABLE_WRITE_CALLBACK wc = def_write_callback(NULL);
    //
    // these booleans for pe_callback just ensure that the
    // test is working as we expect it to. We expect the get_and_pin to 
    // cause a partial eviction of f1's PAIR, reducing its size from 8 to 1
    // and we expect that to be enough so that the unpin does not invoke a partial eviction
    // This is just to ensure that the bug is being exercised
    //
    check_pe_callback = true;
    r = toku_cachetable_get_and_pin(f2, make_blocknum(1), 1, &v1, &s1, wc, def_fetch, def_pf_req_callback, def_pf_callback, true, NULL);
    assert(r == 0);
    ct->ev.signal_eviction_thread();
    usleep(1*1024*1024);
    assert(pe_callback_called);
    pe_callback_called = false;
    r = toku_test_cachetable_unpin(f2, make_blocknum(1), 1, CACHETABLE_CLEAN, make_pair_attr(8));
    check_pe_callback = false;
    assert(!pe_callback_called);
    assert(r == 0);
    
    return arg;
}

static void
cachetable_test (void) {
    const int test_limit = 12;
    int r;
    check_flush = false;
    dirty_flush_called = false;
    
    toku_cachetable_create(&ct, test_limit, ZERO_LSN, NULL_LOGGER);
    evictor_test_helpers::disable_ev_thread(&ct->ev); // disable eviction thread
    
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    r = toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU);
    assert_zero(r);
    char fname1[TOKU_PATH_MAX + 1];
    unlink(toku_path_join(fname1, 2, TOKU_TEST_FILENAME, "test1.dat"));
    char fname2[TOKU_PATH_MAX + 1];
    unlink(toku_path_join(fname2, 2, TOKU_TEST_FILENAME, "test2.dat"));

    r = toku_cachetable_openf(&f1, ct, fname1, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); 
    assert(r == 0);
    r = toku_cachetable_openf(&f2, ct, fname2, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); 
    assert(r == 0);

    void* v1;
    long s1;
    CACHETABLE_WRITE_CALLBACK wc = def_write_callback(NULL);
    wc.pe_callback = pe_callback;
    wc.flush_callback = flush;
    // pin and unpin a node 20 times, just to get clock count up
    for (int i = 0; i < 20; i++) {
        r = toku_cachetable_get_and_pin(f1, make_blocknum(1), 1, &v1, &s1, wc, def_fetch, def_pf_req_callback, def_pf_callback, true, NULL);
        assert(r == 0);
        r = toku_test_cachetable_unpin(f1, make_blocknum(1), 1, CACHETABLE_DIRTY, make_pair_attr(8));
        assert(r == 0);
    }

    // at this point, we have a dirty PAIR in the cachetable associated with cachefile f1
    // launch a thread that will put another PAIR in the cachetable, and get partial eviction started
    toku_pthread_t tid;
    r = toku_pthread_create(&tid, NULL, f2_pin, NULL); 
    assert_zero(r);

    usleep(2*1024*1024);
    check_flush = true;
    toku_cachefile_close(&f1, false, ZERO_LSN); 
    assert(dirty_flush_called);
    check_flush = false;

    void *ret;
    r = toku_pthread_join(tid, &ret); 
    assert_zero(r);


    toku_cachetable_verify(ct);
    toku_cachefile_close(&f2, false, ZERO_LSN);
    toku_cachetable_close(&ct);
}

int
test_main(int argc, const char *argv[]) {
  default_parse_args(argc, argv);
  cachetable_test();
  return 0;
}
