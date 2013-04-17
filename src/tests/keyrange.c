/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007-2011 Tokutek Inc.  All rights reserved."
#include "test.h"

// verify that key_range64 returns reasonable results after inserting rows into a tree.
// variations include:
// 1. trickle load versus bulk load
// 2. sequential keys versus random keys
// 3. basements on disk versus basements in memory

#include <db.h>
#include <unistd.h>
#include <sys/stat.h>

static DB_ENV *env = NULL;
static DB_TXN *txn = NULL;
static DB *db = NULL;
static u_int32_t db_page_size = 4096;
static u_int32_t db_basement_size = 4096;
static char *envdir = ENVDIR;
static u_int64_t nrows = 30000;
static bool get_all = true;
static bool use_loader = false;
static bool random_keys = false;

static int 
my_compare(DB *this_db UU(), const DBT *a UU(), const DBT *b UU()) {
    assert(a->size == b->size);
    return memcmp(a->data, b->data, a->size);
}

static int 
my_generate_row(DB *dest_db UU(), DB *src_db UU(), DBT *dest_key UU(), DBT *dest_val UU(), const DBT *src_key UU(), const DBT *src_val UU()) {
    assert(dest_key->flags == DB_DBT_REALLOC);
    dest_key->data = toku_realloc(dest_key->data, src_key->size);
    memcpy(dest_key->data, src_key->data, src_key->size);
    dest_key->size = src_key->size;
    assert(dest_val->flags == DB_DBT_REALLOC);
    dest_val->data = toku_realloc(dest_val->data, src_val->size);
    memcpy(dest_val->data, src_val->data, src_val->size);
    dest_val->size = src_val->size;
    return 0;
}

static void
swap(u_int64_t keys[], u_int64_t i, u_int64_t j) {
    u_int64_t t = keys[i]; keys[i] = keys[j]; keys[j] = t;
}

static u_int64_t 
max64(u_int64_t a, u_int64_t b) {
    return a < b ? b : a;
}

