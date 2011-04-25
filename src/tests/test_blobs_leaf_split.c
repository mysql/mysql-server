// verify that the leaf split code can handle rows larger than nodesize

#include "test.h"

static void insert(DB *db, DB_TXN *txn, int k, int val_size) {
    int r;

    // generate the key
    char key_buffer[8]; 
    memset(key_buffer, 0, sizeof key_buffer);
    int newa = htonl(k);
    memcpy(key_buffer, &newa, sizeof newa);

    // generate the value
    char *val_buffer = toku_malloc(val_size); assert(val_buffer);
    memset(val_buffer, 0, val_size);
    
    DBT key = { .data = key_buffer, .size = sizeof key_buffer };
    DBT value = { .data = val_buffer, .size = val_size };
    r = db->put(db, txn, &key, &value, 0); assert(r == 0);

    toku_free(val_buffer);
}

int test_main(int argc, char * const argv[]) {
#if defined(TOKUDB)
    char *db_env_dir = "dir.blobs.leafsplit.env.tdb";
#else
    char *db_env_dir = "dir.blobs.leafsplit.env.bdb";
#endif
    int db_env_open_flags = DB_CREATE | DB_PRIVATE | DB_INIT_MPOOL | DB_INIT_TXN | DB_INIT_LOCK | DB_INIT_LOG;
    char *db_filename = "blobs.db";
    int do_txn = 1;
    u_int64_t cachesize = 0;
    u_int32_t pagesize = 0;

    int i;
    for (i = 1; i < argc; i++) {
        char *arg = argv[i];
        if (strcmp(arg, "-v") == 0 || strcmp(arg, "--verbose") == 0) {
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
        if (strcmp(arg, "--pagesize") == 0 && i+1 < argc) {
            pagesize = atoi(argv[++i]);
            continue;
        }
        if (strcmp(arg, "--cachesize") == 0 && i+1 < argc) {
            cachesize = atol(argv[++i]);
            continue;
        }

        assert(0);
    }

    int r;
    char rm_cmd[strlen(db_env_dir) + strlen("rm -rf ") + 1];
    snprintf(rm_cmd, sizeof(rm_cmd), "rm -rf %s", db_env_dir);
    r = system(rm_cmd); assert(r == 0);

    r = toku_os_mkdir(db_env_dir, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH); assert(r == 0);

    // create and open the env
    DB_ENV *db_env = NULL;
    r = db_env_create(&db_env, 0); assert(r == 0);
    if (cachesize) {
        const u_int64_t gig = 1 << 30;
        r = db_env->set_cachesize(db_env, cachesize / gig, cachesize % gig, 1); assert(r == 0);
    }
    if (!do_txn)
        db_env_open_flags &= ~(DB_INIT_TXN | DB_INIT_LOG);
    r = db_env->open(db_env, db_env_dir, db_env_open_flags, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH); assert(r == 0);

    // create the db
    DB *db = NULL;
    r = db_create(&db, db_env, 0); assert(r == 0);
    DB_TXN *create_txn = NULL;
    if (do_txn) {
        r = db_env->txn_begin(db_env, NULL, &create_txn, 0); assert(r == 0);
    }
    if (pagesize) {
        r = db->set_pagesize(db, pagesize); assert(r == 0);
    }
    r = db->open(db, create_txn, db_filename, NULL, DB_BTREE, DB_CREATE, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH); assert(r == 0);

    insert(db, create_txn, 1, 8000000);
    insert(db, create_txn, 2, 1);

    if (do_txn) {
        r = create_txn->commit(create_txn, 0); assert(r == 0);
    }

    // shutdown
    r = db->close(db, 0); assert(r == 0); db = NULL;
    r = db_env->close(db_env, 0); assert(r == 0); db_env = NULL;

    return 0;
}
