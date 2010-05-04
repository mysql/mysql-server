/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2010 Tokutek Inc.  All rights reserved."
#ident "$Id: loader-stress-test.c 19683 2010-04-30 18:19:17Z yfogel $"


/* TODO:
 *
 *  Decide how to test on recovery (using checkpoint_stress technique?), implement.
 *
 */




/* Purpose is to verify that when a loader fails:
 *  - there are no temp files remaining
 *  - the loader-generated iname file is not present
 *
 * A loader can fail in the following ways:
 *  - user calls loader->abort()
 *  - user aborts transaction
 *  - disk full (ENOSPC)
 *  - crash
 *
 * In the event of a crash, the verification of no temp files and 
 * no loader-generated iname file is done after recovery.
 *
 * Mechanism:
 * This test is derived from the loader-stress-test.
 *
 * The outline of the test is as follows:
 *  - use loader to create table
 *  - verify presence of temp files
 *  - commit / abort / enospc / crash
 *  - verify absence of temp files
 *  - verify absence of unwanted iname files (old inames if committed, new inames if aborted)
 *
 *  
 */


#include "test.h"
#include "toku_pthread.h"
#include <db.h>
#include <sys/stat.h>

#include <sys/types.h>
#include <dirent.h>

#include "ydb-internal.h"

enum test_type {commit,                  // close loader, commit txn
		abort_txn,               // close loader, abort txn
		abort_loader,            // abort loader, abort txn
		abort_via_poll,          // close loader, but poll function returns non-zero, abort txn
		enospc_w,                // close loader, but close fails due to enospc return from toku_os_write
		enospc_f};               // either loader->put() or loader->close() fails due to enospc return from do_fwrite()

int abort_on_poll = 0;  // set when test_loader() called with test_type of abort_via_poll

DB_ENV *env;
enum {MAX_NAME=128};
enum {MAX_DBS=256};
int NUM_DBS=5;
int NUM_ROWS=100000;
int CHECK_RESULTS=0;
int USE_PUTS=0;
int INDUCE_ENOSPC=0;
enum {MAGIC=311};


DBT old_inames[MAX_DBS];
DBT new_inames[MAX_DBS];


int count_temp(char * dirname);
void get_inames(DBT* inames, DB** dbs);
int verify_file(char * dirname, char * filename);
void assert_inames_missing(DBT* inames);
ssize_t bad_write(int, const void *, size_t);

int fwrite_count = 0;
int fwrite_count_nominal = 0;  // number of fwrite calls for normal operation, initially zero
int fwrite_count_trigger = 0;  // sequence number of fwrite call that will fail (zero disables induced failure)

int write_count = 0;
int write_count_nominal = 0;  // number of write calls for normal operation, initially zero
int write_count_trigger = 0;  // sequence number of write call that will fail (zero disables induced failure)

static size_t bad_fwrite (const void *ptr, size_t size, size_t nmemb, FILE *stream) {
    fwrite_count++;
    size_t r;
    if (fwrite_count_trigger == fwrite_count) {
	errno = ENOSPC;
	r = -1;
    } else {
	r = fwrite(ptr, size, nmemb, stream);
    }
    return r;
}


ssize_t 
bad_write(int fd, const void * bp, size_t len) {
    ssize_t r;
    write_count++;
    if (write_count_trigger == write_count) {
	errno = ENOSPC;
	r = -1;
    } else {
	r = write(fd, bp, len);
    }
    return r;
}



// return number of temp files
int
count_temp(char * dirname) {
    int n = 0;
    
    DIR * dir = opendir(dirname);
    
    struct dirent *ent;
    while ((ent=readdir(dir))) {
	if (ent->d_type==DT_REG && strncmp(ent->d_name, "temp", 4)==0) {
	    n++;
	}
    }
    closedir(dir);
    return n;
}



