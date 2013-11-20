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

#ident "Copyright (c) 2010-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"


/*********************
 *
 * Purpose is to preload a set of dictionaries using nested transactions,
 * to be used to test version upgrade.
 *
 * Each row will be inserted using nested transactions MAXDEPTH deep.
 * Each nested transaction will insert a value one greater than the parent transaction.
 * For each row, a single transaction will be aborted, the rest will be committed.
 * The transaction to be aborted will be the row number mod MAXDEPTH.
 * So, for row 0, the outermost transaction will be aborted and the row will not appear in the database.
 * For row 1, transaction 1 will be aborted, so the inserted value will be the original generated value.
 * For each row, the inserted value will be:
 *   if row%MAXDEPTH == 0 no row
 *   else value = generated value + (row%MAXDEPTH -1)
 * 
 *
 * For each row
 *   generate k,v pair
 *   for txndepth = 0 to MAXDEPTH-1 {
 *     add txndepth to v
 *     begin txn
 *     insert
 *     if txndepth = row%MAXDEPTH abort
 *     else commit
 *   }
 * }
 *
 */





#define kv_pair_funcs 1 // pull in kv_pair generators from test.h

#include "test.h"
#include "toku_pthread.h"
#include <db.h>
#include <sys/stat.h>
#include "ydb-internal.h"

#include "test_kv_gen.h"
/*
 */

DB_ENV *env;
enum {MAX_NAME=128};
enum {ROWS_PER_TRANSACTION=10000};
uint NUM_DBS=1;
uint NUM_ROWS=100000;
int CHECK_RESULTS=0;
int optimize=0;
int littlenode = 0;
enum { old_default_cachesize=1024 }; // MB
int CACHESIZE=old_default_cachesize;
int ALLOW_DUPS=0;

// max depth of nested transactions for this test
//#define MAXDEPTH 128
#define MAXDEPTH 64

static void
nested_insert(DB ** dbs, uint depth,  DB_TXN *parent_txn, uint k, uint generated_value);


static void
check_results_nested(DB ** dbs, const uint num_rows) {
    int num_dbs = 1;  // maybe someday increase
    for(int j=0;j<num_dbs;j++){
        DBT key, val;
        unsigned int k=0, v=0;
        dbt_init(&key, &k, sizeof(unsigned int));
        dbt_init(&val, &v, sizeof(unsigned int));
        int r;

        DB_TXN *txn;
        r = env->txn_begin(env, NULL, &txn, 0);
        CKERR(r);

        DBC *cursor;
        r = dbs[j]->cursor(dbs[j], txn, &cursor, 0);
        CKERR(r);
        for(uint i=0;i<num_rows;i++) {
            if (i % MAXDEPTH) {
		r = cursor->c_get(cursor, &key, &val, DB_NEXT);    
		CKERR(r);
		uint observed_k = *(unsigned int*)key.data;
		uint observed_v = *(unsigned int*)val.data;
		uint expected_k = i;
		uint generated_value = generate_val(i, 0);
		uint expected_v = generated_value + (i%MAXDEPTH - 1);
		if (verbose >= 3)
		    printf("expected key %d, observed key %d, expected val %d, observed val %d\n", 
			   expected_k, observed_k, expected_v, observed_v);
		// test that we have the expected keys and values
		assert(observed_k == expected_k);
		assert(observed_v == expected_v);
	    }
            dbt_init(&key, NULL, sizeof(unsigned int));
            dbt_init(&val, NULL, sizeof(unsigned int));
	    if ( verbose && (i%10000 == 0)) {printf("."); fflush(stdout);}
        }
        r = cursor->c_close(cursor);
        CKERR(r);
        r = txn->commit(txn, DB_TXN_NOSYNC);
        CKERR(r);
    }
    if ( verbose ) {printf("ok");fflush(stdout);}
}






static struct timeval starttime;
static double UU() elapsed_time (void) {
    struct timeval now;
    gettimeofday(&now, NULL);
    return now.tv_sec - starttime.tv_sec + 1e-6*(now.tv_usec - starttime.tv_usec);
}

static void preload_dbs(DB **dbs)
{
    gettimeofday(&starttime, NULL);
    uint row;

    if ( verbose ) { printf("loading");fflush(stdout); }

    for(row = 0; row <= NUM_ROWS; row++) {
	uint generated_value = generate_val(row, 0);
	nested_insert(dbs, 0, NULL, row, generated_value);
    }

    if (optimize) {
        if (verbose) { printf("\noptimizing");fflush(stdout);}
        do_hot_optimize_on_dbs(env, dbs, 1);
    }

    if ( CHECK_RESULTS) {
	if ( verbose ) {printf("\nchecking");fflush(stdout);}
	check_results_nested(&dbs[0], NUM_ROWS);
    }
    if ( verbose) {printf("\ndone\n");fflush(stdout);}
}

