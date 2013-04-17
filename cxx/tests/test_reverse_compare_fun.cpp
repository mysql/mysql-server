/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
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

/* try a reverse compare function to verify that the database always uses the application's
   compare function */

#include <arpa/inet.h>
#include <assert.h>
#include <db_cxx.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <memory.h>

#define DIR __FILE__ ".dir"
int verbose;

int keycompare (const void *key1, unsigned int key1len, const void *key2, unsigned int key2len) {
    if (key1len==key2len) {
	return memcmp(key1,key2,key1len);
    } else if (key1len<key2len) {
	int r = memcmp(key1,key2,key1len);
	if (r<=0) return -1; /* If the keys are the same up to 1's length, then return -1, since key1 is shorter than key2. */
	else return 1;
    } else {
	return -keycompare(key2,key2len,key1,key1len);
    }
}

extern "C" int reverse_compare(DB *db __attribute__((__unused__)), const DBT *a, const DBT*b) {
    return -keycompare(a->data, a->size, b->data, b->size);
}

void expect(Dbc *cursor, int k, int v) {
    Dbt key; key.set_flags(DB_DBT_MALLOC);
    Dbt val; val.set_flags(DB_DBT_MALLOC);
    int r = cursor->get(&key, &val, DB_NEXT);
    assert(r == 0);
    assert(key.get_size() == sizeof k);
    int kk;
    memcpy(&kk, key.get_data(), key.get_size());
    assert(val.get_size() == sizeof v);
    int vv;
    memcpy(&vv, val.get_data(), val.get_size());
    if (kk != k || vv != v) printf("expect key %d got %d - %d %d\n", htonl(k), htonl(kk), htonl(v), htonl(vv));
    assert(kk == k);
    assert(vv == v);

    toku_free(key.get_data());
    toku_free(val.get_data());
}

void test_reverse_compare(int n) {
    if (verbose) printf("test_reverse_compare:%d\n", n);

    Db *db;
    DbTxn * const null_txn = 0;
    const char * const fname = "reverse.compare.db";

    int r;
    int i;

    system("rm -rf " DIR);
    toku_os_mkdir(DIR, 0777);

    /* create the dup database file */
    DbEnv env(DB_CXX_NO_EXCEPTIONS);
    r = env.set_redzone(0); assert(r==0);
    r = env.set_default_bt_compare(reverse_compare);
    assert(r == 0);
    r = env.open(DIR, DB_INIT_MPOOL + DB_CREATE + DB_PRIVATE, 0777); assert(r == 0);
    db = new Db(&env, DB_CXX_NO_EXCEPTIONS);
    assert(db);
    r = db->set_pagesize(4096);
    assert(r == 0);
    r = db->open(null_txn, fname, "main", DB_BTREE, DB_CREATE, 0666);
    assert(r == 0);

    /* insert n unique keys {0, 1,  n-1} */
    for (i=0; i<n; i++) {
        int k, v;
        k = htonl(i);
        Dbt key(&k, sizeof k);
        v = htonl(i);
        Dbt val(&v, sizeof v);
        r = db->put(null_txn, &key, &val, 0);
        assert(r == 0);
    }

    /* reopen the database to force nonleaf buffering */
    r = db->close(0);
    assert(r == 0);
    delete db;

    db = new Db(&env, 0);
    assert(db);
    r = db->set_pagesize(4096);
    assert(r == 0);
    r = db->open(null_txn, fname, "main", DB_BTREE, 0, 0666);
    assert(r == 0);

    /* insert n unique keys {n, n+1,  2*n-1} */
    for (i=n; i<2*n; i++) {
        int k, v;
        k = htonl(i);
        Dbt key(&k, sizeof k);
        v = htonl(i);
        Dbt val(&v, sizeof v);
        r = db->put(null_txn, &key, &val, 0);
        assert(r == 0);
    }

    /* verify the sort order with a cursor */
    Dbc *cursor;
    r = db->cursor(null_txn, &cursor, 0);
    assert(r == 0);

    //for (i=0; i<2*n; i++) 
    for (i=2*n-1; i>=0; i--)
        expect(cursor, htonl(i), htonl(i));

    r = cursor->close();
    assert(r == 0);

    r = db->close(0);
    assert(r == 0);
    delete db;
}

int main(int argc, const char *argv[]) {
    int i;
    for (i = 1; i <= (1<<16); i *= 2) {
        test_reverse_compare(i);
    }
    return 0;
}
