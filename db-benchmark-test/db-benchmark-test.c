/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

/* Insert a bunch of stuff */
#include <toku_portability.h>
#include "tokudb_common_funcs.h"
#include <toku_time.h>
#include <toku_assert.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#if !defined(DB_YESOVERWRITE)
#define DB_YESOVERWRITE 0
#endif

#if !defined(DB_PRELOCKED_WRITE)
#define NO_DB_PRELOCKED
#define DB_PRELOCKED_WRITE 0
#endif

int verbose=1;
int which;

enum { SERIAL_SPACING = 1<<6 };
enum { DEFAULT_ITEMS_TO_INSERT_PER_ITERATION = 1<<20 };
enum { DEFAULT_ITEMS_PER_TRANSACTION = 1<<14 };

static void insert (long long v);
#define CKERR(r) if (r!=0) fprintf(stderr, "%s:%d error %d %s\n", __FILE__, __LINE__, r, db_strerror(r)); assert(r==0);
#define CKERR2(r,rexpect) if (r!=rexpect) fprintf(stderr, "%s:%d error %d %s\n", __FILE__, __LINE__, r, db_strerror(r)); assert(r==rexpect);

/* default test parameters */
int keysize = sizeof (long long);
int valsize = sizeof (long long);
int pagesize = 0;
long long cachesize = 128*1024*1024;
int do_1514_point_query = 0;
int dupflags = 0;
int insert_multiple = 0;
int num_dbs = 1;
int noserial = 0; // Don't do the serial stuff
int norandom = 0; // Don't do the random stuff
int prelock  = 0;
int prelockflag = 0;
int items_per_transaction = DEFAULT_ITEMS_PER_TRANSACTION;
int items_per_iteration   = DEFAULT_ITEMS_TO_INSERT_PER_ITERATION;
int finish_child_first = 0;  // Commit or abort child first (before doing so to the parent).  No effect if child does not exist.
int singlex_child = 0;  // Do a single transaction, but do all work with a child
int singlex = 0;  // Do a single transaction
int singlex_create = 0;  // Create the db using the single transaction (only valid if singlex)
int insert1first = 0;  // insert 1 before doing the rest
int check_small_rollback = 0; // verify that the rollback logs are small (only valid if singlex)
int do_transactions = 0;
int if_transactions_do_logging = DB_INIT_LOG; // set this to zero if we want no logging when transactions are used
int do_abort = 0;
int n_insertions_since_txn_began=0;
int env_open_flags = DB_CREATE|DB_PRIVATE|DB_INIT_MPOOL;
u_int32_t put_flags = DB_YESOVERWRITE;
double compressibility = -1; // -1 means make it very compressible.  1 means use random bits everywhere.  2 means half the bits are random.
int do_append = 0;
int do_checkpoint_period = 0;
u_int32_t checkpoint_period = 0;
static const char *log_dir = NULL;
static int commitflags = 0;
static int redzone = 0;
static int redzone_set = 0;

static int use_random = 0;
enum { MAX_RANDOM_C = 16000057 }; // prime-numbers.org
static unsigned char random_c[MAX_RANDOM_C];
static int next_random_c;

static void init_random_c(void) {
    int i;
    for (i=0; i<MAX_RANDOM_C; i++)
	random_c[i] = (unsigned char) random();
}

static void update_random_c_index(int n) {
    next_random_c += n;
    if (next_random_c >= MAX_RANDOM_C)
	next_random_c = 0;
}

static unsigned char get_random_c(void) {
    update_random_c_index(1);
    return random_c[next_random_c];
}

static int min_int(int a, int b) {
    return a > b ? b : a;
}

static void copy_random_c(unsigned char *p, int n) {
    while (n > 0) {
	int m = min_int(n, MAX_RANDOM_C-next_random_c);
	memcpy(p, &random_c[next_random_c], m);
	n -= m;
	p += m;
	update_random_c_index(m);
    }
}

static void do_prelock(DB* db, DB_TXN* txn) {
    if (prelock) {
#if !defined(NO_DB_PRELOCKED)
        int r = db->pre_acquire_table_lock(db, txn);
        assert(r==0);
#else
	db = db; txn = txn;
#endif
    }
}

