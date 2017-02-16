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


// Purpose of this test is to verify that a dictionary created by the 4.2.0 
// loader can be properly read with 5.0.
// This file was derived from the 4.2.0 version of loader-stress-test.c,
// which was used to create the dictionary. 
// This test only reads (and upgrades) the dictionary, it does not load it.

// Need to use malloc for the malloc instrumentation tests
#define TOKU_ALLOW_DEPRECATED

#include "test.h"
#include "toku_pthread.h"
#include <db.h>
#include <sys/stat.h>
#include "ydb-internal.h"

DB_ENV *env;
enum {MAX_NAME=128};
enum {MAX_DBS=256};
int NUM_DBS=1;
int NUM_ROWS=250000;
int CHECK_RESULTS=0;
int USE_PUTS=0;
enum { old_default_cachesize=1024 }; // MB
int CACHESIZE=old_default_cachesize;
int ALLOW_DUPS=0;
enum {MAGIC=311};
char *datadir = NULL;
bool check_est = true; // do check the estimates by default
bool footprint_print = false; // print memory footprint info 


// Code for showing memory footprint information.
pthread_mutex_t my_lock = PTHREAD_MUTEX_INITIALIZER;
size_t hiwater;
size_t water;
size_t hiwater_start;
static long long mcount = 0, fcount=0;


size_t malloc_usable_size(void *p);

static void my_free(void*p) {
    if (p) {
	water-=malloc_usable_size(p);
    }
    free(p);
}

static void *my_malloc(size_t size) {
    void *r = malloc(size);
    if (r) {
	water += malloc_usable_size(r);
	if (water>hiwater) hiwater=water;
    }
    return r;
}

static void *my_realloc(void *p, size_t size) {
    size_t old_usable = p ? malloc_usable_size(p) : 0;
    void *r = realloc(p, size);
    if (r) {
	water -= old_usable;
	water += malloc_usable_size(r);
    }
    return r;
}

//
//   Functions to create unique key/value pairs, row generators, checkers, ... for each of NUM_DBS
//

//   a is the bit-wise permute table.  For DB[i], permute bits as described in a[i] using 'twiddle32'
// inv is the inverse bit-wise permute of a[].  To get the original value from a twiddled value, twiddle32 (again) with inv[]
int   a[MAX_DBS][32];
int inv[MAX_DBS][32];


// rotate right and left functions
static inline unsigned int rotr32(const unsigned int x, const unsigned int num) {
    const unsigned int n = num % 32;
    return (x >> n) | ( x << (32 - n));
}
static inline unsigned int rotl32(const unsigned int x, const unsigned int num) {
    const unsigned int n = num % 32;
    return (x << n) | ( x >> (32 - n));
}

static void generate_permute_tables(void) {
    int i, j, tmp;
    for(int db=0;db<MAX_DBS;db++) {
        for(i=0;i<32;i++) {
            a[db][i] = i;
        }
        for(i=0;i<32;i++) {
            j = random() % (i + 1);
            tmp = a[db][j];
            a[db][j] = a[db][i];
            a[db][i] = tmp;
        }
//        if(db < NUM_DBS){ printf("a[%d] = ", db); for(i=0;i<32;i++) { printf("%2d ", a[db][i]); } printf("\n");}
        for(i=0;i<32;i++) {
            inv[db][a[db][i]] = i;
        }
    }
}

// permute bits of x based on inverse permute table bitmap
static unsigned int inv_twiddle32(unsigned int x, int db)
{
    unsigned int b = 0;
    for(int i=0;i<32;i++) {
        b |= (( x >> i ) & 1) << inv[db][i];
    }
    return b;
}


static unsigned int pkey_for_val(int key, int i) {
    return rotr32(key, i) - MAGIC;
}


static void check_results(DB **dbs)
{
    for(int j=0;j<NUM_DBS;j++){
        DBT key, val;
        unsigned int k=0, v=0;
        dbt_init(&key, &k, sizeof(unsigned int));
        dbt_init(&val, &v, sizeof(unsigned int));
        int r;
        unsigned int pkey_for_db_key;

        DB_TXN *txn;
        r = env->txn_begin(env, NULL, &txn, 0);
        CKERR(r);

        DBC *cursor;
        r = dbs[j]->cursor(dbs[j], txn, &cursor, 0);
        CKERR(r);
        for(int i=0;i<NUM_ROWS;i++) {
            r = cursor->c_get(cursor, &key, &val, DB_NEXT);    
            CKERR(r);
            k = *(unsigned int*)key.data;
            pkey_for_db_key = (j == 0) ? k : inv_twiddle32(k, j);
            v = *(unsigned int*)val.data;
            // test that we have the expected keys and values
            assert((unsigned int)pkey_for_db_key == (unsigned int)pkey_for_val(v, j));
//            printf(" DB[%d] key = %10u, val = %10u, pkey_for_db_key = %10u, pkey_for_val=%10d\n", j, v, k, pkey_for_db_key, pkey_for_val(v, j));
        }
        if (verbose) {
	    printf("."); 
	    fflush(stdout);
	}
        r = cursor->c_close(cursor);
        CKERR(r);
        r = txn->commit(txn, 0);
        CKERR(r);
    }
    if (verbose)
	printf("\nCheck OK\n");
}

