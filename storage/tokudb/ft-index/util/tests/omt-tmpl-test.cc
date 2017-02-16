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

#include <type_traits>
#include <memory.h>
#include <toku_portability.h>
#include <toku_assert.h>
#include <toku_time.h>
#include <util/omt.h>

namespace toku {

namespace test {

    inline int intcmp(const int &a, const int &b);
    inline int intcmp(const int &a, const int &b) {
        if (a < b) {
            return -1;
        }
        if (a > b) {
            return +1;
        }
        return 0;
    }

    typedef omt<int> int_omt_t;

    static int intiter_magic = 0xdeadbeef;
    inline int intiter(const int &value __attribute__((__unused__)), const uint32_t idx __attribute__((__unused__)), int *const extra);
    inline int intiter(const int &value __attribute__((__unused__)), const uint32_t idx __attribute__((__unused__)), int *const extra) {
        invariant(*extra == intiter_magic);
        return 0;
    }

    struct intiter2extra {
        int count;
        int last;
    };
    inline int intiter2(const int &value, const uint32_t idx __attribute__((__unused__)), struct intiter2extra *const extra);
    inline int intiter2(const int &value, const uint32_t idx __attribute__((__unused__)), struct intiter2extra *const extra) {
        extra->count++;
        invariant(extra->last < value);
        extra->last = value;
        return 0;
    }

    static void unittest(void) {
        int_omt_t o;
        int r;
        o.create();
        invariant(o.size() == 0);

        r = o.insert<int, intcmp>(1, 1, nullptr);
        invariant_zero(r);
        r = o.insert<int, intcmp>(3, 3, nullptr);
        invariant_zero(r);

        invariant(o.size() == 2);

        r = o.insert<int, intcmp>(2, 2, nullptr);
        invariant_zero(r);

        invariant(o.size() == 3);

        int x;
        r = o.fetch(1, &x);
        invariant_zero(r);

        invariant(x == 2);

        r = o.iterate<int, intiter>(&intiter_magic);
        invariant_zero(r);

        struct intiter2extra e = {0, 0};
        r = o.iterate_on_range<struct intiter2extra, intiter2>(0, 2, &e);
        invariant_zero(r);
        invariant(e.count == 2);
        invariant(e.last == 2);

        r = o.set_at(5, 1);
        invariant_zero(r);
        r = o.delete_at(1);
        invariant_zero(r);

        invariant(o.size() == 2);

        o.destroy();

        int *XMALLOC_N(4, intarray);
        for (int i = 0; i < 4; ++i) {
            intarray[i] = i + 1;
        }
        int_omt_t left, right;
        left.create_steal_sorted_array(&intarray, 4, 4);
        invariant_null(intarray);
        right.create();
        r = right.insert<int, intcmp>(8, 8, nullptr);
        invariant_zero(r);
        r = right.insert<int, intcmp>(7, 7, nullptr);
        invariant_zero(r);
        r = right.insert<int, intcmp>(6, 6, nullptr);
        invariant_zero(r);
        r = right.insert<int, intcmp>(5, 5, nullptr);
        invariant_zero(r);

        int_omt_t combined;
        combined.merge(&left, &right);
        invariant(combined.size() == 8);
        invariant(left.size() == 0);
        invariant(right.size() == 0);
        struct intiter2extra e2 = {0, 0};
        r = combined.iterate<struct intiter2extra, intiter2>(&e2);
        invariant_zero(r);
        invariant(e2.count == 8);
        invariant(e2.last == 8);

        combined.destroy();
    }

} // end namespace test

} // end namespace toku

int main(void) {
    toku::test::unittest();
    return 0;
}
