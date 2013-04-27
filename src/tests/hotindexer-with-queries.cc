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

#ident "Copyright (c) 2010-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#include "toku_config.h"
#include "test.h"
#include "toku_pthread.h"
#include <db.h>
#include <sys/stat.h>
#include "key-val.h"

enum {NUM_INDEXER_INDEXES=1};
static const int NUM_DBS = NUM_INDEXER_INDEXES + 1; // 1 for source DB
static const int NUM_ROWS = 1000000;
int num_rows;
typedef enum {FORWARD = 0, BACKWARD} Direction;
typedef enum {TXN_NONE = 0, TXN_CREATE = 1, TXN_END = 2} TxnWork;

DB_ENV *env;

/*
 *  client scans the primary table (like a range query)
 */

static void * client(void *arg)
{
    DB *src = (DB *)arg;
    if ( verbose ) printf("client start\n");

    int r;
    struct timeval start, now;
    DB_TXN *txn;
    DBT key, val;
    uint32_t k, v;

    dbt_init(&key, &k, sizeof(unsigned int));
    dbt_init(&val, &v, sizeof(unsigned int));
    
    r = env->txn_begin(env, NULL, &txn, 0); CKERR(r);
    
    DBC *cursor;
    r = src->cursor(src, txn, &cursor, 0);  CKERR(r);

    int row = 0;

    gettimeofday(&start,0);
    while ( r != DB_NOTFOUND ) {
        r = cursor->c_get(cursor, &key, &val, DB_NEXT);
        if ( r != DB_NOTFOUND ) {
            row++;
        }
    }
    gettimeofday(&now, 0);
    if ( verbose ) printf("client : primary table scanned in %d sec, contains %d rows\n",
                          (int)(now.tv_sec - start.tv_sec),
                          row);

    r = cursor->c_close(cursor); CKERR(r);
    r = txn->commit(txn, 0); CKERR(r);
    if ( verbose ) printf("client done\n");

    return NULL;
}

toku_pthread_t *client_thread;
static void client_init(void) 
{
    client_thread = (toku_pthread_t *)toku_malloc(sizeof(toku_pthread_t));
}

static void client_cleanup(void) 
{
    toku_free(client_thread);   client_thread = NULL;
}

static void query_only(DB *src) 
{
    int r;
    void *t0;
    client_init();

    // start thread doing query
    r = toku_pthread_create(client_thread, 0, client, (void *)src);  
    CKERR(r);

    r = toku_pthread_join(*client_thread, &t0);     
    CKERR(r);
    
    client_cleanup();
}

static void test_indexer(DB *src, DB **dbs)
{
    int r;
    DB_TXN    *txn;
    DB_INDEXER *indexer;
    uint32_t db_flags[NUM_DBS];

    if ( verbose ) printf("test_indexer\n");
    for(int i=0;i<NUM_DBS;i++) { 
        db_flags[i] = 0; 
    }

    client_init();

    // create and initialize indexer
    r = env->txn_begin(env, NULL, &txn, 0);                                                               
    CKERR(r);

    if ( verbose ) printf("test_indexer create_indexer\n");
    r = env->create_indexer(env, txn, &indexer, src, NUM_DBS-1, &dbs[1], db_flags, 0);
    CKERR(r);
    r = indexer->set_error_callback(indexer, NULL, NULL);
    CKERR(r);
    r = indexer->set_poll_function(indexer, poll_print, NULL);
    CKERR(r);

    // start thread doing query
    r = toku_pthread_create(client_thread, 0, client, (void *)src);  CKERR(r);

    struct timeval start, now;
    if ( verbose ) {
        printf("test_indexer build\n");
        gettimeofday(&start,0);
    }
    r = indexer->build(indexer);
    CKERR(r);
    if ( verbose ) {
        gettimeofday(&now,0);
        int duration = (int)(now.tv_sec - start.tv_sec);
        if ( duration > 0 )
            printf("test_indexer build : sec = %d\n", duration);
    }

    void *t0;
    r = toku_pthread_join(*client_thread, &t0);     CKERR(r);

    if ( verbose ) printf("test_indexer close\n");
    r = indexer->close(indexer);
    CKERR(r);
    r = txn->commit(txn, DB_TXN_SYNC);
    CKERR(r);

    client_cleanup();
}


