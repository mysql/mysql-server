/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
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

#ident "Copyright (c) 2009-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#include "test.h"
#include "toku_pthread.h"
#include "key-val.h"
#include <db.h>
#include <sys/stat.h>

DB_ENV *env;
enum {NUM_DBS=1};
enum {NUM_KV_PAIRS=3};
struct kv_pair {
    int64_t key;
    int64_t val;
};
struct kv_pair kv_pairs[NUM_KV_PAIRS] = {{1,4},
                                         {2,5},
                                         {3,6}};

static void test_indexer(DB *src, DB **dbs)
{
    int r;
    DB_TXN    *txn;
    DB_INDEXER *indexer;
    uint32_t db_flags[NUM_DBS];

    if ( verbose ) printf("test_indexer\n");
    for(int i=0;i<NUM_DBS;i++) { 
        db_flags[i] = DB_NOOVERWRITE; 
    }

    // create and initialize loader
    r = env->txn_begin(env, NULL, &txn, 0);                                                               
    CKERR(r);

    if ( verbose ) printf("test_indexer create_indexer\n");
    r = env->create_indexer(env, txn, &indexer, src, NUM_DBS, dbs, db_flags, 0);
    CKERR(r);
    r = indexer->set_error_callback(indexer, NULL, NULL);
    CKERR(r);
    r = indexer->set_poll_function(indexer, poll_print, NULL);
    CKERR(r);

    if ( verbose ) printf("test_indexer build\n");
    r = indexer->build(indexer);
    CKERR(r);

    if ( verbose ) printf("test_indexer close\n");
    r = indexer->close(indexer);
    CKERR(r);
    r = txn->commit(txn, DB_TXN_SYNC);
    CKERR(r);
    
    if ( verbose ) printf("PASS\n");
    if ( verbose ) printf("test_indexer done\n");
}

const char *src_name="src.db";

static void run_test(void) 
{
    int r;
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    r = toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO);                          CKERR(r);
    char logname[TOKU_PATH_MAX+1];
    r = toku_os_mkdir(toku_path_join(logname, 2, TOKU_TEST_FILENAME, "log"), S_IRWXU+S_IRWXG+S_IRWXO);                   CKERR(r);

    r = db_env_create(&env, 0);                                                  CKERR(r);
    r = env->set_lg_dir(env, "log");                                             CKERR(r);
    r = env->set_default_bt_compare(env, int64_dbt_cmp);                         CKERR(r);
    r = env->set_generate_row_callback_for_put(env, put_multiple_generate_switch);      CKERR(r);
    int envflags = DB_INIT_LOCK | DB_INIT_LOG | DB_INIT_MPOOL | DB_INIT_TXN | DB_CREATE | DB_PRIVATE | DB_INIT_LOG;
    r = env->open(env, TOKU_TEST_FILENAME, envflags, S_IRWXU+S_IRWXG+S_IRWXO);               CKERR(r);
    env->set_errfile(env, stderr);
    //Disable auto-checkpointing
    r = env->checkpointing_set_period(env, 0);                                   CKERR(r);

    DB *src_db = NULL;
    r = db_create(&src_db, env, 0);                                                             CKERR(r);
    r = src_db->open(src_db, NULL, src_name, NULL, DB_BTREE, DB_AUTO_COMMIT|DB_CREATE, 0666);   CKERR(r);
    DBT key, val;
    DB_TXN *txn;
    r = env->txn_begin(env, NULL, &txn, 0);                                      CKERR(r);
    for(int i=0;i<NUM_KV_PAIRS;i++) {
        dbt_init(&key, &kv_pairs[i].key, sizeof(kv_pairs[i].key));
        dbt_init(&val, &kv_pairs[i].val, sizeof(kv_pairs[i].val));
        r = src_db->put(src_db, txn, &key, &val, 0);                             CKERR(r);
    }

    DB *dbs[NUM_DBS];
    for (int i = 0; i < NUM_DBS; i++) {
        r = db_create(&dbs[i], env, 0); CKERR(r);
        char key_name[32]; 
        sprintf(key_name, "key%d", i);
        r = dbs[i]->open(dbs[i], NULL, key_name, NULL, DB_BTREE, DB_AUTO_COMMIT|DB_CREATE, 0666);   CKERR(r);
        dbs[i]->app_private = (void *) (intptr_t) i;
    }

    // -------------------------- //
    if (1) test_indexer(src_db, dbs);
    // -------------------------- //

    r = txn->commit(txn, DB_TXN_SYNC);                                           CKERR(r);

    for(int i=0;i<NUM_DBS;i++) {
        r = dbs[i]->close(dbs[i], 0);                                            CKERR(r);
    }

    r = src_db->close(src_db, 0);                                                CKERR(r);
    r = env->close(env, 0);                                                      CKERR(r);
}


// ------------ infrastructure ----------
static void do_args(int argc, char * const argv[]);

int test_main(int argc, char * const *argv) {
    do_args(argc, argv);
    run_test();
    return 0;
}

static void do_args(int argc, char * const argv[]) {
    int resultcode;
    char *cmd = argv[0];
    argc--; argv++;
    while (argc>0) {
	if (strcmp(argv[0], "-v")==0) {
	    verbose++;
	} else if (strcmp(argv[0],"-q")==0) {
	    verbose--;
	    if (verbose<0) verbose=0;
        } else if (strcmp(argv[0], "-h")==0) {
	    resultcode=0;
	do_usage:
	    fprintf(stderr, "Usage:\n%s\n", cmd);
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
