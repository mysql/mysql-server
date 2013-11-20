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
// test that an update calls back into the update function

#include "test.h"

const int envflags = DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE;

DB_ENV *env;

const int to_update[] =      { 0, 1, 1, 1, 0, 0, 1, 0, 1, 0  };
      int updates_called[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0  };

// the commands are: byte 1 is "nop" "add" or "del".  Byte 2 is the amount to add.
enum cmd { CNOP, CADD, CDEL };

static int update_fun(DB *UU(db),
                      const DBT *key,
                      const DBT *UU(old_val), const DBT *UU(extra),
                      void UU((*set_val)(const DBT *new_val,
                                         void *set_extra)),
                      void *UU(set_extra)) {
    unsigned int *k;
    assert(key->size == sizeof(*k));
    CAST_FROM_VOIDP(k, key->data);
    assert(to_update[*k] == 1);
    assert(updates_called[*k] == 0);
    updates_called[*k] = 1;
    return 0;
}

static void setup (void) {
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    { int chk_r = toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(chk_r); }
    { int chk_r = db_env_create(&env, 0); CKERR(chk_r); }
    env->set_errfile(env, stderr);
    env->set_update(env, update_fun);
    { int chk_r = env->open(env, TOKU_TEST_FILENAME, envflags, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(chk_r); }
}

static void cleanup (void) {
    { int chk_r = env->close(env, 0); CKERR(chk_r); }
}

int test_main (int argc, char * const argv[]) {
    parse_args(argc, argv);
    setup();

    DB *db;

    {
        DB_TXN* txna = NULL;
        { int chk_r = env->txn_begin(env, NULL, &txna, 0); CKERR(chk_r); }

        { int chk_r = db_create(&db, env, 0); CKERR(chk_r); }
        { int chk_r = db->open(db, txna, "foo.db", NULL, DB_BTREE, DB_CREATE, 0666); CKERR(chk_r); }

        {
            DBT key, val;
            unsigned int i;
            DBT *keyp = dbt_init(&key, &i, sizeof(i));
            DBT *valp = dbt_init(&val, "a", 2);
            for (i = 0; i < (sizeof(to_update) / sizeof(to_update[0])); ++i) {
                { int chk_r = db->put(db, txna, keyp, valp, 0); CKERR(chk_r); }
            }
        }

        { int chk_r = txna->commit(txna, 0); CKERR(chk_r); }
    }

    {
        DB_TXN *txnb = NULL;
        { int chk_r = env->txn_begin(env, NULL, &txnb, 0); CKERR(chk_r); }

        {
            DBT key, nullextra;
            unsigned int i;
            DBT *keyp = dbt_init(&key, &i, sizeof(i));
            DBT *nullextrap = dbt_init(&nullextra, NULL, 0);
            for (i = 0; i < (sizeof(to_update) / sizeof(to_update[0])); ++i) {
                if (to_update[i] == 1) {
                    { int chk_r = db->update(db, txnb, keyp, nullextrap, 0); CKERR(chk_r); }
                }
            }
        }

        { int chk_r = txnb->commit(txnb, 0); CKERR(chk_r); }
    }

    { int chk_r = db->close(db, 0); CKERR(chk_r); }

    cleanup();

    for (unsigned int i = 0;
         i < (sizeof(to_update) / sizeof(to_update[0])); ++i) {
        assert(to_update[i] == updates_called[i]);
    }

    return 0;
}