static void *expect_poll_void = &expect_poll_void;

static struct progress_info {
    double time;
    double progress;
} *progress_infos=NULL;
static int progress_infos_count=0;

static void test_loader(DB **dbs)
{
    int r;
    DB_TXN    *txn;
    

    // this is the essential part of the upgrade test
    check_results(dbs);

    for (int i=0; i<NUM_DBS; i++) {
	r = env->txn_begin(env, NULL, &txn, 0);                                                               
	CKERR(r);
	DB_BTREE_STAT64 stats;
	r = dbs[i]->stat64(dbs[i], txn, &stats);
	CKERR(r);
	if (verbose)
	    printf("n_keys=%" PRIu64 " n_data=%" PRIu64 " dsize=%" PRIu64 " fsize=%" PRIu64 "\n",
		   stats.bt_nkeys, stats.bt_ndata, stats.bt_dsize, stats.bt_fsize);
	assert(stats.bt_nkeys == (uint64_t)NUM_ROWS);
	assert(stats.bt_ndata == (uint64_t)NUM_ROWS);
	assert(stats.bt_dsize == ((uint64_t)NUM_ROWS) * 2 * sizeof(unsigned int));
	r = txn->commit(txn, 0);
	CKERR(r);
    }
}


char *free_me = NULL;
char *env_dir = TOKU_TEST_FILENAME; // the default env_dir.
char *tmp_subdir = "tmp.subdir";

#define OLDDATADIR "../../../../tokudb.data/"
char *db_v4_dir        = OLDDATADIR "env_preload.4.2.0.loader250kd1.cleanshutdown";


static void setup(void) {
    int r;
    int len = 256;
    char syscmd[len];
    char * src_db_dir;

    src_db_dir = db_v4_dir;

    r = snprintf(syscmd, len, "rm -rf %s", env_dir);
    assert(r<len);
    r = system(syscmd);                                                                                  
    CKERR(r);
    
    r = snprintf(syscmd, len, "cp -r %s %s", src_db_dir, env_dir);
    assert(r<len);
    r = system(syscmd);                                                                                 
    CKERR(r);
}




static void run_test(void) 
{
    int r;

    setup();
    {
	char len = strlen(env_dir) + strlen(tmp_subdir) + 10;
	char tmpdir[len];
	r = snprintf(tmpdir, len, "%s/%s", env_dir, tmp_subdir);
	assert(r<len);
	r = db_env_create(&env, 0);                                                                           CKERR(r);
	r = env->set_tmp_dir(env, tmp_subdir);                                                                CKERR(r);
    }
    r = env->set_default_bt_compare(env, uint_dbt_cmp);                                                       CKERR(r);
    if ( verbose ) printf("CACHESIZE = %d MB\n", CACHESIZE);
    r = env->set_cachesize(env, CACHESIZE / 1024, (CACHESIZE % 1024)*1024*1024, 1);                           CKERR(r);
    if (datadir) {
        r = env->set_data_dir(env, datadir);                                                                  CKERR(r);
    }
    int envflags = DB_INIT_LOCK | DB_INIT_LOG | DB_INIT_MPOOL | DB_INIT_TXN | DB_CREATE | DB_PRIVATE;
    r = env->open(env, env_dir, envflags, S_IRWXU+S_IRWXG+S_IRWXO);                                            CKERR(r);
    env->set_errfile(env, stderr);
    r = env->checkpointing_set_period(env, 60);                                                                CKERR(r);

    DBT desc;
    dbt_init(&desc, "foo", sizeof("foo"));
    char name[MAX_NAME*2];

    DB **dbs = (DB**)toku_malloc(sizeof(DB*) * NUM_DBS);
    assert(dbs != NULL);
    int idx[MAX_DBS];
    for(int i=0;i<NUM_DBS;i++) {
        idx[i] = i;
        r = db_create(&dbs[i], env, 0);                                                                       CKERR(r);
        dbs[i]->app_private = &idx[i];
        snprintf(name, sizeof(name), "db_%04x", i);
        r = dbs[i]->open(dbs[i], NULL, name, NULL, DB_BTREE, DB_CREATE, 0666);                                CKERR(r);
        IN_TXN_COMMIT(env, NULL, txn_desc, 0, {
                { int chk_r = dbs[i]->change_descriptor(dbs[i], txn_desc, &desc, 0); CKERR(chk_r); }
        });
    }

    generate_permute_tables();

    // -------------------------- //
    test_loader(dbs);
    // -------------------------- //

    for(int i=0;i<NUM_DBS;i++) {
        dbs[i]->close(dbs[i], 0);                                                                             CKERR(r);
        dbs[i] = NULL;
    }
    if (verbose >= 2)
	print_engine_status(env);
    r = env->close(env, 0);                                                                                   CKERR(r);
    toku_free(dbs);
}


