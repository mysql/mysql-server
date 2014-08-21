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


/**************
 *
 * NOTE: This test is used for upgrade testing as well as for exercising the loader.
 *       Changes should not be made gratuitously.
 *       The 4.2.0 version of this test was used to create many of the preloaded
 *       environments in the <svn-top>/tokudb/tokudb.data directory.
 */


// Need to use malloc for the malloc instrumentation tests
#ifndef TOKU_ALLOW_DEPRECATED
#define TOKU_ALLOW_DEPRECATED
#endif

#include "test.h"
#include "toku_pthread.h"
#include <db.h>
#include <sys/stat.h>
#include "ydb-internal.h"
#include <memory.h>
#include <dlfcn.h>

DB_ENV *env;
enum {MAX_NAME=128};
enum {MAX_DBS=1024};
int NUM_DBS=5;
int NUM_ROWS=100000;
int VALSIZE=sizeof(unsigned int);
int CHECK_RESULTS=0;
int DISALLOW_PUTS=0;
int COMPRESS=0;
enum { old_default_cachesize=1024 }; // MB
int CACHESIZE=old_default_cachesize;
int ALLOW_DUPS=0;
enum {MAGIC=311};
char *datadir = NULL;
bool check_est = true; // do check the estimates by default
bool footprint_print = false; // print memory footprint info 
bool upgrade_test = false;   

// Code for showing memory footprint information.
pthread_mutex_t my_lock = PTHREAD_MUTEX_INITIALIZER;
size_t hiwater;
size_t water;
size_t hiwater_start;
static long long mcount = 0, fcount=0;

static void my_free(void*p) {
    if (p) {
        water-=toku_malloc_usable_size(p);
    }
    free(p);
}

static void *my_malloc(size_t size) {
    void *r = malloc(size);
    if (r) {
        water += toku_malloc_usable_size(r);
        if (water>hiwater) hiwater=water;
    }
    return r;
}