#define STRINGIFY2(s) #s
#define STRINGIFY(s) STRINGIFY2(s)
const char *dbdir = "./bench."  STRINGIFY(DIRSUF); /* DIRSUF is passed in as a -D argument to the compiler. */
char *dbfilename = "bench.db";
char *dbname;

DB_ENV *dbenv;
enum {MAX_DBS=128};
DB *dbs[MAX_DBS];
uint32_t put_flagss[MAX_DBS];
DB_TXN *parenttid=0;
DB_TXN *tid=0;
DBT dest_keys[MAX_DBS];
DBT dest_vals[MAX_DBS];

#if defined(TOKUDB)
static int
put_multiple_generate(DB *dest_db, DB *src_db, DBT *dest_key, DBT *dest_val, const DBT *src_key, const DBT *src_val, void *extra) {
    assert(src_db == NULL);
    assert(dest_db != NULL);
    assert(extra == &put_flags); //Verifying extra gets set right.

    dest_key->data = src_key->data;
    dest_key->size = src_key->size;
    dest_val->data = src_val->data;
    dest_val->size = src_val->size;
    return 0;
}
#endif



static void benchmark_setup (void) {
    int r;
   
    if (!do_append) {
	char unlink_cmd[strlen(dbdir) + strlen("rm -rf ") + 1];
	snprintf(unlink_cmd, sizeof(unlink_cmd), "rm -rf %s", dbdir);
	//printf("unlink_cmd=%s\n", unlink_cmd);
	r = system(unlink_cmd);
        CKERR(r);

        if (strcmp(dbdir, ".") != 0) {
            r = toku_os_mkdir(dbdir,S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH);
            assert(r == 0);
        }
    }

    r = db_env_create(&dbenv, 0);
    assert(r == 0);

#if DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR <= 4
    if (dbenv->set_lk_max) {
	r = dbenv->set_lk_max(dbenv, items_per_transaction*2);
	assert(r==0);
    }
#endif
    if (dbenv->set_lk_max_locks) {
        r = dbenv->set_lk_max_locks(dbenv, items_per_transaction*2);
        assert(r == 0);
    }

    if (dbenv->set_cachesize) {
        r = dbenv->set_cachesize(dbenv, cachesize / (1024*1024*1024), cachesize % (1024*1024*1024), 1);
        if (r != 0) 
            printf("WARNING: set_cachesize %d\n", r);
    }

    if (log_dir) {
        r = dbenv->set_lg_dir(dbenv, log_dir);
        assert(r == 0);
    }
#if defined(TOKUDB)
    if (insert_multiple) {
        r = dbenv->set_generate_row_callback_for_put(dbenv, put_multiple_generate);
        CKERR(r);
    }
#endif

#if defined(TOKUDB)
    if (redzone_set) {
	r = dbenv->set_redzone(dbenv, redzone);
	assert(r == 0);
    }
#endif

    r = dbenv->open(dbenv, dbdir, env_open_flags, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
    assert(r == 0);

#if defined(TOKUDB)
    if (do_checkpoint_period) {
        r = dbenv->checkpointing_set_period(dbenv, checkpoint_period);
        assert(r == 0);
        u_int32_t period;
        r = dbenv->checkpointing_get_period(dbenv, &period);
        assert(r == 0 && period == checkpoint_period);
    }
#endif

    for (which = 0; which < num_dbs; which++) {
        r = db_create(&dbs[which], dbenv, 0);
        assert(r == 0);
    }

    if (do_transactions) {
	r=dbenv->txn_begin(dbenv, 0, &tid, 0); CKERR(r);
    }
    for (which = 0; which < num_dbs; which++) {
        DB *db = dbs[which];
        if (pagesize && db->set_pagesize) {
            r = db->set_pagesize(db, pagesize); 
            assert(r == 0);
        }
        if (dupflags) {
            r = db->set_flags(db, dupflags);
            assert(r == 0);
        }
        char name[strlen(dbfilename)+10];
        if (which==0)
            sprintf(name, "%s", dbfilename);
        else
            sprintf(name, "%s_%d", dbfilename, which);
        r = db->open(db, tid, name, NULL, DB_BTREE, DB_CREATE, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
        if (r!=0) fprintf(stderr, "errno=%d, %s\n", errno, strerror(errno));
        assert(r == 0);
    }
    if (insert1first) {
        if (do_transactions) {
            r=tid->commit(tid, 0);
            assert(r==0);
            tid = NULL;
            r=dbenv->txn_begin(dbenv, 0, &tid, 0); CKERR(r);
        }
        insert(-1);
        if (singlex) {
            r=tid->commit(tid, 0);
            assert(r==0);
            tid = NULL;
            r=dbenv->txn_begin(dbenv, 0, &tid, 0); CKERR(r);
        }
    }
    else if (singlex && !singlex_create) {
        r=tid->commit(tid, 0);
        assert(r==0);
        tid = NULL;
        r=dbenv->txn_begin(dbenv, 0, &tid, 0); CKERR(r);
    }
    if (do_transactions) {
	if (singlex) {
            for (which = 0; which < num_dbs; which++) {
                DB *db = dbs[which];
                do_prelock(db, tid);
            }
        }
        else {
            r=tid->commit(tid, 0);
            assert(r==0);
            tid = NULL;
        }
    }
    if (singlex_child) {
        parenttid = tid;
        tid = NULL;
        r=dbenv->txn_begin(dbenv, parenttid, &tid, 0); CKERR(r);
    }

}

#if defined(TOKUDB)
static void test1514(void);
#endif
static void benchmark_shutdown (void) {
    int r;
    
#if defined(TOKUDB)
    if (do_1514_point_query) test1514();
#endif
    if (do_transactions && singlex && !insert1first && (singlex_create || prelock)) {
#if defined(TOKUDB)
        //There should be a single 'truncate' in the rollback instead of many 'insert' entries.
	struct txn_stat *s;
	r = tid->txn_stat(tid, &s);
	assert(r==0);
        //TODO: #1125 Always do the test after performance testing is done.
        if (singlex_child) fprintf(stderr, "SKIPPED 'small rollback' test for child txn\n");
        else
            assert(s->rollback_raw_count < 100);  // gross test, not worth investigating details
	os_free(s);
	//system("ls -l bench.tokudb");
#endif
    }
    if (do_transactions && singlex) {
        if (!singlex_child || finish_child_first) {
            assert(tid);
            r = (do_abort ? tid->abort(tid) : tid->commit(tid, 0));    assert(r==0);
            tid = NULL; 
        }
        if (singlex_child) {
            tid = NULL;
            assert(parenttid);
            r = (do_abort ? parenttid->abort(parenttid) : parenttid->commit(parenttid, 0));    assert(r==0);
            parenttid = NULL;
        }
        else
            assert(!parenttid);
    }
    assert(!tid);
    assert(!parenttid);

    for (which = 0; which < num_dbs; which++) {
        DB *db = dbs[which];
        r = db->close(db, 0);
        assert(r == 0);
    }
    r = dbenv->close(dbenv, 0);
    assert(r == 0);
}

static void long_long_to_array (unsigned char *a, int array_size, unsigned long long l) {
    int i;
    for (i=0; i<8 && i<array_size; i++)
	a[i] = (l>>(56-8*i))&0xff;
}

static DBT *fill_dbt(DBT *dbt, const void *data, int size) {
    memset(dbt, 0, sizeof *dbt);
    dbt->size = size;
    dbt->data = (void *) data;
    return dbt;
}

// Fill array with 0's if compressibilty==-1, otherwise fill array with data that is likely to compress by a factor of compressibility.
static void fill_array (unsigned char *data, int size) {
    memset(data, 0, size);
    if (compressibility>0) {
	if (use_random) {
            int i;
            for (i=0; i<size/compressibility; i++) {
                data[i] = (unsigned char) random();
            }
        } else {
            copy_random_c(data, size/compressibility);
        }
    }
}

static void insert (long long v) {
    int r;
    unsigned char kc[keysize];
    unsigned char vc[valsize];;
    DBT  kt, vt;
    fill_array(kc, keysize);
    long_long_to_array(kc, keysize, v); // Fill in the array first, then write the long long in.
    fill_array(vc, valsize);
    long_long_to_array(vc, valsize, v);
    fill_dbt(&kt, kc, keysize);
    fill_dbt(&vt, vc, valsize);
    if (insert_multiple) {
#if defined(TOKUDB)
        r = dbenv->put_multiple(dbenv, NULL, tid, &kt, &vt, num_dbs, dbs, dest_keys, dest_vals, put_flagss, &put_flags); //Extra used just to verify its passed right
#else
        r = EINVAL;
#endif
        CKERR(r);
    }
    else {
        for (which = 0; which < num_dbs; which++) {
            DB *db = dbs[which];
            r = db->put(db, tid, &kt, &vt, put_flags);
            CKERR(r);
        }
    }
    if (do_transactions) {
	if (n_insertions_since_txn_began>=items_per_transaction && !singlex) {
	    n_insertions_since_txn_began=0;
	    r = tid->commit(tid, commitflags); assert(r==0);
            tid = NULL;
	    r=dbenv->txn_begin(dbenv, 0, &tid, 0); assert(r==0);
            for (which = 0; which < num_dbs; which++) {
                DB *db = dbs[which];
                do_prelock(db, tid);
            }
	    n_insertions_since_txn_began=0;
	}
	n_insertions_since_txn_began++;
    }
}

static void serial_insert_from (long long from) {
    long long i;
    if (do_transactions && !singlex) {
	int r = dbenv->txn_begin(dbenv, 0, &tid, 0); assert(r==0);
        for (which = 0; which < num_dbs; which++) {
            DB *db = dbs[which];
            do_prelock(db, tid);
        }
    }
    for (i=0; i<items_per_iteration; i++) {
	insert((from+i)*SERIAL_SPACING);
    }
    if (do_transactions && !singlex) {
	int  r= tid->commit(tid, 0);             assert(r==0);
	tid=NULL;
    }
}

static long long llrandom (void) {
    return (((long long)(random()))<<32) + random();
}

static void random_insert_below (long long below) {
    long long i;
    if (do_transactions && !singlex) {
	int r = dbenv->txn_begin(dbenv, 0, &tid, 0); assert(r==0);
        for (which = 0; which < num_dbs; which++) {
            DB *db = dbs[which];
            do_prelock(db, tid);
        }
    }
    for (i=0; i<items_per_iteration; i++) {
	insert(llrandom()%below);
    }
    if (do_transactions && !singlex) {
	int  r= tid->commit(tid, 0);             assert(r==0);
	tid=NULL;
    }
}

static void biginsert (long long n_elements, struct timeval *starttime) {
    long long i;
    struct timeval t1,t2;
    int iteration;
    for (i=0, iteration=0; i<n_elements; i+=items_per_iteration, iteration++) {
	if (!noserial) {
	    gettimeofday(&t1,0);
	    serial_insert_from(i);
	    gettimeofday(&t2,0);
	    if (verbose) printf("serial %9.6fs %8.0f/s    ", toku_tdiff(&t2, &t1), items_per_iteration/toku_tdiff(&t2, &t1));
	    fflush(stdout);
	}
        if (!norandom) {
	gettimeofday(&t1,0);
	random_insert_below((i+items_per_iteration)*SERIAL_SPACING);
	gettimeofday(&t2,0);
	if (verbose) printf("random %9.6fs %8.0f/s    ", toku_tdiff(&t2, &t1), items_per_iteration/toku_tdiff(&t2, &t1));
        }
	if (verbose) printf("cumulative %9.6fs %8.0f/s\n", toku_tdiff(&t2, starttime), (((float)items_per_iteration*(!noserial+!norandom))/toku_tdiff(&t2, starttime))*(iteration+1));
    }
}



const long long default_n_items = 1LL<<22;

static int print_usage (const char *argv0) {
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, " %s [-x] [--keysize KEYSIZE] [--valsize VALSIZE] [--noserial] [--norandom] [ n_iterations ]\n", argv0);
    fprintf(stderr, "   where\n");
    fprintf(stderr, "    -x              do transactions (XCOUNT transactions per iteration) (default: no transactions at all)\n");
    fprintf(stderr, "    --keysize KEYSIZE sets the key size (default 8)\n");
    fprintf(stderr, "    --valsize VALSIZE sets the value size (default 8)\n");
    fprintf(stderr, "    --cachesize CACHESIZE set the database cache size\n");
    fprintf(stderr, "    --pagesize PAGESIZE sets the database page size\n");
    fprintf(stderr, "    --noserial         causes the serial insertions to be skipped\n");
    fprintf(stderr, "    --norandom         causes the random insertions to be skipped\n");
    fprintf(stderr, "    --compressibility C   creates data that should compress by about a factor C.   Default C is large.   C is an float.\n");
    fprintf(stderr, "    --xcount N            how many insertions per transaction (default=%d)\n", DEFAULT_ITEMS_PER_TRANSACTION);
    fprintf(stderr, "    --nosync              commit with nosync\n");
    fprintf(stderr, "    --singlex             (implies -x) Run the whole job as a single transaction.  (Default don't run as a single transaction.)\n");
    fprintf(stderr, "    --singlex-child       (implies -x) Run the whole job as a single transaction, do all work a child of that transaction.\n");
    fprintf(stderr, "    --finish-child-first  Commit/abort child before doing so to parent (no effect if no child).\n");
    fprintf(stderr, "    --singlex-create      (implies --singlex)  Create the file using the single transaction (Default is to use a different transaction to create.)\n");
    fprintf(stderr, "    --check_small_rollback (Only valid in --singlex mode)  Verify that very little data was saved in the rollback logs.\n");
    fprintf(stderr, "    --prelock             Prelock the database.\n");
    fprintf(stderr, "    --prelockflag         Prelock the database and send the DB_PRELOCKED_WRITE flag.\n");
    fprintf(stderr, "    --abort               Abort the singlex after the transaction is over. (Requires --singlex.)\n");
    fprintf(stderr, "    --nolog               If transactions are used, then don't write the recovery log\n");
    fprintf(stderr, "    --log_dir LOGDIR      Put the logs in LOGDIR\n");
    fprintf(stderr, "    --env DIR\n");
    fprintf(stderr, "    --periter N           how many insertions per iteration (default=%d)\n", DEFAULT_ITEMS_TO_INSERT_PER_ITERATION);
    fprintf(stderr, "    --1514                do a point query for something not there at end.  See #1514.  (Requires --norandom)\n");
    fprintf(stderr, "    --append              append to an existing file\n");
    fprintf(stderr, "    --userandom           use random()\n");
    fprintf(stderr, "    --checkpoint-period %"PRIu32"       checkpoint period\n", checkpoint_period); 
    fprintf(stderr, "    --numdbs N            Insert same items into N dbs (1 to %d)\n", MAX_DBS); 
    fprintf(stderr, "    --insertmultiple      Use DB_ENV->put_multiple api.  Requires transactions.\n"); 
    fprintf(stderr, "    --redzone N           redzone in percent\n");
    fprintf(stderr, "    --srandom N           srandom(N)\n");
    fprintf(stderr, "   n_iterations     how many iterations (default %lld)\n", default_n_items/DEFAULT_ITEMS_TO_INSERT_PER_ITERATION);

    return 1;
}

#if defined(TOKUDB)
static int
nothing(DBT const* UU(key), DBT const* UU(val), void* UU(extra)) {
    return 0;
}

static void
test1514(void) {
    assert(norandom); //Otherwise we can't know the given element is missing.
    unsigned char kc[keysize], vc[valsize];
    DBT  kt;
    long long v = SERIAL_SPACING - 1;
    fill_array(kc, sizeof kc);
    long_long_to_array(kc, keysize, v); // Fill in the array first, then write the long long in.
    fill_array(vc, sizeof vc);
    long_long_to_array(vc, valsize, v);
    int r;
    DBC *c;


    struct timeval t1,t2;

    for (which = 0; which < num_dbs; which++) {
        DB *db = dbs[which];
        r = db->cursor(db, tid, &c, 0); CKERR(r);
        gettimeofday(&t1,0);
        r = c->c_getf_set(c, 0, fill_dbt(&kt, kc, keysize), nothing, NULL);
        gettimeofday(&t2,0);
        CKERR2(r, DB_NOTFOUND);
        r = c->c_close(c); CKERR(r);
    }

    if (verbose) printf("(#1514) Single Point Query %9.6fs\n", toku_tdiff(&t2, &t1));
}
#endif

static int test_main (int argc, char *const argv[]) {
    struct timeval t1,t2,t3;
    long long total_n_items = default_n_items;
    char *endptr;
    int i;
    for (i=1; i<argc; i++) {
        const char *arg = argv[i];
        if (arg[0] != '-')
            break;
	if (strcmp(arg, "-q") == 0) {
	    verbose--; if (verbose<0) verbose=0;
	} else if (strcmp(arg, "-x") == 0) {
            do_transactions = 1;
        } else if (strcmp(arg, "--noserial") == 0) {
	    noserial=1;
	} else if (strcmp(arg, "--norandom") == 0) {
	    norandom=1;
	} else if (strcmp(arg, "--insertmultiple") == 0) {
            insert_multiple=1;
	} else if (strcmp(arg, "--numdbs") == 0) {
	    num_dbs = atoi(argv[++i]);
            if (num_dbs <= 0 || num_dbs > MAX_DBS) {
                fprintf(stderr, "--numdbs needs between 1 and %d\n", MAX_DBS);
                return print_usage(argv[0]);
            }
	} else if (strcmp(arg, "--compressibility") == 0) {
	    compressibility = atof(argv[++i]);
	    init_random_c(); (void) get_random_c();
	} else if (strcmp(arg, "--nolog") == 0) {
	    if_transactions_do_logging = 0;
	} else if (strcmp(arg, "--singlex-create") == 0) {
	    do_transactions = 1;
	    singlex = 1;
	    singlex_create = 1;
	} else if (strcmp(arg, "--finish-child-first") == 0) {
	    finish_child_first = 1;
	} else if (strcmp(arg, "--singlex-child") == 0) {
	    do_transactions = 1;
	    singlex = 1;
	    singlex_child = 1;
	} else if (strcmp(arg, "--singlex") == 0) {
	    do_transactions = 1;
	    singlex = 1;
	} else if (strcmp(arg, "--insert1first") == 0) {
	    insert1first = 1;
	} else if (strcmp(arg, "--check_small_rollback") == 0) {
	    check_small_rollback = 1;
	} else if (strcmp(arg, "--xcount") == 0) {
            if (i+1 >= argc) return print_usage(argv[0]);
            items_per_transaction = strtoll(argv[++i], &endptr, 10); assert(*endptr == 0);
        } else if (strcmp(arg, "--abort") == 0) {
            do_abort = 1;
        } else if (strcmp(arg, "--periter") == 0) {
            if (i+1 >= argc) return print_usage(argv[0]);
            items_per_iteration = strtoll(argv[++i], &endptr, 10); assert(*endptr == 0);
        } else if (strcmp(arg, "--cachesize") == 0) {
            if (i+1 >= argc) return print_usage(argv[0]);
            cachesize = strtoll(argv[++i], &endptr, 10); assert(*endptr == 0);
        } else if (strcmp(arg, "--keysize") == 0) {
            if (i+1 >= argc) return print_usage(argv[0]);
            keysize = atoi(argv[++i]);
        } else if (strcmp(arg, "--valsize") == 0) {
            if (i+1 >= argc) return print_usage(argv[0]);
            valsize = atoi(argv[++i]);
        } else if (strcmp(arg, "--pagesize") == 0) {
            if (i+1 >= argc) return print_usage(argv[0]);
           pagesize = atoi(argv[++i]);
        } else if (strcmp(arg, "--dupsort") == 0) {
            dupflags = DB_DUP + DB_DUPSORT;
            continue;
	} else if (strcmp(arg, "--env") == 0) {
	    if (i+1 >= argc) return print_usage(argv[0]);
	    dbdir = argv[++i];
#if defined(TOKUDB)
        } else if (strcmp(arg, "--1514") == 0) {
            do_1514_point_query=1;
#endif
        } else if (strcmp(arg, "--prelock") == 0) {
            prelock=1;
        } else if (strcmp(arg, "--prelockflag") == 0) {
            prelock=1;
            prelockflag=1;
        } else if (strcmp(arg, "--srandom") == 0) {
            if (i+1 >= argc) return print_usage(argv[0]);
            srandom(atoi(argv[++i]));
        } else if (strcmp(arg, "--append") == 0) {
            do_append = 1;
        } else if (strcmp(arg, "--checkpoint-period") == 0) {
            if (i+1 >= argc) return print_usage(argv[9]);
            do_checkpoint_period = 1;
            checkpoint_period = (u_int32_t) atoi(argv[++i]);
        } else if (strcmp(arg, "--nosync") == 0) {
            commitflags += DB_TXN_NOSYNC;
        } else if (strcmp(arg, "--userandom") == 0) {
            use_random = 1;
        } else if (strcmp(arg, "--unique_checks") == 0) {
            if (i+1 >= argc) return print_usage(argv[0]);
            int unique_checks = atoi(argv[++i]);
            if (unique_checks)
                put_flags = DB_NOOVERWRITE;
            else
                put_flags = DB_YESOVERWRITE;
        } else if (strcmp(arg, "--log_dir") == 0) {
            if (i+1 >= argc) return print_usage(argv[0]);
            log_dir = argv[++i];
	} else if (strcmp(arg, "--redzone") == 0) {
	    if (i+1 >= argc) return print_usage(argv[0]);
	    redzone_set = 1;
	    redzone = atoi(argv[++i]);
        } else {
	    return print_usage(argv[0]);
	}
    }
    if (do_transactions) {
	env_open_flags |= DB_INIT_TXN | if_transactions_do_logging | DB_INIT_LOCK;
    }
    if (do_transactions && prelockflag) {
        put_flags |= DB_PRELOCKED_WRITE;
    }
    if (i<argc) {
        /* if it looks like a number */
        char *end;
        errno=0;
        long n_iterations = strtol(argv[i], &end, 10);
        if (errno!=0 || *end!=0 || end==argv[i]) {
            print_usage(argv[0]);
            return 1;
        }
        total_n_items = items_per_iteration * (long long)n_iterations;
    }
    if (verbose) {
	if (!noserial) printf("serial ");
	if (!noserial && !norandom) printf("and ");
	if (!norandom) printf("random ");
	printf("insertions of %d per batch%s\n", items_per_iteration, do_transactions ? " (with transactions)" : "");
    }
#if !defined TOKUDB
    if (insert_multiple) {
	fprintf(stderr, "--insert_multiple only works on the TokuDB (not BDB)\n");
	return print_usage(argv[0]);
    }
    if (check_small_rollback) {
	fprintf(stderr, "--check_small_rollback only works on the TokuDB (not BDB)\n");
	return print_usage(argv[0]);
    }
#endif
    if (insert_multiple) {
        memset(dest_keys, 0, sizeof(dest_keys));
        memset(dest_vals, 0, sizeof(dest_vals));
        for (which = 0; which < num_dbs; which++) {
            put_flagss[i] = put_flags;
        }
    }
    if (check_small_rollback && !singlex) {
	fprintf(stderr, "--check_small_rollback requires --singlex\n");
	return print_usage(argv[0]);
    }
    if (!do_transactions && insert_multiple) {
	fprintf(stderr, "--insert_multiple requires transactions\n");
	return print_usage(argv[0]);
    }
    benchmark_setup();
    gettimeofday(&t1,0);
    biginsert(total_n_items, &t1);
    gettimeofday(&t2,0);
    benchmark_shutdown();
    gettimeofday(&t3,0);
    if (verbose) {
	printf("Shutdown %9.6fs\n", toku_tdiff(&t3, &t2));
	printf("Total time %9.6fs for %lld insertions = %8.0f/s\n", toku_tdiff(&t3, &t1), 
	       (!noserial+!norandom)*total_n_items, (!noserial+!norandom)*total_n_items/toku_tdiff(&t3, &t1));
    }
#if 0 && defined TOKUDB
    if (verbose) {
	extern int toku_os_get_max_rss(int64_t*);
        int64_t mrss;
        int r = toku_os_get_max_rss(&mrss);
        assert(r==0);
	printf("maxrss=%.2fMB\n", mrss/256.0);
    }
    if (0) {
	extern void print_hash_histogram (void) __attribute__((__visibility__("default")));
	print_hash_histogram();
    }
#endif

    return 0;
}