static void run_test(void) 
{
    int r;
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    r = toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO);                          CKERR(r);
    char logname[TOKU_PATH_MAX+1];
    r = toku_os_mkdir(toku_path_join(logname, 2, TOKU_TEST_FILENAME, "log"), S_IRWXU+S_IRWXG+S_IRWXO);                   CKERR(r);

    r = db_env_create(&env, 0);                                                  CKERR(r);
    r = env->set_redzone(env, 0);                                                CKERR(r);
    r = env->set_lg_dir(env, "log");                                             CKERR(r);
    r = env->set_default_bt_compare(env, uint_dbt_cmp);                           CKERR(r);
    generate_permute_tables();
    r = env->set_generate_row_callback_for_put(env, put_multiple_generate);      CKERR(r);
    int envflags = DB_INIT_LOCK | DB_INIT_LOG | DB_INIT_MPOOL | DB_INIT_TXN | DB_CREATE | DB_PRIVATE | DB_INIT_LOG;
    r = env->open(env, TOKU_TEST_FILENAME, envflags, S_IRWXU+S_IRWXG+S_IRWXO);               CKERR(r);
    env->set_errfile(env, stderr);
    r = env->checkpointing_set_period(env, 0);                                   CKERR(r);

    DBT desc;
    dbt_init(&desc, "foo", sizeof("foo"));
    int ids[MAX_DBS];
    DB *dbs[MAX_DBS];
    for (int i = 0; i < NUM_DBS; i++) {
        ids[i] = i;
        r = db_create(&dbs[i], env, 0); CKERR(r);
        dbs[i]->app_private = &ids[i];
        char key_name[32]; 
        sprintf(key_name, "key%d", i);
        r = dbs[i]->open(dbs[i], NULL, key_name, NULL, DB_BTREE, DB_AUTO_COMMIT|DB_CREATE, 0666);   CKERR(r);
        IN_TXN_COMMIT(env, NULL, txn_desc, 0, {
                { int chk_r = dbs[i]->change_descriptor(dbs[i], txn_desc, &desc, 0); CKERR(chk_r); }
        });
    }

    // generate the src DB (do not use put_multiple)
    DB_TXN *txn;
    r = env->txn_begin(env, NULL, &txn, 0);                                      CKERR(r);
    r = generate_initial_table(dbs[0], txn, num_rows);                           CKERR(r);
    r = txn->commit(txn, DB_TXN_SYNC);                                           CKERR(r);

    // scan the whole table twice to reduce possible flattening effects
    // -------------------------- //
    query_only(dbs[0]);
    query_only(dbs[0]);
    // -------------------------- //

    // scan the whole table while running the indexer
    // -------------------------- //
    test_indexer(dbs[0], dbs);
    // -------------------------- //

    // scan the whole table again to confirm performance
    // -------------------------- //
    query_only(dbs[0]);
    // -------------------------- //

    for(int i=0;i<NUM_DBS;i++) {
        r = dbs[i]->close(dbs[i], 0);                                            CKERR(r);
    }
    r = env->close(env, 0);                                                      CKERR(r);

    if ( verbose && (r == 0)) printf("PASS\n");
}

// ------------ infrastructure ----------

static inline void
do_args (int argc, char * const argv[]) {
    const char *progname=argv[0];
    num_rows = NUM_ROWS;
    argc--; argv++;
    while (argc>0) {
	if (strcmp(argv[0],"-v")==0) {
	    verbose++;
	} else if (strcmp(argv[0],"-q")==0) {
	    verbose=0;
        } else if (strcmp(argv[0],"-r")==0) {
            argc--; argv++;
            num_rows = atoi(argv[0]);
	} else {
	    fprintf(stderr, "Usage:\n %s [-v] [-q] [-r rows]\n", progname);
	    exit(1);
	}
	argc--; argv++;
    }
}


int test_main(int argc, char * const *argv) {
    do_args(argc, argv);
    run_test();
    return 0;
}