static void 
run_test(void) {
    if (verbose) printf("%s %" PRIu64 "\n", __FUNCTION__, nrows);

    size_t key_size = 9;
    size_t val_size = 9;
    size_t est_row_size_with_overhead = 8 + key_size + 4 + val_size + 4; // xid + key + key_len + val + val)len
    size_t rows_per_basement = db_basement_size / est_row_size_with_overhead;

    int r;
    r = db_env_create(&env, 0); CKERR(r);
    env->set_errfile(env, stderr);
    r = env->set_redzone(env, 0); CKERR(r);
    r = env->set_generate_row_callback_for_put(env, my_generate_row); CKERR(r);
    r = env->set_default_bt_compare(env, my_compare); CKERR(r);
    r = env->open(env, envdir, DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_CREATE|DB_PRIVATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);

    r = db_create(&db, env, 0); CKERR(r);
    r = db->set_pagesize(db, db_page_size);
    r = env->txn_begin(env, 0, &txn, 0); CKERR(r);
    r = db->open(db, txn, "foo.db", 0, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    r = txn->commit(txn, 0);    CKERR(r);

    u_int64_t *keys = toku_malloc(nrows * sizeof (u_int64_t));
    assert(keys);
    for (u_int64_t i = 0; i < nrows; i++)
        keys[i] = 2*i + 1;

    if (random_keys)
        for (u_int64_t i = 0; i < nrows; i++)
            swap(keys, random() % nrows, random() % nrows);

    // insert keys 1, 3, 5, ... 2*(nrows-1) + 1
    r = env->txn_begin(env, 0, &txn, 0); CKERR(r);
    if (use_loader) {
        DB_LOADER *loader = NULL;
        r = env->create_loader(env, txn, &loader, db, 1, &db, NULL, NULL, 0); CKERR(r);
        for (u_int64_t i=0; i<nrows; i++) {
            char key[100],val[100];
            snprintf(key, sizeof key, "%08llu", (unsigned long long)keys[i]);
            snprintf(val, sizeof val, "%08llu", (unsigned long long)keys[i]);
	    assert(1+strlen(key) == key_size && 1+strlen(val) == val_size);
            DBT k,v;
            r = loader->put(loader, dbt_init(&k, key, 1+strlen(key)), dbt_init(&v,val, 1+strlen(val))); CKERR(r);
        }
        r = loader->close(loader); CKERR(r);
    } else {
        for (u_int64_t i=0; i<nrows; i++) {
            char key[100],val[100];
            snprintf(key, sizeof key, "%08llu", (unsigned long long)keys[i]);
            snprintf(val, sizeof val, "%08llu", (unsigned long long)keys[i]);
	    assert(1+strlen(key) == key_size && 1+strlen(val) == val_size);
            DBT k,v;
            r = db->put(db, txn, dbt_init(&k, key, 1+strlen(key)), dbt_init(&v,val, 1+strlen(val)), 0); CKERR(r);
        }
    }
    r = txn->commit(txn, 0);    CKERR(r);

    // close and reopen to get rid of basements
    r = db->close(db, 0); CKERR(r); // close MUST flush the nodes of this db out of the cache table for this test to be valid
    r = db_create(&db, env, 0); CKERR(r);
    r = env->txn_begin(env, 0, &txn, 0); CKERR(r);
    r = db->open(db, txn, "foo.db", 0, DB_BTREE, 0, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
    r = txn->commit(txn, 0); CKERR(r);

    r = env->txn_begin(env, 0, &txn, 0); CKERR(r);

    if (get_all) {
        // read the basements into memory
        for (u_int64_t i=0; i<nrows; i++) {
            char key[100];
            snprintf(key, 100, "%08llu", (unsigned long long)2*i+1);
            DBT k,v;
            memset(&v, 0, sizeof(v));
            r = db->get(db, txn, dbt_init(&k, key, 1+strlen(key)), &v, 0); CKERR(r);
        }
    }

    DB_BTREE_STAT64 s64;
    r = db->stat64(db, txn, &s64); CKERR(r);
    if (verbose) 
        printf("stats %" PRId64 " %" PRId64 "\n", s64.bt_nkeys, s64.bt_dsize);
    if (use_loader) {
	assert(s64.bt_nkeys == nrows);
	assert(s64.bt_dsize == nrows * (key_size + val_size));
    } else {
	assert(0 < s64.bt_nkeys && s64.bt_nkeys <= nrows);
	assert(0 < s64.bt_dsize && s64.bt_dsize <= nrows * (key_size + val_size));
    }

    if (0) goto skipit; // debug: just write the tree

    // verify key_range for keys that exist in the tree
    for (u_int64_t i=0; i<nrows; i++) {
	char key[100];
	snprintf(key, 100, "%08llu", (unsigned long long)2*i+1);
	DBT k;
	u_int64_t less,equal,greater;
	int is_exact;
	r = db->key_range64(db, txn, dbt_init(&k, key, 1+strlen(key)), &less, &equal, &greater, &is_exact); CKERR(r);
	if (verbose)
            printf("key %llu/%llu %llu %llu %llu %llu\n", (unsigned long long)2*i, (unsigned long long)2*nrows, (unsigned long long)less, (unsigned long long)equal, (unsigned long long)greater,
                   (unsigned long long)(less+equal+greater));
        assert(is_exact == 0);
        assert(0 < less + equal + greater);
        if (use_loader) {
            assert(less + equal + greater <= nrows);
            assert(get_all ? equal == 1 : equal == 0);
            assert(less <= max64(i, i + rows_per_basement/2));
            assert(greater <= nrows - less);
        } else {
            assert(less + equal + greater <= nrows + nrows / 8);
            assert(get_all ? equal == 1 : equal == 0);
            u_int64_t est_i = max64(i, i + rows_per_basement/2);
            assert(less <= est_i + est_i / 1);
            assert(greater <= nrows - i + rows_per_basement/2);
	}
    }

    // verify key range for keys that do not exist in the tree
    for (u_int64_t i=0; i<1+nrows; i++) {
	char key[100];
	snprintf(key, 100, "%08llu", (unsigned long long)2*i);
	DBT k;
	u_int64_t less,equal,greater;
	int is_exact;
	r = db->key_range64(db, txn, dbt_init(&k, key, 1+strlen(key)), &less, &equal, &greater, &is_exact); CKERR(r);
	if (verbose) 
            printf("key %llu/%llu %llu %llu %llu %llu\n", (unsigned long long)2*i, (unsigned long long)2*nrows, (unsigned long long)less, (unsigned long long)equal, (unsigned long long)greater,
                   (unsigned long long)(less+equal+greater));
        assert(is_exact == 0);
        assert(0 < less + equal + greater);
        if (use_loader) {
            assert(less + equal + greater <= nrows);
            assert(equal == 0);
            assert(less <= max64(i, i + rows_per_basement/2));
            assert(greater <= nrows - less);
        } else {
            assert(less + equal + greater <= nrows + nrows / 8);
            assert(equal == 0);
            u_int64_t est_i = max64(i, i + rows_per_basement/2);
            assert(less <= est_i + est_i / 1);
            assert(greater <= nrows - i + rows_per_basement/2);
        }
    }

 skipit:
    r = txn->commit(txn, 0);    CKERR(r);
    r = db->close(db, 0);     CKERR(r);
    r = env->close(env, 0);   CKERR(r);
    
    toku_free(keys);
}

static int 
usage(void) {
    fprintf(stderr, "-v (verbose)\n");
    fprintf(stderr, "-q (quiet)\n");
    fprintf(stderr, "--envdir %s\n", envdir);
    fprintf(stderr, "--nrows %" PRIu64 " (number of rows)\n", nrows);
    fprintf(stderr, "--nrows %" PRIu64 " (number of rows)\n", nrows);
    fprintf(stderr, "--loader %u (use the loader to load the keys)\n", use_loader);
    fprintf(stderr, "--get %u (get all keys before keyrange)\n", get_all);
    fprintf(stderr, "--random_keys %u\n", random_keys);
    fprintf(stderr, "--page_size %u\n", db_page_size);
    fprintf(stderr, "--basement_size %u\n", db_basement_size);
    return 1;
}

int
test_main (int argc , char * const argv[]) {
    for (int i = 1 ; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose++;
            continue;
        }
        if (strcmp(argv[i], "-q") == 0) {
            if (verbose > 0)
                verbose--;
            continue;
        }
        if (strcmp(argv[i], "--envdir") == 0 && i+1 < argc) {
            envdir = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "--nrows") == 0 && i+1 < argc) {
            nrows = atoll(argv[++i]);
            continue;
        }
        if (strcmp(argv[i], "--get") == 0 && i+1 < argc) {
            get_all = atoi(argv[++i]) != 0;
            continue;
        }
        if (strcmp(argv[i], "--loader") == 0 && i+1 < argc) {
            use_loader = atoi(argv[++i]) != 0;
            continue;
        }
        if (strcmp(argv[i], "--random_keys") == 0 && i+1 < argc) {
            random_keys = atoi(argv[++i]) != 0;
            continue;
        }
        if (strcmp(argv[i], "--page_size") == 0 && i+1 < argc) {
            db_page_size = atoi(argv[++i]);
            continue;
        }
        if (strcmp(argv[i], "--basement_size") == 0 && i+1 < argc) {
            db_basement_size = atoi(argv[++i]);
            continue;
        }
        return usage();
    }

    char rmcmd[32 + strlen(envdir)]; 
    snprintf(rmcmd, sizeof rmcmd, "rm -rf %s", envdir);
    int r;
    r = system(rmcmd); CKERR(r);
    r = toku_os_mkdir(envdir, S_IRWXU+S_IRWXG+S_IRWXO);       CKERR(r);

    run_test();

    return 0;
}
