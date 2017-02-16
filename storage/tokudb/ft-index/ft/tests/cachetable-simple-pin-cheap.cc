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

bool pf_called;
static bool true_pf_req_callback(void* UU(ftnode_pv), void* UU(read_extraargs)) {
  if (pf_called) return false;
  return true;
}

static int true_pf_callback(void* UU(ftnode_pv), void* UU(dd), void* UU(read_extraargs), int UU(fd), PAIR_ATTR* sizep) {
    *sizep = make_pair_attr(9);
    pf_called = true;
    return 0;
}

static void kibbutz_work(void *fe_v)
{
    CACHEFILE CAST_FROM_VOIDP(f1, fe_v);
    sleep(2);
    int r = toku_test_cachetable_unpin(f1, make_blocknum(1), 1, CACHETABLE_CLEAN, make_pair_attr(8));
    assert(r==0);
    remove_background_job_from_cf(f1);    
}

static void
unlock_dummy (void* UU(v)) {
}

static void reset_unlockers(UNLOCKERS unlockers) {
    unlockers->locked = true;
}

static void
run_test (pair_lock_type lock_type) {
    const int test_limit = 12;
    struct unlockers unlockers = {true, unlock_dummy, NULL, NULL};
    int r;
    CACHETABLE ct;
    toku_cachetable_create(&ct, test_limit, ZERO_LSN, nullptr);
    const char *fname1 = TOKU_TEST_FILENAME;
    unlink(fname1);
    CACHEFILE f1;
    r = toku_cachetable_openf(&f1, ct, fname1, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); assert(r == 0);
    
    void* v1;
    long s1;
    CACHETABLE_WRITE_CALLBACK wc = def_write_callback(NULL);
    r = toku_cachetable_get_and_pin_with_dep_pairs(f1, make_blocknum(1), 1, &v1, &s1, wc, def_fetch, def_pf_req_callback, def_pf_callback, lock_type, NULL, 0, NULL, NULL);
    cachefile_kibbutz_enq(f1, kibbutz_work, f1);
    reset_unlockers(&unlockers);
    r = toku_cachetable_get_and_pin_nonblocking(f1, make_blocknum(1), 1, &v1, &s1, wc, def_fetch, def_pf_req_callback, def_pf_callback, PL_WRITE_EXPENSIVE, NULL, &unlockers);
    // to fix #5393, we changed behavior on full fetch where if we 
    // requested a PL_WRITE_CHEAP, and had to grab a PL_WRITE_EXPENSIVE for
    // a full fetch, we keep it as a PL_WRITE_EXPENSIVE because downgrading back
    // was too big a pain.
    if (lock_type == PL_WRITE_EXPENSIVE || lock_type == PL_WRITE_CHEAP) {
        assert(r == TOKUDB_TRY_AGAIN); assert(!unlockers.locked);
    }
    else {
        assert(r == 0); assert(unlockers.locked);        
        r = toku_test_cachetable_unpin(f1, make_blocknum(1), 1, CACHETABLE_CLEAN, make_pair_attr(8)); assert(r==0);
    }

    // now do the same test with a partial fetch required
    pf_called = false;
    r = toku_cachetable_get_and_pin_with_dep_pairs(f1, make_blocknum(1), 1, &v1, &s1, wc, def_fetch, true_pf_req_callback, true_pf_callback, lock_type, NULL, 0, NULL, NULL);
    assert(pf_called);
    cachefile_kibbutz_enq(f1, kibbutz_work, f1);
    reset_unlockers(&unlockers);
    r = toku_cachetable_get_and_pin_nonblocking(f1, make_blocknum(1), 1, &v1, &s1, wc, def_fetch, def_pf_req_callback, def_pf_callback, PL_WRITE_EXPENSIVE, NULL, &unlockers);
    if (lock_type == PL_WRITE_EXPENSIVE) {
        assert(r == TOKUDB_TRY_AGAIN); assert(!unlockers.locked);
    }
    else {
        assert(r == 0); assert(unlockers.locked);        
        r = toku_test_cachetable_unpin(f1, make_blocknum(1), 1, CACHETABLE_CLEAN, make_pair_attr(8)); assert(r==0);
    }
    
    toku_cachetable_verify(ct);
    toku_cachefile_close(&f1, false, ZERO_LSN); 
    toku_cachetable_close(&ct);
}

int
test_main(int argc, const char *argv[]) {
  default_parse_args(argc, argv);
  run_test(PL_READ);
  run_test(PL_WRITE_CHEAP);
  run_test(PL_WRITE_EXPENSIVE);
  return 0;
}
