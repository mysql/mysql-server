/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
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

#include <stdlib.h>
#include <ft/comparator.h>

static int MAGIC = 49;
static DBT dbt_a;
static DBT dbt_b;
static DESCRIPTOR expected_desc;

static int magic_compare(DB *db, const DBT *a, const DBT *b) {
    invariant(db && a && b);
    invariant(db->cmp_descriptor == expected_desc);
    invariant(a == &dbt_a);
    invariant(b == &dbt_b);
    return MAGIC;
}

static void test_desc(void) {
    int c;
    toku::comparator cmp;
    DESCRIPTOR_S d1, d2;

    // create with d1, make sure it gets used
    cmp.create(magic_compare, &d1);
    expected_desc = &d1;
    c = cmp(&dbt_a, &dbt_b);
    invariant(c == MAGIC);

    // set desc to d2, make sure it gets used
    toku::comparator cmp2;
    cmp2.create(magic_compare, &d2);
    cmp.inherit(cmp2);
    expected_desc = &d2;
    c = cmp(&dbt_a, &dbt_b);
    invariant(c == MAGIC);
    cmp2.destroy();

    // go back to using d1, but using the create_from API
    toku::comparator cmp3, cmp4;
    cmp3.create(magic_compare, &d1); // cmp3 has d1
    cmp4.create_from(cmp3); // cmp4 should get d1 from cmp3
    expected_desc = &d1;
    c = cmp3(&dbt_a, &dbt_b);
    invariant(c == MAGIC);
    c = cmp4(&dbt_a, &dbt_b);
    invariant(c == MAGIC);
    cmp3.destroy();
    cmp4.destroy();

    cmp.destroy();
}

static int dont_compare_me_bro(DB *db, const DBT *a, const DBT *b) {
    abort();
    return db && a && b;
}

static void test_infinity(void) {
    int c;
    toku::comparator cmp;
    cmp.create(dont_compare_me_bro, nullptr);

    // make sure infinity-valued end points compare as expected
    // to an arbitrary (uninitialized!) dbt. the comparison function
    // should never be called and thus the dbt never actually read.
    DBT arbitrary_dbt;

    c = cmp(&arbitrary_dbt, toku_dbt_positive_infinity());
    invariant(c < 0);
    c = cmp(toku_dbt_negative_infinity(), &arbitrary_dbt);
    invariant(c < 0);

    c = cmp(toku_dbt_positive_infinity(), &arbitrary_dbt);
    invariant(c > 0);
    c = cmp(&arbitrary_dbt, toku_dbt_negative_infinity());
    invariant(c > 0);

    c = cmp(toku_dbt_negative_infinity(), toku_dbt_negative_infinity());
    invariant(c == 0);
    c = cmp(toku_dbt_positive_infinity(), toku_dbt_positive_infinity());
    invariant(c == 0);

    cmp.destroy();
}

int main(void) {
    test_desc();
    test_infinity();
    return 0;
}
