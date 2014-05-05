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

// test simple, non-overlapping read locks and then write locks
void locktree_unit_test::test_simple_lock(void) {
    locktree_manager mgr;
    mgr.create(nullptr, nullptr, nullptr, nullptr);

    DICTIONARY_ID dict_id = { 1 };
    locktree *lt = mgr.get_lt(dict_id, nullptr, compare_dbts, nullptr);

    int r;
    TXNID txnid_a = 1001;
    TXNID txnid_b = 2001;
    TXNID txnid_c = 3001;
    TXNID txnid_d = 4001;
    const DBT *one = get_dbt(1);
    const DBT *two = get_dbt(2);
    const DBT *three = get_dbt(3);
    const DBT *four = get_dbt(4);

    for (int test_run = 0; test_run < 2; test_run++) {
        // test_run == 0 means test with read lock
        // test_run == 1 means test with write lock
#define ACQUIRE_LOCK(txn, left, right, conflicts) \
        test_run == 0 ? lt->acquire_read_lock(txn, left, right, conflicts, false) \
            : lt->acquire_write_lock(txn, left, right, conflicts, false)

        // four txns, four points
        r = ACQUIRE_LOCK(txnid_a, one, one, nullptr);
        invariant(r == 0);
        r = ACQUIRE_LOCK(txnid_b, two, two, nullptr);
        invariant(r == 0);
        r = ACQUIRE_LOCK(txnid_c, three, three, nullptr);
        invariant(r == 0);
        r = ACQUIRE_LOCK(txnid_d, four, four, nullptr);
        invariant(r == 0);
        locktree_test_release_lock(lt, txnid_a, one, one);
        locktree_test_release_lock(lt, txnid_b, two, two);
        locktree_test_release_lock(lt, txnid_c, three, three);
        locktree_test_release_lock(lt, txnid_d, four, four);
        invariant(no_row_locks(lt));

        // two txns, two ranges
        r = ACQUIRE_LOCK(txnid_c, one, two, nullptr);
        invariant(r == 0);
        r = ACQUIRE_LOCK(txnid_b, three, four, nullptr);
        invariant(r == 0);
        locktree_test_release_lock(lt, txnid_c, one, two);
        locktree_test_release_lock(lt, txnid_b, three, four);
        invariant(no_row_locks(lt));

        // two txns, one range, one point
        r = ACQUIRE_LOCK(txnid_c, three, four, nullptr);
        invariant(r == 0);
        r = ACQUIRE_LOCK(txnid_d, one, one, nullptr);
        invariant(r == 0);
        locktree_test_release_lock(lt, txnid_c, three, four);
        locktree_test_release_lock(lt, txnid_d, one, one);
        invariant(no_row_locks(lt));

#undef ACQUIRE_LOCK
    }

    const int64_t num_locks = 10000;

    int64_t *keys = (int64_t *) toku_malloc(num_locks * sizeof(int64_t));
    for (int64_t i = 0; i < num_locks; i++) {
        keys[i] = i; 
    }
    for (int64_t i = 0; i < num_locks; i++) {
        int64_t k = rand() % num_locks; 
        int64_t tmp = keys[k];
        keys[k] = keys[i];
        keys[i] = tmp;
    }


    r = mgr.set_max_lock_memory((num_locks + 1) * 500);
    invariant_zero(r);

    DBT k;
    k.ulen = 0;
    k.size = sizeof(keys[0]);
    k.flags = DB_DBT_USERMEM;

    for (int64_t i = 0; i < num_locks; i++) {
        k.data = (void *) &keys[i];
        r = lt->acquire_read_lock(txnid_a, &k, &k, nullptr, false);
        invariant(r == 0);
    }

    for (int64_t i = 0; i < num_locks; i++) {
        k.data = (void *) &keys[i];
        locktree_test_release_lock(lt, txnid_a, &k, &k);
    }

    toku_free(keys);

    mgr.release_lt(lt);
    mgr.destroy();
}

} /* namespace toku */

int main(void) {
    toku::locktree_unit_test test;
    test.test_simple_lock();
    return 0;
}
