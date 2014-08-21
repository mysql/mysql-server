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

#ident "Copyright (c) 2010-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "test.h"
#include "cachetable/cachetable.h"
#include <inttypes.h>

/* Test for #2755.  The ft_loader is using too much VM. */
static void test_cachetable_reservation (long size) {
    CACHETABLE ct;
    {
        toku_cachetable_create(&ct, size, ZERO_LSN, NULL);
    }
    {
        uint64_t r0 = toku_cachetable_reserve_memory(ct, 0.5, 0);
        uint64_t r0_bound = size/2 + size/16;
        uint64_t r1 = toku_cachetable_reserve_memory(ct, 0.5, 0);
        uint64_t r1_bound = r0_bound/2;
        uint64_t r2 = toku_cachetable_reserve_memory(ct, 0.5, 0);
        uint64_t r2_bound = r1_bound/2;
        if (verbose) printf("%10ld: r0=%10" PRIu64 " r1=%10" PRIu64 " r2=%10" PRIu64 "\n", size, r0, r1, r2);
        assert(r0 <= r0_bound);
        assert(r1 <= r1_bound);
        assert(r2 <= r2_bound);
        assert(r1 <= r0);
        assert(r2 <= r1);

        long unreservable_part = size * 0.25;
        assert(r0 <= (size - unreservable_part)*0.5);
        assert(r1 <= (size - unreservable_part - r0)*0.5);
        assert(r2 <= (size - unreservable_part - r0 -1)*0.5);
        toku_cachetable_release_reserved_memory(ct, r0);
        toku_cachetable_release_reserved_memory(ct, r1);
        toku_cachetable_release_reserved_memory(ct, r2);
    }
    {
        toku_cachetable_close(&ct);
    }
    
}

int test_main (int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    test_cachetable_reservation(1L<<28);
    test_cachetable_reservation(1LL<<33);
    test_cachetable_reservation(3L<<28);
    test_cachetable_reservation((3L<<28) - 107);
    return 0;
}
