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

#include "test.h"

static DB_TXN *txn1, *txn2, *txn3;
static uint64_t txnid1, txnid2, txnid3;

struct iterate_extra {
    iterate_extra() : n(0) {
        visited_txn[0] = false;
        visited_txn[1] = false;
        visited_txn[2] = false;
    }
    int n;
    bool visited_txn[3];
};

static int iterate_callback(DB_TXN *txn,
                            iterate_row_locks_callback iterate_locks,
                            void *locks_extra, void *extra) {
    uint64_t txnid = txn->id64(txn);
    uint64_t client_id = txn->get_client_id(txn);
    iterate_extra *info = reinterpret_cast<iterate_extra *>(extra);
    DB *db;
    DBT left_key, right_key;
    int r = iterate_locks(&db, &left_key, &right_key, locks_extra);
    invariant(r == DB_NOTFOUND);
    if (txnid == txnid1) {
        assert(!info->visited_txn[0]);
        invariant(client_id == 0);
        info->visited_txn[0] = true;
    } else if (txnid == txnid2) {
        assert(!info->visited_txn[1]);
        invariant(client_id == 1);
        info->visited_txn[1] = true;
    } else if (txnid == txnid3) {
        assert(!info->visited_txn[2]);
        invariant(client_id == 2);
        info->visited_txn[2] = true;
    }
    info->n++;
    return 0;
}

int test_main(int UU(argc), char *const UU(argv[])) {
    int r;
    const int env_flags = DB_INIT_MPOOL | DB_CREATE | DB_THREAD |
        DB_INIT_LOCK | DB_INIT_LOG | DB_INIT_TXN | DB_PRIVATE;

    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    r = toku_os_mkdir(TOKU_TEST_FILENAME, 0755); CKERR(r);

    DB_ENV *env;
    r = db_env_create(&env, 0); CKERR(r);
    r = env->iterate_live_transactions(env, iterate_callback, NULL);
    assert(r == EINVAL);
    r = env->open(env, TOKU_TEST_FILENAME, env_flags, 0755); CKERR(r);

    r = env->txn_begin(env, NULL, &txn1, 0); CKERR(r);
    txn1->set_client_id(txn1, 0);
    txnid1 = txn1->id64(txn1);
    r = env->txn_begin(env, NULL, &txn2, 0); CKERR(r);
    txn2->set_client_id(txn2, 1);
    txnid2 = txn2->id64(txn2);
    r = env->txn_begin(env, NULL, &txn3, 0); CKERR(r);
    txn3->set_client_id(txn3, 2);
    txnid3 = txn3->id64(txn3);

    {
        iterate_extra e;
        r = env->iterate_live_transactions(env, iterate_callback, &e); CKERR(r);
        assert(e.visited_txn[0]);
        assert(e.visited_txn[1]);
        assert(e.visited_txn[2]);
        assert(e.n == 3);
    }

    r = txn1->commit(txn1, 0); CKERR(r);
    r = txn2->abort(txn2); CKERR(r);
    {
        iterate_extra e;
        r = env->iterate_live_transactions(env, iterate_callback, &e); CKERR(r);
        assert(!e.visited_txn[0]);
        assert(!e.visited_txn[1]);
        assert(e.visited_txn[2]);
        assert(e.n == 1);
    }

    r = txn3->commit(txn3, 0); CKERR(r);
    {
        iterate_extra e;
        r = env->iterate_live_transactions(env, iterate_callback, &e); CKERR(r);
        assert(!e.visited_txn[0]);
        assert(!e.visited_txn[1]);
        assert(!e.visited_txn[2]);
        assert(e.n == 0);
    }

    r = env->close(env, 0); CKERR(r);
    return 0;
}
