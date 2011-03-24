// measure the cost of closing db's with a full cache table

// create db 0 with txn 0
// create db's 1..N-1 with auto txn1
// fill the cache table with blocks for db 0
// close db 1..N-1 (these should be fast)
// close db 0
// abort txn 0

#include "test.h"
#include <byteswap.h>

static long htonl64(long x) {
#if BYTE_ORDER == LITTLE_ENDIAN
    return bswap_64(x);
#else
#error
#endif
}

static inline float tdiff (struct timeval *a, struct timeval *b) {
    return (a->tv_sec - b->tv_sec) +1e-6*(a->tv_usec - b->tv_usec);
}

static void
insert_row(DB_ENV *env UU(), DB_TXN *txn, DB *db, uint64_t rowi) {
    int r;

    // generate the key
    char key_buffer[8];
    uint64_t k = htonl64(rowi);
    memcpy(key_buffer, &k, sizeof k);

    // generate the val
    char val_buffer[1024];
    memset(val_buffer, 0, sizeof val_buffer);

    DBT key = { .data = key_buffer, .size = sizeof key_buffer };
    DBT value = { .data = val_buffer, .size = sizeof val_buffer };
    //uint32_t put_flags = DB_YESOVERWRITE | (txn ? (DB_PRELOCKED_FILE_READ | DB_PRELOCKED_WRITE) : 0);
    uint32_t put_flags = DB_YESOVERWRITE;
    r = db->put(db, txn, &key, &value, put_flags); assert(r == 0);
}

static void
populate(DB_ENV *env, DB_TXN *txn, DB *db, uint64_t nrows) {
    int r;
    struct timeval tstart;
    r = gettimeofday(&tstart, NULL); assert(r == 0);
    struct timeval tlast = tstart;

    for (uint64_t rowi = 0; rowi < nrows; rowi++) {
        insert_row(env, txn, db, rowi); 

        // maybe report performance
        uint64_t rows_per_report = 1000;
        if (((rowi + 1) % rows_per_report) == 0) {
            struct timeval tnow;
            r = gettimeofday(&tnow, NULL); assert(r == 0);
            float last_time = tdiff(&tnow, &tlast);
            float total_time = tdiff(&tnow, &tstart);
            if (verbose) {
                fprintf(stderr, "%ld %.3f %.0f/s %.0f/s\n", rowi + 1, last_time, rows_per_report/last_time, rowi/total_time); fflush(stderr);
            }
            tlast = tnow;
        }
    }
}

static void
run_test(DB_ENV *env, int ndbs, int do_txn, uint32_t pagesize, uint64_t nrows) {
    int r;

    DB_TXN *txn0 = NULL;
    if (do_txn) {
        r = env->txn_begin(env, NULL, &txn0, 0); assert(r == 0);
    }

    DB *dbs[ndbs];
    int i = 0;
    {
        DB *db = NULL;
        if (verbose) fprintf(stderr, "creating %d\n", i);
        r = db_create(&db, env, 0); assert(r == 0);
        if (pagesize) {
            r = db->set_pagesize(db, pagesize); assert(r == 0);
        }
        char db_filename[32]; sprintf(db_filename, "test%d", i);
        r = db->open(db, txn0, db_filename, NULL, DB_BTREE, DB_CREATE, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH); assert(r == 0);
        if (do_txn) {
            r = db->pre_acquire_table_lock(db, txn0); assert(r == 0);
        }
        dbs[i] = db;
    }

    for (i = 1; i < ndbs; i++) {
        DB *db = NULL;
        if (verbose) fprintf(stderr, "creating %d\n", i);
        r = db_create(&db, env, 0); assert(r == 0);
        if (pagesize) {
            r = db->set_pagesize(db, pagesize); assert(r == 0);
        }
        DB_TXN *txn1 = NULL;
        if (do_txn) {
            r = env->txn_begin(env, NULL, &txn1, 0); assert(r == 0);
        }
        char db_filename[32]; sprintf(db_filename, "test%d", i);
        r = db->open(db, txn1, db_filename, NULL, DB_BTREE, DB_CREATE, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH); assert(r == 0);
        if (do_txn) {
            r = txn1->commit(txn1, 0); assert(r == 0);
        }
        dbs[i] = db;
    }

    if (verbose) fprintf(stderr, "populating\n");
    populate(env, txn0, dbs[0], nrows);

    for (i = 1; i < ndbs; i++) {
        DB *db = dbs[i];
        if (verbose) fprintf(stderr, "closing %d\n", i);
        r = db->close(db, 0); assert(r == 0);
    }

    if (verbose) fprintf(stderr, "closing %d\n", 0);
    r = dbs[0]->close(dbs[0], 0); assert(r == 0);
    if (do_txn) {
        if (verbose) fprintf(stderr, "abort txn0\n");
        r = txn0->abort(txn0); assert(r == 0);
    }
}

int 
test_main(int argc, char * const argv[]) {
    char *env_dir = "dir.shutdown.ca";
    int ndbs = 1;
    int do_txn = 1;
    u_int32_t pagesize = 4096;
    u_int64_t cachesize = 1000000000;
    u_int64_t nrows = 500000;

    for (int i = 1; i < argc ; i++) {
        char * const arg = argv[i];
        if (strcmp(arg, "-v") == 0) {
            verbose++;
            continue;
        }
        if (strcmp(arg, "-q") == 0) {
            if (verbose > 0) verbose--;
            continue;
        }
        if (strcmp(arg, "--txn") == 0 && i+1 < argc) {
            do_txn = atoi(argv[++i]);
            continue;
        }
        if (strcmp(arg, "--ndbs") == 0 && i+1 < argc) {
            ndbs = atoi(argv[++i]);
            continue;
        }
        if (strcmp(arg, "--pagesize") == 0 && i+1 < argc) {
            pagesize = atoi(argv[++i]);
            continue;
        }
        if (strcmp(arg, "--cachesize") == 0 && i+1 < argc) {
            cachesize = atol(argv[++i]);
            continue;
        }
        if (strcmp(arg, "--rows") == 0 && i+1 < argc) {
            nrows = atol(argv[++i]);
            continue;
        }

        assert(0);
    }

    // create clean env dir
    char rm_cmd[strlen(env_dir) + strlen("rm -rf ") + 1];
    snprintf(rm_cmd, sizeof(rm_cmd), "rm -rf %s", env_dir);
    int r;
    r = system(rm_cmd); assert(r == 0);
    r = toku_os_mkdir(env_dir, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH); assert(r == 0);

    DB_ENV *env = NULL;
    r = db_env_create(&env, 0); assert(r == 0);
    if (cachesize) {
        const u_int64_t gig = 1 << 30;
        r = env->set_cachesize(env, cachesize / gig, cachesize % gig, 1); assert(r == 0);
    }
    int env_open_flags = DB_CREATE | DB_PRIVATE | DB_INIT_MPOOL | DB_INIT_TXN | DB_INIT_LOCK | DB_INIT_LOG;
    if (!do_txn)
        env_open_flags &= ~(DB_INIT_TXN | DB_INIT_LOG);
    r = env->open(env, env_dir, env_open_flags, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH); assert(r == 0);

    run_test(env, ndbs, do_txn, pagesize, nrows);

    if (verbose) fprintf(stderr, "closing env\n");
    r = env->close(env, 0); assert(r == 0);

    return 0;
}
