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

#ident "Copyright (c) 2010-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

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
int NUM_DBS=5;
int NUM_ROWS=100000;
int CHECK_RESULTS=0;
int optimize=0;
int littlenode = 0;
enum { old_default_cachesize=1024 }; // MB
int CACHESIZE=old_default_cachesize;
int ALLOW_DUPS=0;

static struct timeval starttime;
static double UU() elapsed_time (void) {
    struct timeval now;
    gettimeofday(&now, NULL);
    return now.tv_sec - starttime.tv_sec + 1e-6*(now.tv_usec - starttime.tv_usec);
}

static void preload_dbs(DB **dbs)
{
    gettimeofday(&starttime, NULL);
    int r;
    DB_TXN    *txn;

    DBT skey, sval;
    DBT key, val;
    dbt_init_realloc(&key);
    dbt_init_realloc(&val);
    unsigned int k, v;
    if ( verbose ) { printf("loading");fflush(stdout); }
    int outer_loop_num = ( NUM_ROWS <= ROWS_PER_TRANSACTION ) ? 1 : (NUM_ROWS / ROWS_PER_TRANSACTION);
    for(int x=0;x<outer_loop_num;x++) {
        r = env->txn_begin(env, NULL, &txn, 0);                                                              CKERR(r);
        for(int i=1;i<=ROWS_PER_TRANSACTION;i++) {
            k = i + (x*ROWS_PER_TRANSACTION);
            v = generate_val(k, 0);
            dbt_init(&skey, &k, sizeof(unsigned int));
            dbt_init(&sval, &v, sizeof(unsigned int));

            for(int db = 0;db < NUM_DBS;db++) {
                put_multiple_generate(dbs[db], // dest_db
                                      NULL, // src_db, ignored
                                      &key, &val, // dest_key, dest_val
                                      &skey, &sval, // src_key, src_val
                                      NULL); // extra, ignored

                r = dbs[db]->put(dbs[db], txn, &key, &val, 0);                                               CKERR(r);
                if (key.flags == 0) { dbt_init_realloc(&key); }
                if (val.flags == 0) { dbt_init_realloc(&val); }
            }
        }
        r = txn->commit(txn, 0);                                                                             CKERR(r);
        if ( verbose ) {printf(".");fflush(stdout);}
    }
    if ( key.flags ) { toku_free(key.data); key.data = NULL; }
    if ( val.flags ) { toku_free(val.data); key.data = NULL; }

    if (optimize) {
        if (verbose) { printf("\noptimizing");fflush(stdout);}
        do_hot_optimize_on_dbs(env, dbs, NUM_DBS);
    }

    if ( CHECK_RESULTS) {
        if ( verbose ) {printf("\nchecking");fflush(stdout);}
        check_results(env, dbs, NUM_DBS, NUM_ROWS);
    }
    if ( verbose) {printf("\ndone\n");fflush(stdout);}
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
//    r = env->set_default_bt_compare(env, uint_dbt_cmp);                                                       CKERR(r);
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
    for(int i=0;i<NUM_DBS;i++) {
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

    for(int i=0;i<NUM_DBS;i++) {
        r = dbs[i]->close(dbs[i], 0);                                                                         CKERR(r);
        dbs[i] = NULL;
    }

    if (verbose >= 2)
	print_engine_status(env);
    r = env->close(env, 0);                                                                                   CKERR(r);
    toku_free(dbs);

    /*********** DO NOT TRIM LOGFILES: Trimming logfiles defeats purpose of upgrade tests which must handle untrimmed logfiles.
    // reopen, then close environment to trim logfiles
    r = db_env_create(&env, 0);                                                                               CKERR(r);
    r = env->open(env, env_dir, envflags, S_IRWXU+S_IRWXG+S_IRWXO);                                           CKERR(r);
    r = env->close(env, 0);                                                                                   CKERR(r);
    ***********/

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
