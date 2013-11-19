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

#include <toku_time.h>

__attribute__((__unused__))
static long current_time_usec(void) {
    struct timeval t;
    gettimeofday(&t, NULL);
    return t.tv_usec + t.tv_sec * 1000000;
}

namespace toku {

// test write lock conflicts when read or write locks exist
// test read lock conflicts when write locks exist
void locktree_unit_test::test_conflicts(void) {
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
    const DBT *three = get_dbt(3);
    const DBT *four = get_dbt(4);
    const DBT *five = get_dbt(5);

    for (int test_run = 0; test_run < 2; test_run++) {
        // test_run == 0 means test with read lock
        // test_run == 1 means test with write lock
#define ACQUIRE_LOCK(txn, left, right, conflicts) \
        test_run == 0 ? lt->acquire_read_lock(txn, left, right, conflicts) \
                      : lt->acquire_write_lock(txn, left, right, conflicts)

        // acquire some locks for txnid_a
        r = ACQUIRE_LOCK(txnid_a, one, one, nullptr);
        invariant(r == 0);
        r = ACQUIRE_LOCK(txnid_a, three, four, nullptr);
        invariant(r == 0);

#undef ACQUIRE_LOCK

        for (int sub_test_run = 0; sub_test_run < 2; sub_test_run++) {
        // sub_test_run == 0 means test read lock on top of write lock
        // sub_test_run == 1 means test write lock on top of write lock
        // if test_run == 0, then read locks exist. only test write locks.
#define ACQUIRE_LOCK(txn, left, right, conflicts) \
        sub_test_run == 0 && test_run == 1 ? \
            lt->acquire_read_lock(txn, left, right, conflicts) \
          : lt->acquire_write_lock(txn, left, right, conflicts)
            // try to get point write locks for txnid_b, should fail
            r = ACQUIRE_LOCK(txnid_b, one, one, nullptr);
            invariant(r == DB_LOCK_NOTGRANTED);
            r = ACQUIRE_LOCK(txnid_b, three, three, nullptr);
            invariant(r == DB_LOCK_NOTGRANTED);
            r = ACQUIRE_LOCK(txnid_b, four, four, nullptr);
            invariant(r == DB_LOCK_NOTGRANTED);

            // try to get some overlapping range write locks for txnid_b, should fail
            r = ACQUIRE_LOCK(txnid_b, zero, two, nullptr);
            invariant(r == DB_LOCK_NOTGRANTED);
            r = ACQUIRE_LOCK(txnid_b, four, five, nullptr);
            invariant(r == DB_LOCK_NOTGRANTED);
            r = ACQUIRE_LOCK(txnid_b, two, three, nullptr);
            invariant(r == DB_LOCK_NOTGRANTED);
#undef ACQUIRE_LOCK
        }

        lt->remove_overlapping_locks_for_txnid(txnid_a, one, one);
        lt->remove_overlapping_locks_for_txnid(txnid_a, three, four);
        invariant(no_row_locks(lt));
    }

    mgr.release_lt(lt);
    mgr.destroy();
}

} /* namespace toku */

int main(void) {
    toku::locktree_unit_test test;
    test.test_conflicts();
    return 0;
}
