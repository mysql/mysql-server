/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2010 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#include "test.h"
#include "toku_pthread.h"
#include <db.h>
#include <sys/stat.h>

bool fast = false;

DB_ENV *env;
enum {NUM_DBS=2};
int USE_PUTS=0;

uint32_t num_rows = 1;
uint32_t which_db_to_fail = -1;
uint32_t which_row_to_fail = -1;
enum how_to_fail { FAIL_NONE, FAIL_KSIZE, FAIL_VSIZE } how_to_fail = FAIL_NONE;

static int put_multiple_generate(DB *dest_db,
				 DB *src_db __attribute__((__unused__)),
				 DBT *dest_key, DBT *dest_val,
				 const DBT *src_key, const DBT *src_val __attribute__((__unused__))) {

    uint32_t which = *(uint32_t*)dest_db->app_private;
    assert(src_key->size==4);
    uint32_t rownum = *(uint32_t*)src_key->data;

    uint32_t ksize, vsize;
    const uint32_t kmax=32*1024, vmax=32*1024*1024;
    if (which==which_db_to_fail && rownum==which_row_to_fail) {
	switch (how_to_fail) {
	case FAIL_NONE:  ksize=kmax;   vsize=vmax;   goto gotsize;
	case FAIL_KSIZE: ksize=kmax+1; vsize=vmax;   goto gotsize;
	case FAIL_VSIZE: ksize=kmax;   vsize=vmax+1; goto gotsize;
	}
	assert(0);
    gotsize:;
    } else {
	ksize=4; vsize=100;
    }
    assert(dest_key->flags==DB_DBT_REALLOC);
    if (dest_key->ulen < ksize) {
	dest_key->data = toku_xrealloc(dest_key->data, ksize);
	dest_key->ulen = ksize;
    }
    assert(dest_val->flags==DB_DBT_REALLOC);
    if (dest_val->ulen < vsize) {
	dest_val->data = toku_xrealloc(dest_val->data, vsize);
	dest_val->ulen = vsize;
    }
    assert(ksize>=sizeof(uint32_t));
    for (uint32_t i=0; i<ksize; i++) ((char*)dest_key->data)[i] = random();
    for (uint32_t i=0; i<vsize; i++) ((char*)dest_val->data)[i] = random();
    *(uint32_t*)dest_key->data = rownum;
    dest_key->size = ksize;
    dest_val->size = vsize;

    return 0;
}

struct error_extra {
    int bad_i;
    int error_count;
};

static void error_callback (DB *db __attribute__((__unused__)), int which_db, int err, DBT *key __attribute__((__unused__)), DBT *val __attribute__((__unused__)), void *extra) {
    struct error_extra *e =(struct error_extra *)extra;
    assert(which_db==(int)which_db_to_fail);
    assert(err==EINVAL);
    assert(e->error_count==0);
    e->error_count++;
}

static void test_loader_maxsize(DB **dbs)
{
    int r;
    DB_TXN    *txn;
    DB_LOADER *loader;
    uint32_t db_flags[NUM_DBS];
    uint32_t dbt_flags[NUM_DBS];
    for(int i=0;i<NUM_DBS;i++) { 
        db_flags[i] = DB_NOOVERWRITE; 
        dbt_flags[i] = 0;
    }
    uint32_t loader_flags = USE_PUTS; // set with -p option

    // create and initialize loader
    r = env->txn_begin(env, NULL, &txn, 0);                                                               
    CKERR(r);
    r = env->create_loader(env, txn, &loader, dbs[0], NUM_DBS, dbs, db_flags, dbt_flags, loader_flags);
    CKERR(r);
    struct error_extra error_extra = {.error_count=0};
    r = loader->set_error_callback(loader, error_callback, (void*)&error_extra);
    CKERR(r);
    r = loader->set_poll_function(loader, NULL, NULL);
    CKERR(r);

    // using loader->put, put values into DB
    DBT key, val;
    unsigned int k, v;
    for(uint32_t i=0;i<num_rows;i++) {
        k = i;
        v = i;
        dbt_init(&key, &k, sizeof(unsigned int));
        dbt_init(&val, &v, sizeof(unsigned int));
        r = loader->put(loader, &key, &val);
        if (USE_PUTS) {
            //PUT loader can return -1 if it finds an error during the puts.
            CKERR2s(r, 0,-1);
        }
        else {
            CKERR(r);
        }
    }

    // close the loader
    if (verbose) { printf("closing"); fflush(stdout); }
    r = loader->close(loader);
    if (verbose) {  printf(" done\n"); }
    switch(how_to_fail) {
    case FAIL_NONE:  assert(r==0);      assert(error_extra.error_count==0); goto checked;
    case FAIL_KSIZE: assert(r==EINVAL); assert(error_extra.error_count==1); goto checked;
    case FAIL_VSIZE: assert(r==EINVAL); assert(error_extra.error_count==1); goto checked;
    }
    assert(0);
 checked:

    r = txn->commit(txn, 0);
    CKERR(r);
}

