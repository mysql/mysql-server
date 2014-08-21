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
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "test.h"

bool flush_may_occur;
long expected_bytes_to_free;

static void
flush (CACHEFILE f __attribute__((__unused__)),
       int UU(fd),
       CACHEKEY k  __attribute__((__unused__)),
       void* UU(v),
       void** UU(dd),
       void *e     __attribute__((__unused__)),
       PAIR_ATTR s      __attribute__((__unused__)),
       PAIR_ATTR* new_size      __attribute__((__unused__)),
       bool w      __attribute__((__unused__)),
       bool keep,
       bool c      __attribute__((__unused__)),
        bool UU(is_clone)
       ) {
    assert(flush_may_occur);
    if (!keep) {
        //int* foo = v;
        //assert(*foo == 3);
        toku_free(v);
    }
}

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
    int* XMALLOC(foo);
    *value = foo;
    *sizep = make_pair_attr(4);
    *foo = 4;
    return 0;
}

static void
other_flush (CACHEFILE f __attribute__((__unused__)),
       int UU(fd),
       CACHEKEY k  __attribute__((__unused__)),
       void *v     __attribute__((__unused__)),
	     void** UU(dd),
       void *e     __attribute__((__unused__)),
       PAIR_ATTR s      __attribute__((__unused__)),
       PAIR_ATTR* new_size      __attribute__((__unused__)),
       bool w      __attribute__((__unused__)),
       bool keep   __attribute__((__unused__)),
       bool c      __attribute__((__unused__)),
        bool UU(is_clone)
       ) {
}

static void 
pe_est_callback(
    void* UU(ftnode_pv), 
    void* UU(dd),
    long* bytes_freed_estimate, 
    enum partial_eviction_cost *cost, 
    void* UU(write_extraargs)
    )
{
    *bytes_freed_estimate = 1000;
    *cost = PE_EXPENSIVE;
}

static int 
pe_callback (
    void *ftnode_pv, 
    PAIR_ATTR UU(bytes_to_free), 
    void* extraargs __attribute__((__unused__)),
    void (*finalize)(PAIR_ATTR bytes_freed, void *extra),
    void *finalize_extra
    ) 
{
    usleep(1*1024*1024);
    if (verbose) printf("calling pe_callback\n");
    expected_bytes_to_free--;
    int* CAST_FROM_VOIDP(foo, ftnode_pv);
    int blah = *foo;
    *foo = blah-1;
    finalize(make_pair_attr(bytes_to_free.size-1), finalize_extra);
    return 0;
}

static int 
other_pe_callback (
    void *ftnode_pv __attribute__((__unused__)), 
    PAIR_ATTR bytes_to_free __attribute__((__unused__)), 
    void* extraargs __attribute__((__unused__)),
    void (*finalize)(PAIR_ATTR bytes_freed, void *extra),
    void *finalize_extra
    ) 
{
    finalize(bytes_to_free, finalize_extra);
    return 0;
}

static void
cachetable_test (void) {
    const int test_limit = 20;
    int r;
    CACHETABLE ct;
    toku_cachetable_create(&ct, test_limit, ZERO_LSN, nullptr);
    evictor_test_helpers::set_hysteresis_limits(&ct->ev, test_limit, 100*test_limit);
    evictor_test_helpers::disable_ev_thread(&ct->ev);
    const char *fname1 = TOKU_TEST_FILENAME;
    unlink(fname1);
    CACHEFILE f1;
    r = toku_cachetable_openf(&f1, ct, fname1, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); assert(r == 0);

    void* v1;
    void* v2;
    long s1, s2;
    flush_may_occur = false;
    for (int i = 0; i < 100000; i++) {
      CACHETABLE_WRITE_CALLBACK wc = def_write_callback(NULL);
      wc.flush_callback = flush;
      wc.pe_est_callback = pe_est_callback;
      wc.pe_callback = pe_callback;
      r = toku_cachetable_get_and_pin(f1, make_blocknum(1), 1, &v1, &s1, wc, fetch, def_pf_req_callback, def_pf_callback, true, NULL);
      r = toku_test_cachetable_unpin(f1, make_blocknum(1), 1, CACHETABLE_CLEAN, make_pair_attr(4));
    }
    for (int i = 0; i < 8; i++) {
      CACHETABLE_WRITE_CALLBACK wc = def_write_callback(NULL);
      wc.flush_callback = flush;
      wc.pe_est_callback = pe_est_callback;
      wc.pe_callback = pe_callback;
      r = toku_cachetable_get_and_pin(f1, make_blocknum(2), 2, &v2, &s2, wc, fetch, def_pf_req_callback, def_pf_callback, true, NULL);
      r = toku_test_cachetable_unpin(f1, make_blocknum(2), 2, CACHETABLE_CLEAN, make_pair_attr(4));
    }
    for (int i = 0; i < 4; i++) {
      CACHETABLE_WRITE_CALLBACK wc = def_write_callback(NULL);
      wc.flush_callback = flush;
      wc.pe_est_callback = pe_est_callback;
      wc.pe_callback = pe_callback;
      r = toku_cachetable_get_and_pin(f1, make_blocknum(3), 3, &v2, &s2, wc, fetch, def_pf_req_callback, def_pf_callback, true, NULL);
      r = toku_test_cachetable_unpin(f1, make_blocknum(3), 3, CACHETABLE_CLEAN, make_pair_attr(4));
    }
    for (int i = 0; i < 2; i++) {
      CACHETABLE_WRITE_CALLBACK wc = def_write_callback(NULL);
      wc.flush_callback = flush;
      wc.pe_est_callback = pe_est_callback;
      wc.pe_callback = pe_callback;
      r = toku_cachetable_get_and_pin(f1, make_blocknum(4), 4, &v2, &s2, wc, fetch, def_pf_req_callback, def_pf_callback, true, NULL);
      r = toku_test_cachetable_unpin(f1, make_blocknum(4), 4, CACHETABLE_CLEAN, make_pair_attr(4));
    }
    flush_may_occur = false;
    expected_bytes_to_free = 4;
    CACHETABLE_WRITE_CALLBACK wc = def_write_callback(NULL);
    wc.flush_callback = other_flush;
    wc.pe_est_callback = pe_est_callback;
    wc.pe_callback = other_pe_callback;
    toku_cachetable_put(f1, make_blocknum(5), 5, NULL, make_pair_attr(4), wc, put_callback_nop);
    flush_may_occur = true;
    r = toku_test_cachetable_unpin(f1, make_blocknum(5), 5, CACHETABLE_CLEAN, make_pair_attr(8));
    ct->ev.signal_eviction_thread();

    // we are testing that having a wildly different estimate than
    // what actually gets freed is ok
    // in the callbacks, we estimate that 1000 bytes gets freed
    // whereas in reality, only 1 byte will be freed
    // we measure that only 1 byte gets freed (which leaves cachetable
    // oversubscrubed)
    usleep(3*1024*1024);
    assert(expected_bytes_to_free == 3);


    toku_cachefile_close(&f1, false, ZERO_LSN);
    toku_cachetable_close(&ct);
}

int
test_main(int argc, const char *argv[]) {
    default_parse_args(argc, argv);
    cachetable_test();
    return 0;
}
