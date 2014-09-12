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

// make sure deadlocks are detected when a lock request starts
void lock_request_unit_test::test_start_deadlock(void) {
    int r;
    locktree::manager mgr;
    locktree *lt;
    // something short
    const uint64_t lock_wait_time = 10;

    mgr.create(nullptr, nullptr, nullptr, nullptr);
    DICTIONARY_ID dict_id = { 1 };
    lt = mgr.get_lt(dict_id, nullptr, compare_dbts, nullptr);

    TXNID txnid_a = 1001;
    TXNID txnid_b = 2001;
    TXNID txnid_c = 3001;
    lock_request request_a;
    lock_request request_b;
    lock_request request_c;
    request_a.create();
    request_b.create();
    request_c.create();

    const DBT *one = get_dbt(1);
    const DBT *two = get_dbt(2);

    // start and succeed 1,1 for A and 2,2 for B.
    request_a.set(lt, txnid_a, one, one, lock_request::type::WRITE, false);
    r = request_a.start();
    invariant_zero(r);
    request_b.set(lt, txnid_b, two, two, lock_request::type::WRITE, false);
    r = request_b.start();
    invariant_zero(r);

    // txnid A should not be granted a lock on 2,2, so it goes pending.
    request_a.set(lt, txnid_a, two, two, lock_request::type::WRITE, false);
    r = request_a.start();
    invariant(r == DB_LOCK_NOTGRANTED);

    // if txnid B wants a lock on 1,1 it should deadlock with A
    request_b.set(lt, txnid_b, one, one, lock_request::type::WRITE, false);
    r = request_b.start();
    invariant(r == DB_LOCK_DEADLOCK);

    // txnid C should not deadlock on either of these - it should just time out.
    request_c.set(lt, txnid_c, one, one, lock_request::type::WRITE, false);
    r = request_c.start();
    invariant(r == DB_LOCK_NOTGRANTED);
    r = request_c.wait(lock_wait_time);
    invariant(r == DB_LOCK_NOTGRANTED);
    request_c.set(lt, txnid_c, two, two, lock_request::type::WRITE, false);
    r = request_c.start();
    invariant(r == DB_LOCK_NOTGRANTED);
    r = request_c.wait(lock_wait_time);
    invariant(r == DB_LOCK_NOTGRANTED);

    // release locks for A and B, then wait on A's request which should succeed
    // since B just unlocked and should have completed A's pending request.
    release_lock_and_retry_requests(lt, txnid_a, one, one);
    release_lock_and_retry_requests(lt, txnid_b, two, two);
    r = request_a.wait(lock_wait_time);
    invariant_zero(r);
    release_lock_and_retry_requests(lt, txnid_a, two, two);

    request_a.destroy();
    request_b.destroy();
    request_c.destroy();
    mgr.release_lt(lt);
    mgr.destroy();
}

} /* namespace toku */

int main(void) {
    toku::lock_request_unit_test test;
    test.test_start_deadlock();
    return 0;
}

