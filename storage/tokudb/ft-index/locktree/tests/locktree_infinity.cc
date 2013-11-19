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
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "locktree_unit_test.h"

namespace toku {

// test that ranges with infinite endpoints work
void locktree_unit_test::test_infinity(void) {
    locktree::manager mgr;
    mgr.create(nullptr, nullptr, nullptr, nullptr);
    DESCRIPTOR desc = nullptr;
    DICTIONARY_ID dict_id = { 1 };
    locktree *lt = mgr.get_lt(dict_id, desc, compare_dbts, nullptr);

    int r;
    TXNID txnid_a = 1001;
    TXNID txnid_b = 2001;
    const DBT *zero = get_dbt(0);
    const DBT *one = get_dbt(1);
    const DBT *two = get_dbt(2);
    const DBT *five = get_dbt(5);
    const DBT min_int = min_dbt();
    const DBT max_int = max_dbt();

    // txn A will lock -inf, 5.
    r = lt->acquire_write_lock(txnid_a, toku_dbt_negative_infinity(), five, nullptr);
    invariant(r == 0);
    // txn B will fail to get any lock <= 5, even min_int
    r = lt->acquire_write_lock(txnid_b, five, five, nullptr);
    invariant(r == DB_LOCK_NOTGRANTED);
    r = lt->acquire_write_lock(txnid_b, zero, one, nullptr);
    invariant(r == DB_LOCK_NOTGRANTED);
    r = lt->acquire_write_lock(txnid_b, &min_int, &min_int, nullptr);
    invariant(r == DB_LOCK_NOTGRANTED);
    r = lt->acquire_write_lock(txnid_b, toku_dbt_negative_infinity(), &min_int, nullptr);
    invariant(r == DB_LOCK_NOTGRANTED);

    lt->remove_overlapping_locks_for_txnid(txnid_a, toku_dbt_negative_infinity(), five);

    // txn A will lock 1, +inf
    r = lt->acquire_write_lock(txnid_a, one, toku_dbt_positive_infinity(), nullptr);
    invariant(r == 0);
    // txn B will fail to get any lock >= 1, even max_int
    r = lt->acquire_write_lock(txnid_b, one, one, nullptr);
    invariant(r == DB_LOCK_NOTGRANTED);
    r = lt->acquire_write_lock(txnid_b, two, five, nullptr);
    invariant(r == DB_LOCK_NOTGRANTED);
    r = lt->acquire_write_lock(txnid_b, &max_int, &max_int, nullptr);
    invariant(r == DB_LOCK_NOTGRANTED);
    r = lt->acquire_write_lock(txnid_b, &max_int, toku_dbt_positive_infinity(), nullptr);
    invariant(r == DB_LOCK_NOTGRANTED);

    lt->remove_overlapping_locks_for_txnid(txnid_a, toku_dbt_negative_infinity(), five);

    // txn A will lock -inf, +inf
    r = lt->acquire_write_lock(txnid_a, toku_dbt_negative_infinity(), toku_dbt_positive_infinity(), nullptr);
    invariant(r == 0);
    // txn B will fail to get any lock
    r = lt->acquire_write_lock(txnid_b, zero, one, nullptr);
    invariant(r == DB_LOCK_NOTGRANTED);
    r = lt->acquire_write_lock(txnid_b, two, five, nullptr);
    invariant(r == DB_LOCK_NOTGRANTED);
    r = lt->acquire_write_lock(txnid_b, &min_int, &min_int, nullptr);
    invariant(r == DB_LOCK_NOTGRANTED);
    r = lt->acquire_write_lock(txnid_b, &min_int, &max_int, nullptr);
    invariant(r == DB_LOCK_NOTGRANTED);
    r = lt->acquire_write_lock(txnid_b, &max_int, &max_int, nullptr);
    invariant(r == DB_LOCK_NOTGRANTED);
    r = lt->acquire_write_lock(txnid_b, toku_dbt_negative_infinity(), toku_dbt_negative_infinity(), nullptr);
    invariant(r == DB_LOCK_NOTGRANTED);
    r = lt->acquire_write_lock(txnid_b, toku_dbt_negative_infinity(), toku_dbt_positive_infinity(), nullptr);
    invariant(r == DB_LOCK_NOTGRANTED);
    r = lt->acquire_write_lock(txnid_b, toku_dbt_positive_infinity(), toku_dbt_positive_infinity(), nullptr);
    invariant(r == DB_LOCK_NOTGRANTED);

    lt->remove_overlapping_locks_for_txnid(txnid_a, toku_dbt_negative_infinity(), toku_dbt_positive_infinity());

    mgr.release_lt(lt);
    mgr.destroy();
}

} /* namespace toku */

int main(void) {
    toku::locktree_unit_test test;
    test.test_infinity();
    return 0;
}
