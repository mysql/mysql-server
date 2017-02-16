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
// this test makes sure the LSN filtering is used during recovery of put_multiple

#include <sys/stat.h>
#include <fcntl.h>
#include "test.h"

const int envflags = DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE;

const char *namea="a.db";
const char *nameb="b.db";
enum {num_dbs = 2};
static DBT dest_keys[num_dbs];
static DBT dest_vals[num_dbs];

bool do_test=false, do_recover=false;

static int
put_multiple_generate(DB *dest_db, DB *src_db, DBT_ARRAY *dest_key_arrays, DBT_ARRAY *dest_val_arrays, const DBT *src_key, const DBT *src_val) {
    toku_dbt_array_resize(dest_key_arrays, 1);
    toku_dbt_array_resize(dest_val_arrays, 1);
    DBT *dest_key = &dest_key_arrays->dbts[0];
    DBT *dest_val = &dest_val_arrays->dbts[0];
    if (src_db) {
        assert(src_db->descriptor);
        assert(src_db->descriptor->dbt.size == 4);
        assert((*(uint32_t*)src_db->descriptor->dbt.data) == 0);
    }
    assert(dest_db->descriptor->dbt.size == 4);
    uint32_t which = *(uint32_t*)dest_db->descriptor->dbt.data;
    assert(which < num_dbs);

    if (dest_key->data) toku_free(dest_key->data);
    if (dest_val->data) toku_free(dest_val->data);
    dest_key->data = toku_xmemdup (src_key->data, src_key->size);
    dest_key->size = src_key->size;
    dest_val->data = toku_xmemdup (src_val->data, src_val->size);
    dest_val->size = src_val->size;
    return 0;
}

static void run_test (void) {
    int r;

    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO);

    DB_ENV *env;
    r = db_env_create(&env, 0);                                                         CKERR(r);
    r = env->set_generate_row_callback_for_put(env, put_multiple_generate);
    CKERR(r);
    r = env->open(env, TOKU_TEST_FILENAME, envflags, S_IRWXU+S_IRWXG+S_IRWXO);                      CKERR(r);

    // create a txn that never closes, forcing recovery to run from the beginning of the log
    {
        DB_TXN *oldest_living_txn;
        r = env->txn_begin(env, NULL, &oldest_living_txn, 0);                                         CKERR(r);
    }

    DBT descriptor;
    uint32_t which;
    for (which = 0; which < num_dbs; which++) {
        dbt_init_realloc(&dest_keys[which]);
        dbt_init_realloc(&dest_vals[which]);
    }
    dbt_init(&descriptor, &which, sizeof(which));
    DB *dba;
    DB *dbb;
    r = db_create(&dba, env, 0);                                                        CKERR(r);
    r = db_create(&dbb, env, 0);                                                        CKERR(r);
    r = dba->open(dba, NULL, namea, NULL, DB_BTREE, DB_AUTO_COMMIT|DB_CREATE, 0666);    CKERR(r);
    which = 0;
    IN_TXN_COMMIT(env, NULL, txn_desc, 0, {
            { int chk_r = dba->change_descriptor(dba, txn_desc, &descriptor, 0); CKERR(chk_r); }
    });
    r = dbb->open(dbb, NULL, nameb, NULL, DB_BTREE, DB_AUTO_COMMIT|DB_CREATE, 0666);    CKERR(r);
    which = 1;
    IN_TXN_COMMIT(env, NULL, txn_desc, 0, {
            { int chk_r = dbb->change_descriptor(dbb, txn_desc, &descriptor, 0); CKERR(chk_r); }
    });

    DB *dbs[num_dbs] = {dba, dbb};
    uint32_t flags[num_dbs] = {0, 0};
    // txn_begin; insert <a,a>; txn_abort
    {
        DB_TXN *txn;
        r = env->txn_begin(env, NULL, &txn, 0);                                         CKERR(r);
        DBT k,v;
        dbt_init(&k, "a", 2);
        dbt_init(&v, "b", 2);

        r = env_put_multiple_test_no_array(env, dba, txn, &k, &v, num_dbs, dbs, dest_keys, dest_vals, flags);
        CKERR(r);
        r = txn->abort(txn);                                                            CKERR(r);
    }
    r = dbb->close(dbb, 0);                                                             CKERR(r);
    r = db_create(&dbb, env, 0);                                                        CKERR(r);
    r = dbb->open(dbb, NULL, nameb, NULL, DB_BTREE, DB_AUTO_COMMIT, 0666);    CKERR(r);
    dbs[1] = dbb;

    // txn_begin; insert <a,b>;
    {
        DB_TXN *txn;
        r = env->txn_begin(env, NULL, &txn, 0);                                         CKERR(r);
        DBT k,v;
        dbt_init(&k, "a", 2);
        dbt_init(&v, "b", 2);

        r = env_put_multiple_test_no_array(env, NULL, txn, &k, &v, num_dbs, dbs, dest_keys, dest_vals, flags);
        CKERR(r);
        r = txn->commit(txn, 0);                                                        CKERR(r);
    }
    {
        DB_TXN *txn;
        r = env->txn_begin(env, NULL, &txn, 0);                                         CKERR(r);
        r = dba->close(dbb, 0);                                                         CKERR(r);
        r = env->dbremove(env, txn, nameb, NULL, 0);                                    CKERR(r);
        r = txn->commit(txn, 0);                                                        CKERR(r);
    }

    r = env->log_flush(env, NULL); CKERR(r);
    // abort the process
    toku_hard_crash_on_purpose();
}


