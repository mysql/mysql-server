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

// Test that a db ignores insert messages in blackhole mode

#include "test.h"
#include <util/dbt.h>

static DB *db;
static DB *blackhole_db;
static DB_ENV *env;

static int num_inserts = 10000;

static void fill_dbt(DBT *dbt, void *data, size_t size) {
    dbt->data = data;
    dbt->size = dbt->ulen = size;
    dbt->flags = DB_DBT_USERMEM;
}

static void setup (bool use_txns) {
    int r;
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    r = toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    r = db_env_create(&env, 0); CKERR(r);
    r = env->open(env, TOKU_TEST_FILENAME, 0, 0);
    int txnflags = use_txns ? (DB_INIT_LOCK | DB_INIT_LOG | DB_INIT_TXN) : 0;
    r = env->open(env, TOKU_TEST_FILENAME, DB_CREATE|DB_PRIVATE|txnflags, 0777);

    // create a regular db and a blackhole db
    r = db_create(&db, env, 0); CKERR(r);
    r = db_create(&blackhole_db, env, 0); CKERR(r);
    r = db->open(db, NULL, "test.db", 0, DB_BTREE,
            DB_CREATE,
            S_IRWXU+S_IRWXG+S_IRWXO);
    CKERR(r);
    r = blackhole_db->open(blackhole_db, NULL, "blackhole.db", 0, DB_BTREE, 
            DB_CREATE | DB_BLACKHOLE,
            S_IRWXU+S_IRWXG+S_IRWXO);
    CKERR(r);
}

static void cleanup (void) {
    int r;
    r = db->close(db, 0); CKERR(r);
    r = blackhole_db->close(blackhole_db, 0); CKERR(r);
    r = env->close(env, 0); CKERR(r);
}

static void test_blackhole(void) {
    int r = 0;

    for (int i = 0; i < num_inserts; i++) {
        int k = random();
        int v = k + 100;
        DBT key, value;
        fill_dbt(&key, &k, sizeof k); 
        fill_dbt(&value, &v, sizeof v); 

        // put a random key into the regular db.
        r = db->put(db, NULL, &key, &value, 0);
        assert(r == 0);

        // put that key into the blackhole db.
        r = blackhole_db->put(blackhole_db, NULL, &key, &value, 0);
        assert(r == 0);

        // we should be able to find this key in the regular db
        int get_v;
        DBT get_value;
        fill_dbt(&get_value, &get_v, sizeof get_v);
        r = db->get(db, NULL, &key, &get_value, 0);
        assert(r == 0);
        assert(*(int *)get_value.data == v);
        assert(get_value.size == sizeof v);

        // we shouldn't be able to get it back from the blackhole
        r = blackhole_db->get(blackhole_db, NULL, &key, &get_value, 0);
        assert(r == DB_NOTFOUND);
    }
}

int test_main (int argc __attribute__((__unused__)), char *const argv[] __attribute__((__unused__))) {
    // without txns
    setup(false);
    test_blackhole();
    cleanup();

    // with txns
    setup(true);
    test_blackhole();
    cleanup();
    return 0;
}