// return non-zero if file exists
int 
verify_file(char * dirname, char * filename) {
    int n = 0;
    DIR * dir = opendir(dirname);
    
    struct dirent *ent;
    while ((ent=readdir(dir))) {
	if (ent->d_type==DT_REG && strcmp(ent->d_name, filename)==0) {
	    n++;
	}
    }
    closedir(dir);
    return n;
}

void
get_inames(DBT* inames, DB** dbs) {
    int i;
    for (i = 0; i < NUM_DBS; i++) {
	DBT dname;
	char * dname_str = dbs[i]->i->dname;
	dbt_init(&dname, dname_str, sizeof(dname_str));
	dbt_init(&(inames[i]), NULL, 0);
	inames[i].flags |= DB_DBT_MALLOC;
	int r = env->get_iname(env, &dname, &inames[i]);
	CKERR(r);
	char * iname_str = (char*) (inames[i].data);
	if (verbose) printf("dname = %s, iname = %s\n", dname_str, iname_str);
    }
}


void 
assert_inames_missing(DBT* inames) {
    int i;
    char * dir = env->i->real_data_dir;
    for (i=0; i<NUM_DBS; i++) {
	char * iname = inames[i].data;
	int r = verify_file(dir, iname);
	if (r) {
	    printf("File %s exists, but it should not\n", iname);
	}
	assert(r == 0);
	if (verbose) printf("File has been properly deleted: %s\n", iname);
    }
}


#if 0
void print_inames(DB** dbs);
void
print_inames(DB** dbs) {
    int i;
    for (i = 0; i < NUM_DBS; i++) {
	DBT dname;
	DBT iname;
	char * dname_str = dbs[i]->i->dname;
	dbt_init(&dname, dname_str, sizeof(dname_str));
	dbt_init(&iname, NULL, 0);
	iname.flags |= DB_DBT_MALLOC;
	int r = env->get_iname(env, &dname, &iname);
	CKERR(r);
	char * iname_str = (char*)iname.data;
	if (verbose) printf("dname = %s, iname = %s\n", dname_str, iname_str);
	int n = verify_file(env->i->real_data_dir, iname_str);
	assert(n == 1);
	toku_free(iname.data);
    }
}
#endif


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
static int put_multiple_generate(DB *dest_db, DB *src_db, DBT *dest_key, DBT *dest_val, const DBT *src_key, const DBT *src_val, void *extra) {

    src_db = src_db;
    extra = extra;

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
        {printf("."); fflush(stdout);}
        r = cursor->c_close(cursor);
        CKERR(r);
        r = txn->commit(txn, 0);
        CKERR(r);
    }
    printf("\nCheck OK\n");
}

static void *expect_poll_void = &expect_poll_void;
static int poll_count=0;
static int poll_function (void *extra, float progress) {
    if (0) {
	static int did_one=0;
	static struct timeval start;
	struct timeval now;
	gettimeofday(&now, 0);
	if (!did_one) {
	    start=now;
	    did_one=1;
	}
	printf("%6.6f %5.1f%%\n", now.tv_sec - start.tv_sec + 1e-6*(now.tv_usec - start.tv_usec), progress*100);
    }
    assert(extra==expect_poll_void);
    assert(0.0<=progress && progress<=1.0);
    poll_count++;
    return abort_on_poll;
}

