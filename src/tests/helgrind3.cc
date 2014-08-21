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
#include "test.h"
// The helgrind2 test performs a DB->get() in two different concurrent threads.
#include <arpa/inet.h>

#include <db.h>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>
#include <memory.h>

DB_ENV *env;
DB *db;

static void initialize (void) {
    int r;
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    toku_os_mkdir(TOKU_TEST_FILENAME, 0777);

    // setup environment
    {
        r = db_env_create(&env, 0); assert(r == 0);
	r = env->set_redzone(env, 0); CKERR(r);
        env->set_errfile(env, stdout);
        r = env->open(env, TOKU_TEST_FILENAME, DB_INIT_MPOOL + DB_PRIVATE + DB_CREATE, 0777); 
        assert(r == 0);
    }

    // setup DB
    {
        DB_TXN *txn = 0;
        r = db_create(&db, env, 0); assert(r == 0);
        r = db->open(db, txn, "test.db", 0, DB_BTREE, DB_CREATE, 0777); assert(r == 0);
    }

    // Put some stuff in
    {    
        char v[10];
        DB_TXN *txn = 0;
        int i;
	const int n = 10;
	memset(v, 0, sizeof(v));
        for (i=0; i<n; i++) {
            int k = htonl(i);
            DBT key, val;
            r = db->put(db, txn, dbt_init(&key, &k, sizeof k), dbt_init(&val, v, sizeof(v)), 0);
            assert(r == 0);
        }
    }
}

static void finish (void) {
    int r;
    r = db->close(db, 0); assert(r==0);
    r = env->close(env, 0); assert(r==0);
}

static void *starta(void* ignore __attribute__((__unused__))) {
    DB_TXN *txn = 0;
    DBT key, val;
    char data[10];
    val.data = data;
    memset(&key, 0, sizeof(key));
    memset(&val, 0, sizeof(val));
    val.flags |= DB_DBT_MALLOC;
    int k = htonl(99);
    int r = db->put(db, txn, dbt_init(&key, &k, sizeof k), &val, 0);
    assert(r==0);
    //printf("val.data=%p\n", val.data);
    return 0;
}
static void *startb(void* ignore __attribute__((__unused__))) {
    DB_TXN *txn = 0;
    DBT key, val;
    memset(&key, 0, sizeof(key));
    memset(&val, 0, sizeof(val));
    int k = htonl(0);
    val.flags |= DB_DBT_MALLOC;
    int r = db->get(db, txn, dbt_init(&key, &k, sizeof k), &val, 0);
    assert(r==0);
    //printf("val.data=%p\n", val.data);
    int i; for (i=0; i<10; i++) assert(((char*)val.data)[i]==0);
    toku_free(val.data);
    return 0;
}

int
test_main (int argc, char * const argv[]) {
    parse_args(argc, argv);
    pthread_t a,b;
    initialize();
    { int x = pthread_create(&a, NULL, starta, NULL); assert(x==0); }
    { int x = pthread_create(&b, NULL, startb, NULL); assert(x==0); }
    { int x = pthread_join(a, NULL);           assert(x==0); }
    { int x = pthread_join(b, NULL);           assert(x==0); }
    finish();
    return 0;
}
