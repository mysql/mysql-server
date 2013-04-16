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
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <assert.h>
#include <db_cxx.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <memory.h>

#define FNAME __FILE__ ".tdb"
#define FNAME2 __FILE__ "2.tdb"

#ifndef DB_DELETE_ANY
#define DB_DELETE_ANY 0
#endif

#ifndef DB_KEYEMPTY
#define DB_KEYEMPTY DB_NOTFOUND
#endif

#define DIR __FILE__ ".dir"
int verbose = 0;

#define TC(expr, expect)           \
  if (verbose) printf("%s expect %d\n", #expr, expect); \
  try {                            \
    expr;                          \
    assert(expect==0); 	           \
  } catch (DbException e) {        \
    if (e.get_errno()!=expect) fprintf(stderr, "err=%d %s\n", e.get_errno(), db_strerror(e.get_errno())); \
    assert(e.get_errno()==expect); \
  }

#define TCRET(expr, expect)        \
  if (verbose) printf("%s expect %d\n", #expr, expect); \
  try {                            \
    int r = expr;                  \
    assert(expect==r); 	           \
  } catch (DbException e) {        \
    if (e.get_errno()!=expect) fprintf(stderr, "err=%d %s\n", e.get_errno(), db_strerror(e.get_errno())); \
    assert(e.get_errno()==expect); \
  }

static void test_env_exceptions (void) {
    {
	DbEnv env(0);
	int r = env.set_redzone(0); assert(r==0);
	TC(env.open(DIR "no.such.dir", DB_INIT_MPOOL | DB_CREATE | DB_PRIVATE, 0777),        ENOENT);
    }
    {
	DbEnv env(0);
	int r = env.set_redzone(0); assert(r==0);
	TC(env.open(DIR "no.such.dir", (u_int32_t)-1, 0777),                                            EINVAL);
    }
    {
	system("rm -rf " DIR);
	toku_os_mkdir(DIR, 0777);
	DbEnv env(0);
	int r = env.set_redzone(0); assert(r==0);
	TC(env.open(DIR, DB_INIT_MPOOL | DB_CREATE | DB_PRIVATE, 0777),                  0);
	DbTxn *txn;
	TC(env.txn_begin(0, &txn, 0),                                                    EINVAL); // not configured for transactions
    }
    {
	system("rm -rf " DIR);
	toku_os_mkdir(DIR, 0777);
	DbEnv env(0);
	int r = env.set_redzone(0); assert(r==0);
	TC(env.open(DIR, DB_INIT_MPOOL | DB_CREATE | DB_PRIVATE | DB_INIT_LOG, 0777),    EINVAL); // cannot do logging without txns
    }
    {
	system("rm -rf " DIR);
	toku_os_mkdir(DIR, 0777);
	DbEnv env(0);
	int r = env.set_redzone(0); assert(r==0);
	TC(env.open(DIR, DB_INIT_MPOOL | DB_CREATE | DB_PRIVATE | DB_INIT_LOG | DB_INIT_TXN, 0777),    0);
	DbTxn *txn;
	TC(env.txn_begin(0, &txn, 0),                                                    0);
	TC(txn->commit(0),                                                               0); 
        delete txn;
    }
    {
	system("rm -rf " DIR);
	toku_os_mkdir(DIR, 0777);
	DbEnv env(0);
	env.set_errfile(stderr);
	int r = env.set_redzone(0); assert(r==0);
	TC(env.open(DIR, DB_INIT_MPOOL | DB_CREATE | DB_PRIVATE | DB_INIT_LOG | DB_INIT_LOCK | DB_INIT_TXN, 0777),    0);
	DbTxn *txn;
	TC(env.txn_begin(0, &txn, 0),                                                    0);
	TC(txn->commit(0),                                                               0); 
        delete txn;
    }
    {
	system("rm -rf " DIR);
	toku_os_mkdir(DIR, 0777);
	DbEnv env(0);
	int r = env.set_redzone(0); assert(r==0);
	TC(env.open(DIR, DB_INIT_MPOOL | DB_CREATE | DB_PRIVATE | DB_INIT_LOG | DB_INIT_TXN, 0777),    0);
	DbTxn *txn;
	TC(env.txn_begin(0, &txn, 0),                                                    0);
	TC(txn->commit((u_int32_t)-1),                                                   EINVAL);
        delete txn;
    }
    system("rm -rf " DIR);
}


static void test_db_exceptions (void) {
    system("rm -rf " DIR);
    toku_os_mkdir(DIR, 0777);
    DbEnv env(0);
    int r = env.set_redzone(0); assert(r==0);
    TC(env.open(DIR, DB_INIT_MPOOL | DB_CREATE | DB_PRIVATE , 0777),    0);
    env.set_errfile(stderr);
    TC( { Db db(&env, (u_int32_t)-1); assert(0); },   EINVAL); // Create with flags=-1 should do an EINVAL
    Db db(&env, 0);
    DB *dbdb=db.get_DB();
    assert(dbdb!=0);
    assert(dbdb==db.get_const_DB());
    assert(&db==Db::get_const_Db(dbdb));
    unlink(FNAME);
    TC(db.open(0, FNAME, 0, DB_BTREE, DB_CREATE, 0777), 0);
    TC(db.open(0, FNAME, 0, DB_BTREE, DB_CREATE, 0777), EINVAL); // it was already open
    {
	Db db2(&env, 0);
	TC(db2.open(0, FNAME2, 0, DB_BTREE, 0, 0777), ENOENT); // it doesn't exist
    }
    {
	Db db2(&env, 0);
	TC(db2.open(0, FNAME, 0, DB_BTREE, 0, 0777), 0); // it does exist
    }
    {
	Db db2(&env, 0);
	TC(db2.open(0, FNAME, 0, DB_BTREE, (u_int32_t)-1, 0777), EINVAL); // bad flags
    }
    {
	Db db2(&env, 0);
	TC(db2.open(0, FNAME, 0, (DBTYPE)-1, 0, 0777), EINVAL); // bad type
    }
    {
	Db db2(&env, 0);
	TC(db2.open(0, FNAME, "sub.db", DB_BTREE, DB_CREATE, 0777), ENOTDIR); // sub DB cannot exist
    }
    {
	Db db2(&env, 0);
	TC(db2.open(0, FNAME, "sub.db", DB_BTREE, 0, 0777), ENOTDIR); // sub DB cannot exist withou DB_CREATE
    }
    {
	Dbc *curs;
	TC(db.cursor(0, &curs, (u_int32_t)-1),  EINVAL);
    }
    {
	Dbc *curs;
	TC(db.cursor(0, &curs, 0),  0);
	Dbt key,val;
        //	TC(curs->get(&key, &val, DB_FIRST), DB_NOTFOUND);
	TC(curs->get(&key, &val, (u_int32_t)-1), EINVAL); // bad flags
	curs->close(); // no deleting cursors.
    }
    {
	Dbt key,val;
	TC(db.del(0, &key, (u_int32_t)-1), EINVAL);
	TC(db.get(0, &key, &val, (u_int32_t)-1), EINVAL);
	TC(db.put(0, &key, &val, (u_int32_t)-1), EINVAL);
    }
    {
	Dbt key((char*)"hello", 6);
	Dbt val((char*)"there", 6);
	Dbt valget;
	TC(db.put(0, &key, &val, 0), 0);
	TC(db.get(0, &key, &valget, 0), 0);
	assert(strcmp((const char*)(valget.get_data()), "there")==0);
    }
}
	

static void test_dbc_exceptions () {
    system("rm -rf " DIR);
    toku_os_mkdir(DIR, 0777);
    DbEnv env(0);
    TC(env.open(DIR, DB_INIT_MPOOL | DB_CREATE | DB_PRIVATE , 0777),    0);
    Db db(&env, 0);
    unlink(FNAME);
    TC(db.open(0, FNAME, 0, DB_BTREE, DB_CREATE, 0777), 0);
    for (int k = 1; k<4; k++) {
        Dbt key(&k, sizeof k);
        Dbt val(&k, sizeof k);
        TC(db.put(0, &key, &val, 0), 0);
    }
    Dbc *curs;
    TC(db.cursor(0, &curs, 0),  0);
    Dbt key; key.set_flags(DB_DBT_MALLOC);
    Dbt val; val.set_flags(DB_DBT_MALLOC);
    TC(curs->get(&key, &val, DB_FIRST), 0);
    toku_free(key.get_data());
    toku_free(val.get_data());
#if 0
    // c_del no longer supported. See #4576.
    TC(curs->del(DB_DELETE_ANY), 0);
    TCRET(curs->get(&key, &val, DB_CURRENT), DB_KEYEMPTY);
    TCRET(curs->del(0), DB_KEYEMPTY);
    TCRET(curs->del(DB_DELETE_ANY), 0);
#endif
    curs->close(); // no deleting cursors.
}

int main(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        char *arg = argv[i];
        if (0 == strcmp(arg, "-v")) {
            verbose++;
            continue;
        }
    }
    test_env_exceptions();
    test_db_exceptions();
    test_dbc_exceptions();
    system("rm -rf " DIR);
    return 0;
}
