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

static void
test_cachetable_def_flush (int n) {
    CACHETABLE_WRITE_CALLBACK wc = def_write_callback(NULL);
    const int test_limit = 2*n;
    int r;
    CACHETABLE ct;
    toku_cachetable_create(&ct, test_limit, ZERO_LSN, nullptr);
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    r = toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU);
    assert_zero(r);
    char fname1[TOKU_PATH_MAX+1];
    unlink(toku_path_join(fname1, 2, TOKU_TEST_FILENAME, "test1.dat"));
    CACHEFILE f1;
    r = toku_cachetable_openf(&f1, ct, fname1, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); assert(r == 0);

    char fname2[TOKU_PATH_MAX+1];
    unlink(toku_path_join(fname2, 2, TOKU_TEST_FILENAME, "test2.dat"));
    CACHEFILE f2;
    r = toku_cachetable_openf(&f2, ct, fname2, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); assert(r == 0);

    // insert keys 0..n-1 
    int i;
    for (i=0; i<n; i++) {
        uint32_t hi;
        hi = toku_cachetable_hash(f1, make_blocknum(i));
        toku_cachetable_put(f1, make_blocknum(i), hi, (void *)(long)i, make_pair_attr(1), wc, put_callback_nop);
        r = toku_test_cachetable_unpin(f1, make_blocknum(i), hi, CACHETABLE_CLEAN, make_pair_attr(1));
        assert(r == 0);
        hi = toku_cachetable_hash(f2, make_blocknum(i));
        toku_cachetable_put(f2, make_blocknum(i), hi, (void *)(long)i, make_pair_attr(1), wc, put_callback_nop);
        r = toku_test_cachetable_unpin(f2, make_blocknum(i), hi, CACHETABLE_CLEAN, make_pair_attr(1));
        assert(r == 0);
    }
    toku_cachetable_verify(ct);

    // verify keys exists
    for (i=0; i<n; i++) {
        uint32_t hi;
        void *v;
        hi = toku_cachetable_hash(f1, make_blocknum(i));
        r = toku_cachetable_maybe_get_and_pin(f1, make_blocknum(i), hi, PL_WRITE_EXPENSIVE, &v);
        assert(r == 0 && v == (void *)(long)i);
        r = toku_test_cachetable_unpin(f1, make_blocknum(i), hi, CACHETABLE_CLEAN, make_pair_attr(1));
        assert(r == 0);
        hi = toku_cachetable_hash(f2, make_blocknum(i));
        r = toku_cachetable_maybe_get_and_pin(f2, make_blocknum(i), hi, PL_WRITE_EXPENSIVE, &v);
        assert(r == 0 && v == (void *)(long)i);
        r = toku_test_cachetable_unpin(f2, make_blocknum(i), hi, CACHETABLE_CLEAN, make_pair_attr(1));
        assert(r == 0);
    }

    // def_flush 
    toku_cachefile_close(&f1, false, ZERO_LSN);
    toku_cachefile_verify(f2);

    // verify keys exist in f2
    for (i=0; i<n; i++) {
        uint32_t hi;
        void *v;
        hi = toku_cachetable_hash(f2, make_blocknum(i));
        r = toku_cachetable_maybe_get_and_pin(f2, make_blocknum(i), hi, PL_WRITE_EXPENSIVE, &v);
        assert(r == 0);
        r = toku_test_cachetable_unpin(f2, make_blocknum(i), hi, CACHETABLE_CLEAN, make_pair_attr(1));
        assert(r == 0);
    }

    toku_cachefile_close(&f2, false, ZERO_LSN);
    toku_cachetable_close(&ct);
}

int
test_main(int argc, const char *argv[]) {
    default_parse_args(argc, argv);
    test_cachetable_def_flush(8);
    return 0;
}