char *free_me = NULL;
char *env_dir = ENVDIR; // the default env_dir

static void run_test(uint32_t nr, uint32_t wdb, uint32_t wrow, enum how_to_fail htf) {
    num_rows = nr; which_db_to_fail = wdb; which_row_to_fail = wrow; how_to_fail = htf;
    int r;
    {
	int len = strlen(env_dir) + 20;
	char syscmd[len];
	r = snprintf(syscmd, len, "rm -rf %s", env_dir);
	assert(r<len);
	r = system(syscmd);                                                                                   CKERR(r);
    }
    r = toku_os_mkdir(env_dir, S_IRWXU+S_IRWXG+S_IRWXO);                                                       CKERR(r);

    r = db_env_create(&env, 0);                                                                               CKERR(r);
    r = env->set_default_bt_compare(env, uint_dbt_cmp);                                                       CKERR(r);
    r = env->set_generate_row_callback_for_put(env, put_multiple_generate);
    CKERR(r);
    int envflags = DB_INIT_LOCK | DB_INIT_MPOOL | DB_INIT_TXN | DB_INIT_LOG | DB_CREATE | DB_PRIVATE;
    r = env->open(env, env_dir, envflags, S_IRWXU+S_IRWXG+S_IRWXO);                                            CKERR(r);
    env->set_errfile(env, stderr);
    //Disable auto-checkpointing
    r = env->checkpointing_set_period(env, 0);                                                                CKERR(r);

    DBT desc;
    dbt_init(&desc, "foo", sizeof("foo"));
    enum {MAX_NAME=128};
    char name[MAX_NAME*2];

    DB **dbs = (DB**)toku_malloc(sizeof(DB*) * NUM_DBS);
    assert(dbs != NULL);
    int idx[NUM_DBS];
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

    if (verbose) printf("running test_loader()\n");
    // -------------------------- //
    test_loader_maxsize(dbs);
    // -------------------------- //
    if (verbose) printf("done    test_loader()\n");

    for(int i=0;i<NUM_DBS;i++) {
        dbs[i]->close(dbs[i], 0);                                                                             CKERR(r);
        dbs[i] = NULL;
    }
    r = env->close(env, 0);                                                                                   CKERR(r);
    toku_free(dbs);
}

// ------------ infrastructure ----------
static void do_args(int argc, char * const argv[]);

int num_rows_set = FALSE;

int test_main(int argc, char * const *argv) {
    do_args(argc, argv);

    run_test(1, -1, -1, FAIL_NONE);
    run_test(1,  0,  0, FAIL_NONE);
    run_test(1,  0,  0, FAIL_KSIZE);
    run_test(1,  0,  0, FAIL_VSIZE);
    if (!fast) {
	run_test(1000000, 0, 500000, FAIL_KSIZE);
	run_test(1000000, 0, 500000, FAIL_VSIZE);
    }
    toku_free(free_me);
    return 0;
}

static void do_args(int argc, char * const argv[]) {
    int resultcode;
    char *cmd = argv[0];
    argc--; argv++;
    while (argc>0) {
        if (strcmp(argv[0], "-h")==0) {
            resultcode=0;
	do_usage:
	    fprintf(stderr, "Usage: %s [-h] [-v] [-q] [-p] [-f] [ -e <envdir> ]\n", cmd);
	    fprintf(stderr, " where -e <env>         uses <env> to construct the directory (so that different tests can run concurrently)\n");
	    fprintf(stderr, "       -h               help\n");
	    fprintf(stderr, "       -v               verbose\n");
	    fprintf(stderr, "       -q               quiet\n");
	    fprintf(stderr, "       -p               use DB->put\n");
	    fprintf(stderr, "       -f               fast (suitable for vgrind)\n");
	    exit(resultcode);
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
        } else if (strcmp(argv[0], "-p")==0) {
            USE_PUTS = 1;
        } else if (strcmp(argv[0], "-f")==0) {
	    fast     = true;
	} else {
	    fprintf(stderr, "Unknown arg: %s\n", argv[0]);
	    resultcode=1;
	    goto do_usage;
	}
	argc--;
	argv++;
    }
}