static void test_loader(enum test_type t, DB **dbs)
{
    int failed_put = 0;

    if (t == abort_via_poll)
	abort_on_poll = 1;
    else
	abort_on_poll = 0;

    int r;
    DB_TXN    *txn;
    DB_LOADER *loader;
    uint32_t db_flags[MAX_DBS];
    uint32_t dbt_flags[MAX_DBS];
    for(int i=0;i<MAX_DBS;i++) { 
        db_flags[i] = DB_NOOVERWRITE; 
        dbt_flags[i] = 0;
    }
    uint32_t loader_flags = USE_PUTS; // set with -p option

    if (verbose) printf("old inames:\n");
    get_inames(old_inames, dbs);
    
    // create and initialize loader
    r = env->txn_begin(env, NULL, &txn, 0);                                                               
    CKERR(r);
    r = env->create_loader(env, txn, &loader, dbs[0], NUM_DBS, dbs, db_flags, dbt_flags, loader_flags);
    CKERR(r);
    r = loader->set_error_callback(loader, NULL, NULL);
    CKERR(r);
    r = loader->set_poll_function(loader, poll_function, expect_poll_void);
    CKERR(r);

    printf("USE_PUTS = %d\n", USE_PUTS);
    if (verbose) printf("new inames:\n");
    get_inames(new_inames, dbs);

    // using loader->put, put values into DB
    DBT key, val;
    unsigned int k, v;
    for(int i=1;i<=NUM_ROWS && !failed_put;i++) {
        k = i;
        v = generate_val(i, 0);
        dbt_init(&key, &k, sizeof(unsigned int));
        dbt_init(&val, &v, sizeof(unsigned int));
        r = loader->put(loader, &key, &val);
	if (t == enospc_f || t == enospc_w)
	    failed_put = r;
	else
	    CKERR(r);
        if ( CHECK_RESULTS || verbose) { if((i%10000) == 0){printf("."); fflush(stdout);} }
    }
    if( CHECK_RESULTS || verbose ) {printf("\n"); fflush(stdout);}        
        
    poll_count=0;

    printf("Data dir is %s\n", env->i->real_data_dir);
    int n = count_temp(env->i->real_data_dir);
    printf("Num temp files = %d\n", n);

    if (t == commit || t == abort_txn) {
	// close the loader
	printf("closing\n"); fflush(stdout);
	r = loader->close(loader);
	CKERR(r);
	if (!USE_PUTS)
	    assert(poll_count>0);
    }
    else if (t == abort_via_poll) {
	assert(!USE_PUTS);  // test makes no sense with USE_PUTS
	printf("closing, but expecting abort via poll\n");
	r = loader->close(loader);
	assert(r);  // not defined what close() returns when poll function returns non-zero
    }
    else if ((t == enospc_f || t == enospc_w)
	     && !failed_put) {
	printf("closing, but expecting failure from enospc\n");
	r = loader->close(loader);
	if (!USE_PUTS)
	    assert(r);
	else
	    CKERR(r);  // if using puts, "outer" loader should close just fine
    }
    else {
	printf("aborting loader"); fflush(stdout);
	r = loader->abort(loader);
	CKERR(r);
    }

    n = count_temp(env->i->real_data_dir);
    if (verbose) printf("Num temp files = %d\n", n);
    assert(n==0);

    printf(" done\n");

    if (t == commit) {
	fwrite_count_nominal = fwrite_count;  // capture how many fwrites were required for normal operation
	write_count_nominal  = write_count;   // capture how many  writes were required for normal operation
	if (verbose) printf("Calls to fwrite nominal: %d, calls to write nominal: %d\n", fwrite_count_nominal, write_count_nominal);
	r = txn->commit(txn, 0);
	CKERR(r);
	if (!USE_PUTS) {
	    assert_inames_missing(old_inames);
	}
	if ( CHECK_RESULTS ) {
	    check_results(dbs);
	}

    }
    else {
	r = txn->abort(txn);
	CKERR(r);
	if (!USE_PUTS) {
	    assert_inames_missing(new_inames);
	}
    }

}


static void run_test(enum test_type t, int trigger) 
{
    int r;

    r = system("rm -rf " ENVDIR);                                                                             CKERR(r);
    r = toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);                                                       CKERR(r);

    r = db_env_create(&env, 0);                                                                               CKERR(r);
    r = env->set_default_bt_compare(env, uint_dbt_cmp);                                                       CKERR(r);
    r = env->set_default_dup_compare(env, uint_dbt_cmp);                                                      CKERR(r);
    r = env->set_generate_row_callback_for_put(env, put_multiple_generate);
    CKERR(r);
