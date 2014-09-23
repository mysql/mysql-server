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

#include "locktree_unit_test.h"

namespace toku {

// test that the same txn can relock ranges it already owns
// ensure that existing read locks can be upgrading to
// write locks if overlapping and ensure that existing read
// or write locks are consolidated by overlapping relocks.
void locktree_unit_test::test_overlapping_relock(void) {
    locktree lt;
    
    DICTIONARY_ID dict_id = { 1 };
    lt.create(nullptr, dict_id, dbt_comparator);

    const DBT *zero = get_dbt(0);
    const DBT *one = get_dbt(1);
    const DBT *two = get_dbt(2);
    const DBT *three = get_dbt(3);
    const DBT *four = get_dbt(4);
    const DBT *five = get_dbt(5);

    int r;
    TXNID txnid_a = 1001;

    // because of the single txnid optimization, there is no consolidation
    // of read or write ranges until there is at least two txnids in
    // the locktree. so here we add some arbitrary txnid to get a point
    // lock [100, 100] so that the test below can expect to actually 
    // do something. at the end of the test, we release 100, 100.
    const TXNID the_other_txnid = 9999;
    const DBT *hundred = get_dbt(100);
    r = lt.acquire_write_lock(the_other_txnid, hundred, hundred, nullptr, false);
    invariant(r == 0);

    for (int test_run = 0; test_run < 2; test_run++) {
        // test_run == 0 means test with read lock
        // test_run == 1 means test with write lock
#define ACQUIRE_LOCK(txn, left, right, conflicts) \
        test_run == 0 ? lt.acquire_read_lock(txn, left, right, conflicts, false) \
            : lt.acquire_write_lock(txn, left, right, conflicts, false)

        // lock [1,1] and [2,2]. then lock [1,2].
        // ensure only [1,2] exists in the tree
        r = ACQUIRE_LOCK(txnid_a, one, one, nullptr);
        invariant(r == 0);
        r = ACQUIRE_LOCK(txnid_a, two, two, nullptr);
        invariant(r == 0);
        r = ACQUIRE_LOCK(txnid_a, one, two, nullptr);
        invariant(r == 0);

        struct verify_fn_obj {
            bool saw_the_other;
            TXNID expected_txnid;
            keyrange *expected_range;
            const comparator *cmp;
            bool fn(const keyrange &range, TXNID txnid) {
                if (txnid == the_other_txnid) {
                    invariant(!saw_the_other);
                    saw_the_other = true;
                    return true;
                }
                invariant(txnid == expected_txnid);
                keyrange::comparison c = range.compare(*cmp, *expected_range);
                invariant(c == keyrange::comparison::EQUALS);
                return true;
            }
        } verify_fn;
        verify_fn.cmp = &lt.m_cmp;

#define do_verify() \
        do { verify_fn.saw_the_other = false; locktree_iterate<verify_fn_obj>(&lt, &verify_fn); } while (0)

        keyrange range;
        range.create(one, two);
        verify_fn.expected_txnid = txnid_a;
        verify_fn.expected_range = &range;
        do_verify();

        // unlocking [1,1] should remove the only range,
        // the other unlocks shoudl do nothing.
        lt.remove_overlapping_locks_for_txnid(txnid_a, one, one);
        lt.remove_overlapping_locks_for_txnid(txnid_a, two, two);
        lt.remove_overlapping_locks_for_txnid(txnid_a, one, two);

        // try overlapping from the right
        r = ACQUIRE_LOCK(txnid_a, one, three, nullptr);
        r = ACQUIRE_LOCK(txnid_a, two, five, nullptr);
        verify_fn.expected_txnid = txnid_a;
        range.create(one, five);
        verify_fn.expected_range = &range;
        do_verify();

        // now overlap from the left
        r = ACQUIRE_LOCK(txnid_a, zero, four, nullptr);
        verify_fn.expected_txnid = txnid_a;
        range.create(zero, five);
        verify_fn.expected_range = &range;
        do_verify();

        // now relock in a range that is already dominated
        r = ACQUIRE_LOCK(txnid_a, five, five, nullptr);
        verify_fn.expected_txnid = txnid_a;
        range.create(zero, five);
        verify_fn.expected_range = &range;
        do_verify();

        // release one of the locks we acquired. this should clean up the whole range.
        lt.remove_overlapping_locks_for_txnid(txnid_a, zero, four);

#undef ACQUIRE_LOCK
    }

    // remove the other txnid's lock now
    lt.remove_overlapping_locks_for_txnid(the_other_txnid, hundred, hundred);

    lt.release_reference();
    lt.destroy();
}

} /* namespace toku */

int main(void) {
    toku::locktree_unit_test test;
    test.test_overlapping_relock();
    return 0;
}