static void
nested_insert(DB ** dbs, uint depth,  DB_TXN *parent_txn, uint k, uint generated_value) {
    if (depth < MAXDEPTH) {
	DBT key, val;
	dbt_init_realloc(&key);
	dbt_init_realloc(&val);
	uint v = generated_value + depth;
	DB_TXN * txn;
        int r = env->txn_begin(env, parent_txn, &txn, 0);
	CKERR(r);
	dbt_init(&key, &k, sizeof(unsigned int));
	dbt_init(&val, &v, sizeof(unsigned int));
	int db = 0;  // maybe later replace with loop
	r = dbs[db]->put(dbs[db], txn, &key, &val, 0);                                               
	CKERR(r);
	if (key.flags == 0) { dbt_init_realloc(&key); }
	if (val.flags == 0) { dbt_init_realloc(&val); }
	nested_insert(dbs, depth+1, txn, k, generated_value);
	if (depth == (k % MAXDEPTH)) {
	    r = txn->abort(txn);
	    CKERR(r);
	    if (verbose>=3)
		printf("abort k = %d, v= %d, depth = %d\n", k, v, depth);
	}
	else {    
	    r = txn->commit(txn, DB_TXN_NOSYNC);
	    CKERR(r);
	    if (verbose>=3)
		printf("commit k = %d, v= %d, depth = %d\n", k, v, depth);
	}
        if ( verbose && (k%10000 == 0)) {printf(".");fflush(stdout);}

	if ( key.flags ) { toku_free(key.data); key.data = NULL; }
	if ( val.flags ) { toku_free(val.data); key.data = NULL; }
    }
}


char *free_me = NULL;
const char *env_dir = TOKU_TEST_FILENAME; // the default env_dir.

static void run_test(void) 
{
    int r;
    {
	int len = strlen(env_dir) + 20;
	char syscmd[len];
	r = snprintf(syscmd, len, "rm -rf %s", env_dir);
	assert(r<len);
	r = system(syscmd);                                                                                   CKERR(r);
    }
    r = toku_os_mkdir(env_dir, S_IRWXU+S_IRWXG+S_IRWXO);                                                      CKERR(r);

    r = db_env_create(&env, 0);                                                                               CKERR(r);
    r = env->set_default_bt_compare(env, uint_dbt_cmp);                                                       CKERR(r);
//    r = env->set_default_dup_compare(env, uint_dbt_cmp);                                                      CKERR(r);
//    if ( verbose ) printf("CACHESIZE = %d MB\n", CACHESIZE);
//    r = env->set_cachesize(env, CACHESIZE / 1024, (CACHESIZE % 1024)*1024*1024, 1);                           CKERR(r);
//    CKERR(r);
    int envflags = DB_INIT_LOCK | DB_INIT_LOG | DB_INIT_MPOOL | DB_INIT_TXN | DB_CREATE | DB_PRIVATE;
    r = env->open(env, env_dir, envflags, S_IRWXU+S_IRWXG+S_IRWXO);                                            CKERR(r);
    env->set_errfile(env, stderr);
    r = env->checkpointing_set_period(env, 0);                                                                CKERR(r);

    DBT desc;
    dbt_init(&desc, "foo", sizeof("foo"));
    char name[MAX_NAME*2];

    DB **dbs = (DB**)toku_malloc(sizeof(DB*) * NUM_DBS);
    assert(dbs != NULL);
    int idx[MAX_DBS];
    for(uint i=0;i<NUM_DBS;i++) {
        idx[i] = i;
        r = db_create(&dbs[i], env, 0);                                                                       CKERR(r);
	if (littlenode) {
	    r=dbs[i]->set_pagesize(dbs[i], 4096);
	    CKERR(0);	    
	}
        dbs[i]->app_private = &idx[i];
        snprintf(name, sizeof(name), "db_%04x", i);
        r = dbs[i]->open(dbs[i], NULL, name, NULL, DB_BTREE, DB_CREATE, 0666);                                CKERR(r);
        IN_TXN_COMMIT(env, NULL, txn_desc, 0, {
                { int chk_r = dbs[i]->change_descriptor(dbs[i], txn_desc, &desc, 0); CKERR(chk_r); }
        });
    }

    generate_permute_tables();

    // -------------------------- //
    preload_dbs(dbs);
    // -------------------------- //

    for(uint i=0;i<NUM_DBS;i++) {
        r = dbs[i]->close(dbs[i], 0);                                                                         CKERR(r);
        dbs[i] = NULL;
    }

    if (verbose >= 2)
	print_engine_status(env);
    r = env->close(env, 0);                                                                                   CKERR(r);
    toku_free(dbs);

}

// ------------ infrastructure ----------
static void do_args(int argc, char * const argv[]);

int test_main(int argc, char * const argv[]) {
    do_args(argc, argv);
    run_test();
    if (free_me) toku_free(free_me);
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
	    fprintf(stderr, "Usage: -h -c -n -d <num_dbs> -r <num_rows> %s\n", cmd);
	    exit(resultcode);
        } else if (strcmp(argv[0], "-d")==0) {
            argc--; argv++;
            NUM_DBS = atoi(argv[0]);
            if ( NUM_DBS > MAX_DBS ) {
                fprintf(stderr, "max value for -d field is %d\n", MAX_DBS);
                resultcode=1;
                goto do_usage;
            }
        } else if (strcmp(argv[0], "-r")==0) {
            argc--; argv++;
            NUM_ROWS = atoi(argv[0]);
        } else if (strcmp(argv[0], "-c")==0) {
            CHECK_RESULTS = 1;
        } else if (strcmp(argv[0], "-n")==0) {
            littlenode = 1;
        } else if (strcmp(argv[0], "-o")==0) {
            optimize = 1;
	} else {
	    fprintf(stderr, "Unknown arg: %s\n", argv[0]);
	    resultcode=1;
	    goto do_usage;
	}
	argc--;
	argv++;
    }
}