//    int envflags = DB_INIT_LOCK | DB_INIT_MPOOL | DB_INIT_TXN | DB_CREATE | DB_PRIVATE;
    int envflags = DB_INIT_LOCK | DB_INIT_LOG | DB_INIT_MPOOL | DB_INIT_TXN | DB_CREATE | DB_PRIVATE;
    r = env->open(env, ENVDIR, envflags, S_IRWXU+S_IRWXG+S_IRWXO);                                            CKERR(r);
    env->set_errfile(env, stderr);
    //Disable auto-checkpointing
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
        r = dbs[i]->set_descriptor(dbs[i], 1, &desc, abort_on_upgrade);                                       CKERR(r);
        dbs[i]->app_private = &idx[i];
        snprintf(name, sizeof(name), "db_%04x", i);
        r = dbs[i]->open(dbs[i], NULL, name, NULL, DB_BTREE, DB_CREATE, 0666);                                CKERR(r);
    }

    generate_permute_tables();

    fwrite_count = 0;
    write_count  = 0;
    if (t == enospc_f)
	fwrite_count_trigger = trigger;
    else
	fwrite_count_trigger = 0;
    if (t == enospc_w)
	write_count_trigger = trigger;
    else
	write_count_trigger = 0;

    db_env_set_func_loader_fwrite(bad_fwrite);
    db_env_set_func_write(bad_write);

    test_loader(t, dbs);

    for(int i=0;i<NUM_DBS;i++) {
        dbs[i]->close(dbs[i], 0);                                                                             CKERR(r);
        dbs[i] = NULL;
    }
    r = env->close(env, 0);                                                                                   CKERR(r);
    toku_free(dbs);
}

// ------------ infrastructure ----------
static void do_args(int argc, char * const argv[]);

int test_main(int argc, char * const *argv) {
    do_args(argc, argv);
    if (verbose) printf("\n\nTesting loader with close and commit (normal)\n");
    run_test(commit, 0);
    if (verbose) printf("\n\nTesting loader with loader abort and txn abort\n");
    run_test(abort_loader, 0);
    if (!USE_PUTS) {
	if (verbose) printf("\n\nTesting loader with loader abort_via_poll and txn abort\n");
	run_test(abort_via_poll, 0);
    }
    if (verbose) printf("\n\nTesting loader with loader close and txn abort\n");
    run_test(abort_txn, 0);
    if (INDUCE_ENOSPC) {
	int i;
	for (i = 1; i < 5; i++) {
	    int trigger = write_count_nominal / i;
	    if (verbose) printf("\n\nTesting loader with enospc induced at write count %d\n", trigger);
	    run_test(enospc_w, trigger);
	}
    }
    {
	int i;
	for (i = 1; i < 5; i++) {
	    int trigger = fwrite_count_nominal / i;
	    if (verbose) printf("\n\nTesting loader with enospc induced at fwrite count %d\n", trigger);
	    run_test(enospc_f, trigger);
	}
    }
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
	    fprintf(stderr, "Usage: -h -c -d <num_dbs> -r <num_rows>\n%s\n", cmd);
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
        } else if (strcmp(argv[0], "-r")==0) {
            argc--; argv++;
            NUM_ROWS = atoi(argv[0]);
        } else if (strcmp(argv[0], "-c")==0) {
            CHECK_RESULTS = 1;
        } else if (strcmp(argv[0], "-p")==0) {
            USE_PUTS = LOADER_USE_PUTS;
	    printf("Using puts\n");
        } else if (strcmp(argv[0], "-e")==0) {
	    INDUCE_ENOSPC = 1;
	    printf("Using enospc\n");
	} else {
	    fprintf(stderr, "Unknown arg: %s\n", argv[0]);
	    resultcode=1;
	    goto do_usage;
	}
	argc--;
	argv++;
    }
}
