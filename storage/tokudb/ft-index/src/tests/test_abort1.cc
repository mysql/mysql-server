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

/* Simple test of logging.  Can I start TokuFT with logging enabled? */

#include <errno.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <db.h>
#include <memory.h>
#include <stdio.h>


// TOKU_TEST_FILENAME is defined in the Makefile

static void
test_db_open_aborts (void) {
    DB_ENV *env;
    DB *db;

    int r;
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    r=toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO);       assert(r==0);
    r=db_env_create(&env, 0); assert(r==0);
    r=env->open(env, TOKU_TEST_FILENAME, DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_PRIVATE|DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    r=db_create(&db, env, 0); CKERR(r);
    {
	DB_TXN *tid;
	r=env->txn_begin(env, 0, &tid, 0); assert(r==0);
	r=db->open(db, tid, "foo.db", 0, DB_BTREE, DB_CREATE, S_IRWXU|S_IRWXG|S_IRWXO); CKERR(r);
	{
	    DBT key,data;
            dbt_init(&key, "hello", 6);
            dbt_init(&data, "there", 6);
	    r=db->put(db, tid, &key, &data, 0);
	    CKERR(r);
	}
        r=db->close(db, 0);       assert(r==0);
	r=tid->abort(tid);        assert(r==0);
    }
    {
        {
            DBT dname;
            DBT iname;
            dbt_init(&dname, "foo.db", sizeof("foo.db"));
            dbt_init(&iname, NULL, 0);
            iname.flags |= DB_DBT_MALLOC;
            r = env->get_iname(env, &dname, &iname);
            CKERR2(r, DB_NOTFOUND);
        }
        toku_struct_stat statbuf;
        char filename[TOKU_PATH_MAX+1];
        r = toku_stat(toku_path_join(filename, 2, TOKU_TEST_FILENAME, "foo.db"), &statbuf);
        assert(r!=0);
        assert(errno==ENOENT);
    }

    r=env->close(env, 0);     assert(r==0);
}

// Do two transactions, one commits, and one aborts.  Do them concurrently.
static void
test_db_put_aborts (void) {
    DB_ENV *env;
    DB *db;

    int r;
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    r=toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO);       assert(r==0);
    r=db_env_create(&env, 0); assert(r==0);
    r=env->open(env, TOKU_TEST_FILENAME, DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_PRIVATE|DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    r=db_create(&db, env, 0); CKERR(r);

    {
	DB_TXN *tid;
	r=env->txn_begin(env, 0, &tid, 0); assert(r==0);
	r=db->open(db, tid, "foo.db", 0, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
	r=tid->commit(tid,0);        assert(r==0);
    }
    {
	DB_TXN *tid;
	DB_TXN *tid2;
	r=env->txn_begin(env, 0, &tid, 0);  assert(r==0);
	r=env->txn_begin(env, 0, &tid2, 0); assert(r==0);
	{
	    DBT key,data;
            dbt_init(&key, "hello", 6);
            dbt_init(&data, "there", 6);
	    r=db->put(db, tid, &key, &data, 0);
	    CKERR(r);
	}
	{
	    DBT key,data;
            dbt_init(&key, "bye", 4);
            dbt_init(&data, "now", 4);
	    r=db->put(db, tid2, &key, &data, 0);
	    CKERR(r);
	}
	//printf("%s:%d aborting\n", __FILE__, __LINE__);
	r=tid->abort(tid);        assert(r==0);
	//printf("%s:%d committing\n", __FILE__, __LINE__);
	r=tid2->commit(tid2,0);   assert(r==0);
    }
    // The database should exist
    {
        char *filename;
        {
            DBT dname;
            DBT iname;
            dbt_init(&dname, "foo.db", sizeof("foo.db"));
            dbt_init(&iname, NULL, 0);
            iname.flags |= DB_DBT_MALLOC;
            r = env->get_iname(env, &dname, &iname);
            CKERR(r);
            CAST_FROM_VOIDP(filename, iname.data);
            assert(filename);
        }
	toku_struct_stat statbuf;
        char fullfile[TOKU_PATH_MAX+1];
	r = toku_stat(toku_path_join(fullfile, 2, TOKU_TEST_FILENAME, filename), &statbuf);
	assert(r==0);
        toku_free(filename);
    }
    // But the item should not be in it.
    if (1)
    {
	DB_TXN *tid;
	r=env->txn_begin(env, 0, &tid, 0); assert(r==0);
	{
	    DBT key,data;
            dbt_init(&key, "hello", 6);
            dbt_init(&data, NULL, 0);
	    r=db->get(db, tid, &key, &data, 0);
	    assert(r!=0);
	    assert(r==DB_NOTFOUND);
	}	    
	{
	    DBT key,data;
            dbt_init(&key, "bye", 4);
            dbt_init(&data, NULL, 0);
	    r=db->get(db, tid, &key, &data, 0);
	    CKERR(r);
	}	    
	r=tid->commit(tid,0);        assert(r==0);
    }

    r=db->close(db, 0);       assert(r==0);
    r=env->close(env, 0);     assert(r==0);
}

int
test_main (int UU(argc), char UU(*const argv[])) {
    test_db_open_aborts();
    test_db_put_aborts();
    return 0;
}