// ------------ infrastructure ----------
static void do_args(int argc, char * const argv[]);

int test_main(int argc, char * const *argv) {
    do_args(argc, argv);

    run_test();
    if (free_me) toku_free(free_me);

    if (progress_infos) {
	if (verbose>=2) {
	    double ratio=progress_infos[progress_infos_count-1].time/progress_infos[progress_infos_count-1].progress;
	    printf("Progress ratios:\n");
	    for (int i=0; i<progress_infos_count; i++) {
		printf(" %5.3f\n", (progress_infos[i].time/progress_infos[i].progress)/ratio);
	    }
	}
	toku_free(progress_infos);
    }
    if (footprint_print) {
	printf("%s:%d Hiwater=%ld water=%ld (extra hiwater=%ldM) mcount=%lld fcount=%lld\n", __FILE__, __LINE__, hiwater, water, (hiwater-hiwater_start)/(1024*1024), mcount, fcount);
	extern void malloc_stats(void);
	malloc_stats();
    }
    return 0;
}

static void do_args(int argc, char * const argv[]) {

    // Must look for "-f" right away before we malloc anything.
    for (int i=1; i<argc; i++)  {

	if (strcmp(argv[i], "-f")) {
	    db_env_set_func_malloc(my_malloc);
	    db_env_set_func_realloc(my_realloc);
	    db_env_set_func_free(my_free);
	}
    }

    int resultcode;
    char *cmd = argv[0];
    argc--; argv++;
    
    CACHESIZE = (toku_os_get_phys_memory_size() / (1024*1024))/2; //MB

    while (argc>0) {
	if (strcmp(argv[0], "-v")==0) {
	    verbose++;
	} else if (strcmp(argv[0],"-q")==0) {
	    verbose--;
	    if (verbose<0) verbose=0;
        } else if (strcmp(argv[0], "-h")==0) {
	    resultcode=0;
	do_usage:
	    fprintf(stderr, "Usage: -h -d <num_dbs> -r <num_rows> [-m <megabytes>] [-M]\n%s\n", cmd);
	    fprintf(stderr, "  where -d <num_dbs>     is the number of dictionaries to build (primary & secondary).  (Default=%d)\n", NUM_DBS);
	    fprintf(stderr, "        -m <m>           use m MB of memory for the cachetable (default is %d MB)\n", CACHESIZE);
            fprintf(stderr, "        -M               use %d MB of memory for the cachetable\n", old_default_cachesize);
	    fprintf(stderr,  "        -f               print memory footprint information at various points in the load\n");
	    exit(resultcode);
        } else if (strcmp(argv[0], "-d")==0) {
            argc--; argv++;
            NUM_DBS = atoi(argv[0]);
            if ( NUM_DBS > MAX_DBS ) {
                fprintf(stderr, "max value for -d field is %d\n", MAX_DBS);
                resultcode=1;
                goto do_usage;
            }
        } else if (strcmp(argv[0], "-v")==0) {
	    verbose++;
	} else if (strcmp(argv[0],"-q")==0) {
	    verbose--;
	    if (verbose<0) verbose=0;
	} else if (strcmp(argv[0], "-f")==0) {
	    footprint_print = true;
        } else if (strcmp(argv[0], "-r")==0) {
            argc--; argv++;
            NUM_ROWS = atoi(argv[0]);
        } else if (strcmp(argv[0], "-m")==0) {
            argc--; argv++;
            CACHESIZE = atoi(argv[0]);
        } else if (strcmp(argv[0], "-M")==0) {
	    CACHESIZE = old_default_cachesize;
	} else {
	    fprintf(stderr, "Unknown arg: %s\n", argv[0]);
	    resultcode=1;
	    goto do_usage;
	}
	argc--;
	argv++;
    }
}