static void run_recover (void) {
    DB_ENV *env;
    int r;

    // Recovery starts from oldest_living_txn, which is older than any inserts done in run_test,
    // so recovery always runs over the entire log.

    // run recovery
    r = db_env_create(&env, 0);                                                         CKERR(r);
    r = env->set_generate_row_callback_for_put(env, put_multiple_generate);
    CKERR(r);
    r = env->open(env, TOKU_TEST_FILENAME, envflags + DB_RECOVER, S_IRWXU+S_IRWXG+S_IRWXO);         CKERR(r);

    // verify the data
    {
        DB *db;
        r = db_create(&db, env, 0);                                                         CKERR(r);
        r = db->open(db, NULL, nameb, NULL, DB_UNKNOWN, DB_AUTO_COMMIT, 0666);              CKERR2(r, ENOENT);
        r = db->close(db, 0);                                                               CKERR(r);
    }
    {
        DB *db;
        r = db_create(&db, env, 0);                                                         CKERR(r);
        r = db->open(db, NULL, namea, NULL, DB_UNKNOWN, DB_AUTO_COMMIT, 0666);              CKERR(r);
        
        DB_TXN *txn;
        r = env->txn_begin(env, NULL, &txn, 0);                                             CKERR(r);
        DBC *cursor;
        r = db->cursor(db, txn, &cursor, 0);                                                CKERR(r);
        DBT k, v;
        r = cursor->c_get(cursor, dbt_init_malloc(&k), dbt_init_malloc(&v), DB_FIRST);
        CKERR(r);
        assert(k.size == 2);
        assert(v.size == 2);
        assert(memcmp(k.data, "a", 2) == 0);
        assert(memcmp(v.data, "b", 2) == 0);
        toku_free(k.data);
        toku_free(v.data);

        r = cursor->c_close(cursor);                                                        CKERR(r);

        r = txn->commit(txn, 0); CKERR(r);
        r = db->close(db, 0); CKERR(r);
    }
    r = env->close(env, 0);                                                             CKERR(r);
    exit(0);
}

const char *cmd;

static void test_parse_args (int argc, char * const argv[]) {
    int resultcode;
    cmd = argv[0];
    argc--; argv++;
    while (argc>0) {
	if (strcmp(argv[0], "-v") == 0) {
	    verbose++;
	} else if (strcmp(argv[0],"-q")==0) {
	    verbose--;
	    if (verbose<0) verbose=0;
	} else if (strcmp(argv[0], "--test")==0) {
	    do_test=true;
        } else if (strcmp(argv[0], "--recover") == 0) {
            do_recover=true;
	} else if (strcmp(argv[0], "-h")==0) {
	    resultcode=0;
	do_usage:
	    fprintf(stderr, "Usage:\n%s [-v|-q]* [-h] {--test | --recover } \n", cmd);
	    exit(resultcode);
	} else {
	    fprintf(stderr, "Unknown arg: %s\n", argv[0]);
	    resultcode=1;
	    goto do_usage;
	}
	argc--;
	argv++;
    }
}

int test_main (int argc, char * const argv[]) {
    test_parse_args(argc, argv);
    if (do_test) {
	run_test();
    } else if (do_recover) {
        run_recover();
    }
    return 0;
}