static void *my_realloc(void *p, size_t size) {
    size_t old_usable = p ? toku_malloc_usable_size(p) : 0;
    void *r = realloc(p, size);
    if (r) {
        water -= old_usable;
        water += toku_malloc_usable_size(r);
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

static const char *loader_temp_prefix = "tokuld"; // #2536

// return number of temp files
static int
count_temp(char * dirname) {
    int n = 0;
    
    DIR * dir = opendir(dirname);
    
    struct dirent *ent;
    while ((ent=readdir(dir))) {
	if ((ent->d_type==DT_REG || ent->d_type==DT_UNKNOWN) && strncmp(ent->d_name, loader_temp_prefix, 6)==0) {
	    n++;
	    if (verbose) {
		printf("Temp files (%d)\n", n);
		printf("  %s/%s\n", dirname, ent->d_name);
	    } 
	}
    }
    closedir(dir);
    return n;
}

// rotate right and left functions
static inline unsigned int rotr32(const unsigned int x, const unsigned int num) {
    if (num == 0) {
        return x;
    } else {
        const unsigned int n = num % 32;
        return (x >> n) | ( x << (32 - n));
    }
}
static inline unsigned int rotl32(const unsigned int x, const unsigned int num) {
    if (num == 0) {
        return x;
    } else {
        const unsigned int n = num % 32;
        return (x << n) | ( x >> (32 - n));
    }
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

// permute bits of x based on permute table bitmap
static unsigned int twiddle32(unsigned int x, int db)
{
    unsigned int b = 0;
    for(int i=0;i<32;i++) {
        b |= (( x >> i ) & 1) << a[db][i];
    }
    return b;
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

// generate val from key, index
static void generate_val(int key, int i, unsigned int*v) {
    v[0] = rotl32((key + MAGIC), i);
    for (unsigned w = 1; w < VALSIZE/sizeof(unsigned int); w++) {
        v[w] = rotr32(v[w-1], 1);
    }
}

static unsigned int pkey_for_val(int key, int i) {
    return rotr32(key, i) - MAGIC;
}

// There is no handlerton in this test, so this function is a local replacement
// for the handlerton's generate_row_for_put().
static int put_multiple_generate(DB *dest_db, DB *src_db, DBT_ARRAY *dest_keys, DBT_ARRAY *dest_vals, const DBT *src_key, const DBT *UU(src_val)) {
    toku_dbt_array_resize(dest_keys, 1);
    toku_dbt_array_resize(dest_vals, 1);
    DBT *dest_key = &dest_keys->dbts[0];
    DBT *dest_val = &dest_vals->dbts[0];

    assert(src_db);
    assert(dest_db != src_db);

    uint32_t which = *(uint32_t*)dest_db->app_private;
    assert(which != 0);

    {
        assert(dest_key->flags==DB_DBT_REALLOC);
        if (dest_key->ulen < sizeof(unsigned int)) {
            dest_key->data = toku_xrealloc(dest_key->data, sizeof(unsigned int));
            dest_key->ulen = sizeof(unsigned int);
        }
        assert(dest_val->flags==DB_DBT_REALLOC);
        if (dest_val->ulen < (unsigned)VALSIZE) {
            dest_val->data = toku_xrealloc(dest_val->data, VALSIZE);
            dest_val->ulen = VALSIZE;
        }
        unsigned int *new_key = (unsigned int *)dest_key->data;

        *new_key = twiddle32(*(unsigned int*)src_key->data, which);
        generate_val(*(unsigned int*)src_key->data, which, (unsigned int*)dest_val->data);

        dest_key->size = sizeof(unsigned int);
        dest_val->size = VALSIZE;
        //data is already set above
    }

//    printf("dest_key.data = %d\n", *(int*)dest_key->data);
//    printf("dest_val.data = %d\n", *(int*)dest_val->data);

    return 0;
}

static int uint_cmp(const void *ap, const void *bp) {
    unsigned int an = *(unsigned int *)ap;
    unsigned int bn = *(unsigned int *)bp;
    if (an < bn) 
        return -1;
    if (an > bn)
        return +1;
    return 0;
}

static void check_results(DB **dbs) {
    // verify trees
    for (int j = 0;j < NUM_DBS; j++) {
        int r = dbs[j]->verify_with_progress(dbs[j], NULL, NULL, 0, 0);
        assert(r == 0);
    }

    // verify rows
    for (int j = 0;j < NUM_DBS; j++) {
        unsigned int prev_k = 0;

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

        // generate the expected keys
        unsigned int *expected_key = (unsigned int *) toku_malloc(NUM_ROWS * sizeof (unsigned int));
        for (int i = 0; i < NUM_ROWS; i++)
            expected_key[i] = j == 0 ? (unsigned int)(i+1) : twiddle32(i+1, j);
        // sort the keys
        qsort(expected_key, NUM_ROWS, sizeof (unsigned int), uint_cmp);

        unsigned int valcheck[VALSIZE/sizeof(unsigned int)];
        for (int i = 0; i < NUM_ROWS+1; i++) {
            r = cursor->c_get(cursor, &key, &val, DB_NEXT);
            if (DISALLOW_PUTS) {
                CKERR2(r, DB_NOTFOUND);
                break;
            }
            if (r == DB_NOTFOUND) {
                assert(i == NUM_ROWS); // check that there are exactly NUM_ROWS in the dictionary
                break;
            }
            CKERR(r);

            k = *(unsigned int*)key.data;

            unsigned int pkey_for_db_key = (j == 0) ? k : inv_twiddle32(k, j);
            v = *(unsigned int*)val.data;
            // test that we have the expected keys and values
            assert((unsigned int)pkey_for_db_key == (unsigned int)pkey_for_val(v, j));


//            printf(" DB[%d] key = %10u, val = %10u, pkey_for_db_key = %10u, pkey_for_val=%10d\n", j, v, k, pkey_for_db_key, pkey_for_val(v, j));

            // check the expected keys
            assert(k == expected_key[i]);
            generate_val(pkey_for_db_key, j, &valcheck[0]);
            assert(val.size == (unsigned)VALSIZE);
            assert(memcmp(val.data, &valcheck[0], VALSIZE)==0);

            // check prev_key < key
            if (i > 0) 
                assert(prev_k < k);

            // update prev = current
            prev_k = k;
        }

        toku_free(expected_key);

        if ( verbose ) {printf("."); fflush(stdout);}
        r = cursor->c_close(cursor);
        CKERR(r);

        r = txn->commit(txn, 0);
        CKERR(r);
    }
    if ( verbose ) printf("\nCheck OK\n");
}

static void *expect_poll_void = &expect_poll_void;
static uint64_t poll_count=0;
static uint64_t bomb_after_poll_count=UINT64_MAX;

static struct progress_info {
    double time;
    double progress;
} *progress_infos=NULL;
static int progress_infos_count=0;
static int progress_infos_limit=0;

// timing
static bool did_start=false;
static struct timeval start;

static int poll_function (void *extra, float progress) {
    if (verbose>=2) {
	assert(did_start);
	struct timeval now;
	gettimeofday(&now, 0);
	double elapsed = now.tv_sec - start.tv_sec + 1e-6*(now.tv_usec - start.tv_usec);
	printf("Progress: %6.6fs %5.1f%%\n", elapsed, progress*100);
	if (progress_infos_count>=progress_infos_limit) {
	    progress_infos_limit = 2*progress_infos_limit + 1;
	    XREALLOC_N(progress_infos_limit, progress_infos);
	}
	progress_infos[progress_infos_count++] = (struct progress_info){elapsed, progress};	    
    }
    assert(extra==expect_poll_void);
    assert(0.0<=progress && progress<=1.0);
    poll_count++; // Calls to poll_function() are protected by a lock, so we don't have to do this atomically.
    if (poll_count>bomb_after_poll_count)
	return TOKUDB_CANCELED;
    else
	return 0;
}

static struct timeval starttime;
static double elapsed_time (void) {
    struct timeval now;
    gettimeofday(&now, NULL);
    return now.tv_sec - starttime.tv_sec + 1e-6*(now.tv_usec - starttime.tv_usec);
}

static void test_loader(DB **dbs)
{
    gettimeofday(&starttime, NULL);
    int r;
    DB_TXN    *txn;
    DB_LOADER *loader;
    uint32_t db_flags[MAX_DBS];
    uint32_t dbt_flags[MAX_DBS];
    uint32_t flags = DB_NOOVERWRITE;
    if ( (DISALLOW_PUTS) && (ALLOW_DUPS == 1) ) flags = 0;
    for(int i=0;i<MAX_DBS;i++) { 
        db_flags[i] = flags;
        dbt_flags[i] = 0;
    }
    
    uint32_t loader_flags = DISALLOW_PUTS | COMPRESS; // set with -p option

    // create and initialize loader
    r = env->txn_begin(env, NULL, &txn, 0);
    CKERR(r);
    hiwater_start = hiwater;
    if (footprint_print)  printf("%s:%d Hiwater=%ld water=%ld\n", __FILE__, __LINE__, hiwater, water);
    r = env->create_loader(env, txn, &loader, dbs[0], NUM_DBS, dbs, db_flags, dbt_flags, loader_flags);
    CKERR(r);
    if (footprint_print)  printf("%s:%d Hiwater=%ld water=%ld\n", __FILE__, __LINE__, hiwater, water);
    r = loader->set_error_callback(loader, NULL, NULL);
    CKERR(r);
    r = loader->set_poll_function(loader, poll_function, expect_poll_void);
    CKERR(r);

    // using loader->put, put values into DB
    DBT key, val;
    unsigned int k;
    unsigned int v[VALSIZE/sizeof(unsigned int)];
    for(int i=1;i<=NUM_ROWS;i++) {
        k = i;
        generate_val(i, 0, &v[0]);
        dbt_init(&key, &k, sizeof(unsigned int));
        dbt_init(&val, &v[0], VALSIZE);
        r = loader->put(loader, &key, &val);
        if (DISALLOW_PUTS) {
            CKERR2(r, EINVAL);
        } else {
            CKERR(r);
        }
        if ( verbose) { if((i%10000) == 0){printf("."); fflush(stdout);} }
    }
    if ( verbose ) {printf("\n"); fflush(stdout);}        
        
    poll_count=0;

    int n = count_temp(env->i->real_data_dir);
    if (verbose) printf("Num temp files = %d\n", n);

    did_start = true;
    gettimeofday(&start, 0);

    // close the loader
    if ( verbose ) printf("%9.6fs closing\n", elapsed_time());
    if (footprint_print) printf("%s:%d Hiwater=%ld water=%ld\n", __FILE__, __LINE__, hiwater, water);
    r = loader->close(loader);
    if (footprint_print) printf("%s:%d Hiwater=%ld water=%ld (extra hiwater=%ldM)\n", __FILE__, __LINE__, hiwater, water, (hiwater-hiwater_start)/(1024*1024));
    if ( verbose ) printf("%9.6fs done\n",    elapsed_time());
    CKERR2s(r,0,TOKUDB_CANCELED);

    if (r==0) {
	if (!DISALLOW_PUTS) {
	    if (poll_count == 0) printf("%s:%d\n", __FILE__, __LINE__);
	    assert(poll_count>0);
	}

	r = txn->commit(txn, 0);
	CKERR(r);

	// verify the DBs
	if ( CHECK_RESULTS ) {
	    check_results(dbs);
	}

	if ( check_est ) {
	    for (int i=0; i<NUM_DBS; i++) {
		r = env->txn_begin(env, NULL, &txn, 0);                                                               
		CKERR(r);
		DB_BTREE_STAT64 stats;
		r = dbs[i]->stat64(dbs[i], txn, &stats);
		CKERR(r);
                if (verbose)
                    printf("NUM_ROWS=%d n_keys=%" PRIu64 " n_data=%" PRIu64 " dsize=%" PRIu64 " fsize=%" PRIu64 "\n",
                           NUM_ROWS, stats.bt_nkeys, stats.bt_ndata, stats.bt_dsize, stats.bt_fsize);
                if (DISALLOW_PUTS) {
                    assert(stats.bt_nkeys == 0);  // Fix as part of #4129.  Was ==
                    assert(stats.bt_ndata == 0);
                    assert(stats.bt_dsize == 0);
                } else {
                    assert(stats.bt_nkeys <= (uint64_t)NUM_ROWS);  // Fix as part of #4129.  Was ==
                    assert(stats.bt_ndata <= (uint64_t)NUM_ROWS);
                    assert(stats.bt_dsize == ((uint64_t)NUM_ROWS) * (sizeof(unsigned int) + VALSIZE));
                }
		r = txn->commit(txn, 0);
		CKERR(r);
	    }
	}
    } else {
	r = txn->abort(txn);
	CKERR(r);
    }
}

static const char *envdir = TOKU_TEST_FILENAME;
const char *tmp_subdir = "tmp.subdir";

static void run_test(void) 
{
    int r;

    if (upgrade_test) {
        // cmake set up the environment
    }
    else {
        toku_os_recursive_delete(envdir);
	r = toku_os_mkdir(envdir, S_IRWXU+S_IRWXG+S_IRWXO);                                                      CKERR(r);
        char tmpdir[TOKU_PATH_MAX+1];
        toku_path_join(tmpdir, 2, envdir, tmp_subdir);
        r = toku_os_mkdir(tmpdir, S_IRWXU+S_IRWXG+S_IRWXO);                                                   CKERR(r);
    }

    r = db_env_create(&env, 0);                                                                           CKERR(r);
    r = env->set_redzone(env, 0);                                                                         CKERR(r);
    r = env->set_tmp_dir(env, tmp_subdir);                                                                CKERR(r);
    
    r = env->set_default_bt_compare(env, uint_dbt_cmp);                                                       CKERR(r);
    if ( verbose ) printf("CACHESIZE = %d MB\n", CACHESIZE);
    r = env->set_cachesize(env, CACHESIZE / 1024, (CACHESIZE % 1024)*1024*1024, 1);                           CKERR(r);
    if (datadir) {
        r = env->set_data_dir(env, datadir);                                                                  CKERR(r);
    }
    r = env->set_generate_row_callback_for_put(env, put_multiple_generate);
    CKERR(r);
    int envflags = DB_INIT_LOCK | DB_INIT_LOG | DB_INIT_MPOOL | DB_INIT_TXN | DB_CREATE | DB_PRIVATE;
    r = env->open(env, envdir, envflags, S_IRWXU+S_IRWXG+S_IRWXO);                                            CKERR(r);
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
        typedef void (*malloc_stats_fun_t)(void);
        malloc_stats_fun_t malloc_stats_f = (malloc_stats_fun_t) dlsym(RTLD_DEFAULT, "malloc_stats");
        if (malloc_stats_f) {
            malloc_stats_f();
        }
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
	    fprintf(stderr, "Usage: -h -c -d <num_dbs> -r <num_rows> [ -b <num_calls> ] [-m <megabytes>] [-M]\n%s\n", cmd);
	    fprintf(stderr, "  where -d <num_dbs>     is the number of dictionaries to build (primary & secondary).  (Default=%d)\n", NUM_DBS);
	    fprintf(stderr, "        -b <num_calls>   causes the poll function to return nonzero after <num_calls>\n");
	    fprintf(stderr, "        -m <m>           use m MB of memory for the cachetable (default is %d MB)\n", CACHESIZE);
            fprintf(stderr, "        -M               use %d MB of memory for the cachetable\n", old_default_cachesize);
	    fprintf(stderr, "        -s               use size factor of 1 and count temporary files\n");
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
        } else if (strcmp(argv[0], "--valsize")==0) {
            argc--; argv++;
            VALSIZE=atoi(argv[0]);
            VALSIZE -= VALSIZE % sizeof(unsigned int);
            if ( VALSIZE < (int)sizeof(unsigned int) ) {
                fprintf(stderr, "--valsize must be multiple of %d\n", (int)sizeof(unsigned int));
                resultcode=1;
                goto do_usage;
            }
        } else if (strcmp(argv[0], "-r")==0) {
            argc--; argv++;
            NUM_ROWS = atoi(argv[0]);
        } else if (strcmp(argv[0], "-c")==0) {
            CHECK_RESULTS = 1;
        } else if (strcmp(argv[0], "-p")==0) {
            DISALLOW_PUTS = LOADER_DISALLOW_PUTS;
        } else if (strcmp(argv[0], "-z")==0) {
            COMPRESS = LOADER_COMPRESS_INTERMEDIATES;
        } else if (strcmp(argv[0], "-m")==0) {
            argc--; argv++;
            CACHESIZE = atoi(argv[0]);
        } else if (strcmp(argv[0], "-M")==0) {
	    CACHESIZE = old_default_cachesize;
        } else if (strcmp(argv[0], "-y")==0) {
            ALLOW_DUPS = 1;
        } else if (strcmp(argv[0], "-s")==0) {
	    //printf("\nTesting loader with size_factor=1\n");
	    db_env_set_loader_size_factor(1);            
	} else if (strcmp(argv[0], "-b")==0) {
	    argc--; argv++;
	    char *end;
	    errno=0;
	    bomb_after_poll_count = strtoll(argv[0], &end, 10);
	    assert(errno==0);
	    assert(*end==0); // make sure we consumed the whole integer.
        } else if (strcmp(argv[0], "--datadir") == 0 && argc > 1) {
            argc--; argv++;
            datadir = argv[0];
	} else if (strcmp(argv[0], "--dont_check_est") == 0) {
	    check_est = false;
        } else if (strcmp(argv[0], "-u")==0) {
            upgrade_test = true;
	} else {
	    fprintf(stderr, "Unknown arg: %s\n", argv[0]);
	    resultcode=1;
	    goto do_usage;
	}
	argc--;
	argv++;
    }
}
