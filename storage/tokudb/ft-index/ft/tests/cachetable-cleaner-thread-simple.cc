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
// This test verifies that the cleaner thread doesn't call the callback if
// nothing needs flushing.
//

CACHEFILE f1;
bool my_cleaner_callback_called;

static int
my_cleaner_callback(
    void* UU(ftnode_pv),
    BLOCKNUM UU(blocknum),
    uint32_t UU(fullhash),
    void* UU(extraargs)
    )
{
    assert(blocknum.b == 100);  // everything is pinned so this should never be called
    assert(fullhash == 100);
    PAIR_ATTR attr = make_pair_attr(8);
    attr.cache_pressure_size = 100;
    int r = toku_test_cachetable_unpin(f1, make_blocknum(100), 100, CACHETABLE_CLEAN, attr);
    my_cleaner_callback_called = true;
    return r;
}

static void
run_test (void) {
    const int test_limit = 1000;
    int r;
    CACHETABLE ct;
    toku_cachetable_create(&ct, test_limit, ZERO_LSN, nullptr);
    toku_set_cleaner_period(ct, 1);
    my_cleaner_callback_called = false;

    const char *fname1 = TOKU_TEST_FILENAME;
    unlink(fname1);
    r = toku_cachetable_openf(&f1, ct, fname1, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); assert(r == 0);
    
    void* vs[5];
    //void* v2;
    long ss[5];
    //long s2;
    CACHETABLE_WRITE_CALLBACK wc = def_write_callback(NULL);
    wc.cleaner_callback = my_cleaner_callback;
    r = toku_cachetable_get_and_pin(f1, make_blocknum(100), 100, &vs[4], &ss[4],
                                    wc,
                                    def_fetch,
                                    def_pf_req_callback,
                                    def_pf_callback,
                                    true, 
                                    NULL);
    PAIR_ATTR attr = make_pair_attr(8);
    attr.cache_pressure_size = 100;
    r = toku_test_cachetable_unpin(f1, make_blocknum(100), 100, CACHETABLE_CLEAN, attr);

    for (int i = 0; i < 4; ++i) {
        r = toku_cachetable_get_and_pin(f1, make_blocknum(i+1), i+1, &vs[i], &ss[i],
                                        wc,
                                        def_fetch,
                                        def_pf_req_callback,
                                        def_pf_callback,
                                        true, 
                                        NULL);
        assert_zero(r);
        // set cachepressure_size to 0
        attr = make_pair_attr(8);
        attr.cache_pressure_size = 0;
        r = toku_test_cachetable_unpin(f1, make_blocknum(i+1), i+1, CACHETABLE_CLEAN, attr);
        assert_zero(r);
    }

    usleep(4000000);
    assert(my_cleaner_callback_called);

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
