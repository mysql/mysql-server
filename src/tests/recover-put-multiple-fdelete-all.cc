/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
// this test makes sure the LSN filtering is used during recovery of put_multiple

#include <sys/stat.h>
#include <fcntl.h>
#include "test.h"

const int envflags = DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE;

const char *namea="a.db";
const char *nameb="b.db";
enum {num_dbs = 2};
static DBT dest_keys[num_dbs];
static DBT dest_vals[num_dbs];

bool do_test=false, do_recover=false;

static int
put_multiple_generate(DB *dest_db, DB *src_db, DBT *dest_key, DBT *dest_val, const DBT *src_key, const DBT *src_val) {
    assert(src_db == NULL);
    assert(dest_db->descriptor->dbt.size == 4);
    uint32_t which = *(uint32_t*)dest_db->descriptor->dbt.data;
    assert(which < num_dbs);

    if (dest_key->data) toku_free(dest_key->data);
    if (dest_val->data) toku_free(dest_val->data);
    dest_key->data = toku_xmemdup (src_key->data, src_key->size);
    dest_key->size = src_key->size;
    dest_val->data = toku_xmemdup (src_val->data, src_val->size);
    dest_val->size = src_val->size;
    return 0;
}

static void run_test (void) {
    int r;

    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO);

    DB_ENV *env;
    r = db_env_create(&env, 0);                                                         CKERR(r);
    r = env->set_generate_row_callback_for_put(env, put_multiple_generate);
    CKERR(r);
    r = env->open(env, TOKU_TEST_FILENAME, envflags, S_IRWXU+S_IRWXG+S_IRWXO);                      CKERR(r);

    // create a txn that never closes, forcing recovery to run from the beginning of the log
    {
        DB_TXN *oldest_living_txn;
        r = env->txn_begin(env, NULL, &oldest_living_txn, 0);                                         CKERR(r);
    }

    DBT descriptor;
    uint32_t which;
    for (which = 0; which < num_dbs; which++) {
        dbt_init_realloc(&dest_keys[which]);
        dbt_init_realloc(&dest_vals[which]);
    }
    dbt_init(&descriptor, &which, sizeof(which));
    DB *dba;
    DB *dbb;
    r = db_create(&dba, env, 0);                                                        CKERR(r);
    r = db_create(&dbb, env, 0);                                                        CKERR(r);
    r = dba->open(dba, NULL, namea, NULL, DB_BTREE, DB_AUTO_COMMIT|DB_CREATE, 0666);    CKERR(r);
    which = 0;
    IN_TXN_COMMIT(env, NULL, txn_desc, 0, {
            { int chk_r = dba->change_descriptor(dba, txn_desc, &descriptor, 0); CKERR(chk_r); }
    });
    r = dbb->open(dbb, NULL, nameb, NULL, DB_BTREE, DB_AUTO_COMMIT|DB_CREATE, 0666);    CKERR(r);
    which = 1;
    IN_TXN_COMMIT(env, NULL, txn_desc, 0, {
            { int chk_r = dbb->change_descriptor(dbb, txn_desc, &descriptor, 0); CKERR(chk_r); }
    });

    DB *dbs[num_dbs] = {dba, dbb};
    uint32_t flags[num_dbs] = {0, 0};
    // txn_begin; insert <a,a>; txn_abort
    {
        DB_TXN *txn;
        r = env->txn_begin(env, NULL, &txn, 0);                                         CKERR(r);
        DBT k,v;
        dbt_init(&k, "a", 2);
        dbt_init(&v, "b", 2);

        r = env->put_multiple(env, NULL, txn, &k, &v, num_dbs, dbs, dest_keys, dest_vals, flags);
        CKERR(r);
        r = txn->abort(txn);                                                            CKERR(r);
    }
    r = dbb->close(dbb, 0);                                                             CKERR(r);
    r = db_create(&dbb, env, 0);                                                        CKERR(r);
    r = dbb->open(dbb, NULL, nameb, NULL, DB_BTREE, DB_AUTO_COMMIT, 0666);    CKERR(r);
    dbs[1] = dbb;

    // txn_begin; insert <a,b>;
    {
        DB_TXN *txn;
        r = env->txn_begin(env, NULL, &txn, 0);                                         CKERR(r);
        DBT k,v;
        dbt_init(&k, "a", 2);
        dbt_init(&v, "b", 2);

        r = env->put_multiple(env, NULL, txn, &k, &v, num_dbs, dbs, dest_keys, dest_vals, flags);
        CKERR(r);
        r = txn->commit(txn, 0);                                                        CKERR(r);
    }
    {
        DB_TXN *txn;
        r = env->txn_begin(env, NULL, &txn, 0);                                         CKERR(r);
        r = dba->close(dba, 0);                                                         CKERR(r);
        r = env->dbremove(env, txn, namea, NULL, 0);                                    CKERR(r);
        r = dbb->close(dbb, 0);                                                         CKERR(r);
        r = env->dbremove(env, txn, nameb, NULL, 0);                                    CKERR(r);
        r = txn->commit(txn, 0);                                                        CKERR(r);
    }

    r = env->log_flush(env, NULL); CKERR(r);
    // abort the process
    toku_hard_crash_on_purpose();
}


static void run_recover (void) {
    DB_ENV *env;
    int r;

    // Recovery starts from oldest_living_txn, which is older than any inserts done in run_test,
    // so recovery always runs over the entire log.

    // run recovery
    r = db_env_create(&env, 0);                                                         CKERR(r);
    r = env->set_generate_row_callback_for_put(env, put_multiple_generate);
    CKERR(r);
    r = env->open(env, TOKU_TEST_FILENAME, envflags + DB_RECOVER, S_IRWXU+S_IRWXG+S_IRWXO);         CKERR(r);

    // verify the data
    {
        DB *db;
        r = db_create(&db, env, 0);                                                         CKERR(r);
        r = db->open(db, NULL, namea, NULL, DB_UNKNOWN, DB_AUTO_COMMIT, 0666);              CKERR2(r, ENOENT);
        r = db->close(db, 0);                                                               CKERR(r);
    }
    {
        DB *db;
        r = db_create(&db, env, 0);                                                         CKERR(r);
        r = db->open(db, NULL, nameb, NULL, DB_UNKNOWN, DB_AUTO_COMMIT, 0666);              CKERR2(r, ENOENT);
        r = db->close(db, 0);                                                               CKERR(r);
    }
    r = env->close(env, 0);                                                             CKERR(r);
    exit(0);
}

const char *cmd;

static void test_parse_args (int argc, char * const argv[]) {
    int resultcode;
    cmd = argv[0];
    argc--; argv++;
    while (argc>0) {
	if (strcmp(argv[0], "-v") == 0) {
	    verbose++;
	} else if (strcmp(argv[0],"-q")==0) {
	    verbose--;
	    if (verbose<0) verbose=0;
	} else if (strcmp(argv[0], "--test")==0) {
	    do_test=true;
        } else if (strcmp(argv[0], "--recover") == 0) {
            do_recover=true;
	} else if (strcmp(argv[0], "-h")==0) {
	    resultcode=0;
	do_usage:
	    fprintf(stderr, "Usage:\n%s [-v|-q]* [-h] {--test | --recover } \n", cmd);
	    exit(resultcode);
	} else {
	    fprintf(stderr, "Unknown arg: %s\n", argv[0]);
	    resultcode=1;
	    goto do_usage;
	}
	argc--;
	argv++;
    }
}

int test_main (int argc, char * const argv[]) {
    test_parse_args(argc, argv);
    if (do_test) {
	run_test();
    } else if (do_recover) {
        run_recover();
    }
    return 0;
}
