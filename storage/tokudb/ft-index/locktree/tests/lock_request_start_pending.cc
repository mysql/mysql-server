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

#include "lock_request_unit_test.h"

namespace toku {

// starting a lock request without immediate success should get
// stored in the lock request set as pending.
void lock_request_unit_test::test_start_pending(void) {
    int r;
    locktree::manager mgr;
    locktree *lt;
    lock_request request;
    // bogus, just has to be something.
    const uint64_t lock_wait_time = 0;

    mgr.create(nullptr, nullptr, nullptr, nullptr);
    DICTIONARY_ID dict_id = { 1 };
    lt = mgr.get_lt(dict_id, nullptr, compare_dbts, nullptr);

    TXNID txnid_a = 1001;
    TXNID txnid_b = 2001;

    const DBT *zero = get_dbt(0);
    const DBT *one = get_dbt(1);
    const DBT *two = get_dbt(2);

    // take a range lock using txnid b
    r = lt->acquire_write_lock(txnid_b, zero, two, nullptr);
    invariant_zero(r);

    locktree::lt_lock_request_info *info = lt->get_lock_request_info();

    // start a lock request for 1,1
    // it should fail. the request should be stored and in the pending state.
    request.create(lock_wait_time);
    request.set(lt, txnid_a, one, one, lock_request::type::WRITE);
    r = request.start();
    invariant(r == DB_LOCK_NOTGRANTED);
    invariant(info->pending_lock_requests.size() == 1);
    invariant(request.m_state == lock_request::state::PENDING);

    // should have made copies of the keys, and they should be equal
    invariant(request.m_left_key_copy.flags == DB_DBT_MALLOC);
    invariant(request.m_right_key_copy.flags == DB_DBT_MALLOC);
    invariant(compare_dbts(nullptr, &request.m_left_key_copy, one) == 0);
    invariant(compare_dbts(nullptr, &request.m_right_key_copy, one) == 0);

    // release the range lock for txnid b
    locktree_unit_test::locktree_test_release_lock(lt, txnid_b, zero, two);

    // now retry the lock requests.
    // it should transition the request to successfully complete.
    lock_request::retry_all_lock_requests(lt);
    invariant(info->pending_lock_requests.size() == 0);
    invariant(request.m_state == lock_request::state::COMPLETE);
    invariant(request.m_complete_r == 0);

    locktree_unit_test::locktree_test_release_lock(lt, txnid_a, one, one);

    request.destroy();
    mgr.release_lt(lt);
    mgr.destroy();
}

} /* namespace toku */

int main(void) {
    toku::lock_request_unit_test test;
    test.test_start_pending();
    return 0;
}

