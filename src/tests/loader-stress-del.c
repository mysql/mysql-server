/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2010 Tokutek Inc.  All rights reserved."
#ident "$Id$"


// Need to use malloc for the malloc instrumentation tests
#define TOKU_ALLOW_DEPRECATED

#include "test.h"
#include "toku_pthread.h"
#include "toku_atomic.h"
#include <db.h>
#include <sys/stat.h>
#include "ydb-internal.h"

DB_ENV *env;
enum {MAX_NAME=128};
enum {MAX_DBS=1024};
int NUM_DBS=1;
int NUM_ROWS=1000000;
int CHECK_RESULTS=1;
int USE_PUTS=0;
enum { old_default_cachesize=1024 }; // MB
int CACHESIZE=old_default_cachesize;
int ALLOW_DUPS=0;
enum {MAGIC=311};
char *datadir = NULL;
BOOL check_est = TRUE; // do check the estimates by default
BOOL footprint_print = FALSE; // print memory footprint info 
BOOL upgrade_test = FALSE;   

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

#if defined(__cilkplusplus) || defined (__cplusplus)
extern "C" {
#endif

static const char *loader_temp_prefix = "tokuld"; // #2536

// return number of temp files
static int
count_temp(char * dirname) {
    int n = 0;
    
    DIR * dir = opendir(dirname);
    
    struct dirent *ent;
    while ((ent=readdir(dir))) {
	if (ent->d_type==DT_REG && strncmp(ent->d_name, loader_temp_prefix, 6)==0) {
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
static unsigned int generate_val(int key, int i) {
    return rotl32((key + MAGIC), i);
}
static unsigned int pkey_for_val(int key, int i) {
    return rotr32(key, i) - MAGIC;
}

// There is no handlerton in this test, so this function is a local replacement
// for the handlerton's generate_row_for_put().
static int put_multiple_generate(DB *dest_db, DB *src_db, DBT *dest_key, DBT *dest_val, const DBT *src_key, const DBT *src_val) {

    src_db = src_db;

    uint32_t which = *(uint32_t*)dest_db->app_private;

    if ( which == 0 ) {
        if (dest_key->flags==DB_DBT_REALLOC) {
            if (dest_key->data) toku_free(dest_key->data);
            dest_key->flags = 0;
            dest_key->ulen  = 0;
        }
        if (dest_val->flags==DB_DBT_REALLOC) {
            if (dest_val->data) toku_free(dest_val->data);
            dest_val->flags = 0;
            dest_val->ulen  = 0;
        }
        dbt_init(dest_key, src_key->data, src_key->size);
        dbt_init(dest_val, src_val->data, src_val->size);
    }
    else {
        assert(dest_key->flags==DB_DBT_REALLOC);
        if (dest_key->ulen < sizeof(unsigned int)) {
            dest_key->data = toku_xrealloc(dest_key->data, sizeof(unsigned int));
            dest_key->ulen = sizeof(unsigned int);
        }
        assert(dest_val->flags==DB_DBT_REALLOC);
        if (dest_val->ulen < sizeof(unsigned int)) {
            dest_val->data = toku_xrealloc(dest_val->data, sizeof(unsigned int));
            dest_val->ulen = sizeof(unsigned int);
        }
        unsigned int *new_key = (unsigned int *)dest_key->data;
        unsigned int *new_val = (unsigned int *)dest_val->data;

        *new_key = twiddle32(*(unsigned int*)src_key->data, which);
        *new_val = generate_val(*(unsigned int*)src_key->data, which);

        dest_key->size = sizeof(unsigned int);
        dest_val->size = sizeof(unsigned int);
        //data is already set above
    }

//    printf("dest_key.data = %d\n", *(int*)dest_key->data);
//    printf("dest_val.data = %d\n", *(int*)dest_val->data);

    return 0;
}

#if defined(__cilkplusplus) || defined(__cplusplus)
} // extern "C"
#endif

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
    for(int j=0;j<NUM_DBS;j++) {
        unsigned int prev_k = 0, prev_v = 0;

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

        for (int i = 0; i < NUM_ROWS+1; i++) {
            r = cursor->c_get(cursor, &key, &val, DB_NEXT);
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

            // check prev_key < key
            if (i > 0) 
                assert(prev_k < k);

            // update prev = current
            prev_k = k; prev_v = v;
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

static void delete_all(DB **dbs) {
    for(int j=0;j<NUM_DBS;j++) {

        int r;

        DB_TXN *txn;
        r = env->txn_begin(env, NULL, &txn, 0);
        CKERR(r);

        // generate the expected keys
        unsigned int *expected_key = (unsigned int *) toku_malloc(NUM_ROWS * sizeof (unsigned int));
        for (int i = 0; i < NUM_ROWS; i++)
            expected_key[i] = j == 0 ? (unsigned int)(i+1) : twiddle32(i+1, j);
        // sort the keys
        qsort(expected_key, NUM_ROWS, sizeof (unsigned int), uint_cmp);

        // delete all of the keys
        for (int i = 0; i < NUM_ROWS; i++) {
            DBT key;
            dbt_init(&key, &expected_key[i], sizeof expected_key[i]);
            r = dbs[j]->del(dbs[j], txn, &key, DB_DELETE_ANY);
            assert(r == 0);
        }

        // verify empty
        DBC *cursor;
        r = dbs[j]->cursor(dbs[j], txn, &cursor, 0);
        CKERR(r);

        DBT key, val;
        unsigned int k=0, v=0;
        dbt_init(&key, &k, sizeof(unsigned int));
        dbt_init(&val, &v, sizeof(unsigned int));

        r = cursor->c_get(cursor, &key, &val, DB_NEXT);
        assert(r == DB_NOTFOUND);

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
static BOOL did_start=FALSE;
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
    if ( (USE_PUTS == 1) && (ALLOW_DUPS == 1) ) flags = DB_YESOVERWRITE;
    for(int i=0;i<MAX_DBS;i++) { 
        db_flags[i] = flags;
        dbt_flags[i] = 0;
    }
    
    uint32_t loader_flags = USE_PUTS ? LOADER_USE_PUTS : 0; // set with -p option

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
    unsigned int k, v;
    for(int i=1;i<=NUM_ROWS;i++) {
        k = i;
        v = generate_val(i, 0);
        dbt_init(&key, &k, sizeof(unsigned int));
        dbt_init(&val, &v, sizeof(unsigned int));
        r = loader->put(loader, &key, &val);
        CKERR(r);
        if ( verbose) { if((i%10000) == 0){printf("."); fflush(stdout);} }
    }
    if ( verbose ) {printf("\n"); fflush(stdout);}        
        
    poll_count=0;

    int n = count_temp(env->i->real_data_dir);
    if (verbose) printf("Num temp files = %d\n", n);

    did_start = TRUE;
    gettimeofday(&start, 0);

    // close the loader
    if ( verbose ) printf("%9.6fs closing\n", elapsed_time());
    if (footprint_print) printf("%s:%d Hiwater=%ld water=%ld\n", __FILE__, __LINE__, hiwater, water);
    r = loader->close(loader);
    if (footprint_print) printf("%s:%d Hiwater=%ld water=%ld (extra hiwater=%ldM)\n", __FILE__, __LINE__, hiwater, water, (hiwater-hiwater_start)/(1024*1024));
    if ( verbose ) printf("%9.6fs done\n",    elapsed_time());
    CKERR2s(r,0,TOKUDB_CANCELED);

    if (r==0) {
	if ( USE_PUTS == 0 ) {
	    if (poll_count == 0) printf("%s:%d\n", __FILE__, __LINE__);
	    assert(poll_count>0);
	}

	r = txn->commit(txn, 0);
	CKERR(r);

	// verify the DBs
	if ( CHECK_RESULTS ) {
	    check_results(dbs);
            delete_all(dbs);
	}

    } else {
	r = txn->abort(txn);
	CKERR(r);
    }
}


char *free_me = NULL;
char *env_dir = ENVDIR; // the default env_dir.
char *tmp_subdir = "tmp.subdir";

#define OLDDATADIR "../../../../tokudb.data/"
char *db_v4_dir        = OLDDATADIR "env_preload.4.1.1.emptydictionaries.cleanshutdown";

static void setup(void) {
    int r;
    int len = 256;
    char syscmd[len];
    char * src_db_dir;

    src_db_dir = db_v4_dir;

    r = snprintf(syscmd, len, "cp -r %s %s", src_db_dir, env_dir);
    assert(r<len);
    r = system(syscmd);                                                                                 
    CKERR(r);
}

static void run_test(void) 
{
    int r;
    
    int cmdlen = strlen(env_dir) + strlen(tmp_subdir) + 10;
    char tmpdir[cmdlen];
    r = snprintf(tmpdir, cmdlen, "%s/%s", env_dir, tmp_subdir);
    assert(r<cmdlen);
    
    // first delete anything left from previous run of this test
    {
	int len = strlen(env_dir) + 20;
	char syscmd[len];
	r = snprintf(syscmd, len, "rm -rf %s", env_dir);
	assert(r<len);
	r = system(syscmd);                                                                                   CKERR(r);
    }
    if (upgrade_test) {
	setup();
    }
    else {
	r = toku_os_mkdir(env_dir, S_IRWXU+S_IRWXG+S_IRWXO);                                                      CKERR(r);
	r = toku_os_mkdir(tmpdir, S_IRWXU+S_IRWXG+S_IRWXO);                                                   CKERR(r);
    }

    r = db_env_create(&env, 0);                                                                           CKERR(r);
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
            CHK(dbs[i]->change_descriptor(dbs[i], txn_desc, &desc, 0));
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
	    fprintf(stderr, "Usage: -h -c -d <num_dbs> -r <num_rows> [ -b <num_calls> ] [-m <megabytes>] [-M]\n%s\n", cmd);
	    fprintf(stderr, "  where -d <num_dbs>     is the number of dictionaries to build (primary & secondary).  (Default=%d)\n", NUM_DBS);
	    fprintf(stderr, "        -b <num_calls>   causes the poll function to return nonzero after <num_calls>\n");
	    fprintf(stderr, "        -e <env>         uses <env> to construct the directory (so that different tests can run concurrently)\n");
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
	} else if (strcmp(argv[0], "-e")==0) {
            argc--; argv++;
	    if (free_me) toku_free(free_me);
	    int len = strlen(ENVDIR) + strlen(argv[0]) + 2;
	    char full_env_dir[len];
	    int r = snprintf(full_env_dir, len, "%s.%s", ENVDIR, argv[0]);
	    assert(r<len);
	    free_me = env_dir = toku_strdup(full_env_dir);
        } else if (strcmp(argv[0], "-v")==0) {
	    verbose++;
	} else if (strcmp(argv[0],"-q")==0) {
	    verbose--;
	    if (verbose<0) verbose=0;
	} else if (strcmp(argv[0], "-f")==0) {
	    footprint_print = TRUE;
        } else if (strcmp(argv[0], "-r")==0) {
            argc--; argv++;
            NUM_ROWS = atoi(argv[0]);
        } else if (strcmp(argv[0], "-c")==0) {
            CHECK_RESULTS = 1;
        } else if (strcmp(argv[0], "-p")==0) {
            USE_PUTS = 1;
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
	    check_est = FALSE;
        } else if (strcmp(argv[0], "-u")==0) {
            upgrade_test = TRUE;
	} else {
	    fprintf(stderr, "Unknown arg: %s\n", argv[0]);
	    resultcode=1;
	    goto do_usage;
	}
	argc--;
	argv++;
    }
}
