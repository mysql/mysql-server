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

#include "manager_unit_test.h"
#include "locktree_unit_test.h"
#include "lock_request_unit_test.h"

namespace toku {

static void assert_status(LTM_STATUS ltm_status, const char *keyname, uint64_t v) {
    TOKU_ENGINE_STATUS_ROW key_status = NULL;
    // lookup keyname in status
    for (int i = 0; ; i++) {
        TOKU_ENGINE_STATUS_ROW status = &ltm_status->status[i];
        if (status->keyname == NULL)
            break;
        if (strcmp(status->keyname, keyname) == 0) {
            key_status = status;
            break;
        }
    }
    assert(key_status);
    assert(key_status->value.num == v);
}

void manager_unit_test::test_status(void) {

    locktree::manager mgr;
    mgr.create(nullptr, nullptr, nullptr, nullptr);

    LTM_STATUS_S status;
    mgr.get_status(&status);
    assert_status(&status, "LTM_WAIT_COUNT", 0);
    assert_status(&status, "LTM_TIMEOUT_COUNT", 0);

    DESCRIPTOR desc = nullptr;
    DICTIONARY_ID dict_id = { 1 };
    locktree *lt = mgr.get_lt(dict_id, desc, compare_dbts, nullptr);
    int r;
    TXNID txnid_a = 1001;
    TXNID txnid_b = 2001;
    const DBT *one = get_dbt(1);

    // txn a write locks one
    r = lt->acquire_write_lock(txnid_a, one, one, nullptr, false);
    assert(r == 0);

    // txn b tries to write lock one, conflicts, waits, and fails to lock one
    lock_request request_b;
    request_b.create();
    request_b.set(lt, txnid_b, one, one, lock_request::type::WRITE, false);
    r = request_b.start();
    assert(r == DB_LOCK_NOTGRANTED);
    r = request_b.wait(1000);
    assert(r == DB_LOCK_NOTGRANTED);
    request_b.destroy();

    range_buffer buffer;
    buffer.create();
    buffer.append(one, one);
    lt->release_locks(txnid_a, &buffer);
    buffer.destroy();

    assert(lt->m_rangetree->is_empty() && lt->m_sto_buffer.is_empty());

    // assert that wait counters incremented
    mgr.get_status(&status);
    assert_status(&status, "LTM_WAIT_COUNT", 1);
    assert_status(&status, "LTM_TIMEOUT_COUNT", 1);

    // assert that wait counters are persistent after the lock tree is destroyed
    mgr.release_lt(lt);
    mgr.get_status(&status);
    assert_status(&status, "LTM_WAIT_COUNT", 1);
    assert_status(&status, "LTM_TIMEOUT_COUNT", 1);

    mgr.destroy();
}

} /* namespace toku */

int main(void) {
    toku::manager_unit_test test;
    test.test_status();
    return 0;
}
