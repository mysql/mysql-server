// this test makes sure the LSN filtering is used during recovery of put_multiple

#include <sys/stat.h>
#include <fcntl.h>
#include "test.h"

const int envflags = DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE;

char *namea="a.db";
char *nameb="b.db";
enum {num_dbs = 2};
static DBT dest_keys[num_dbs];
static DBT dest_vals[num_dbs];

BOOL do_test=FALSE, do_recover=FALSE;

static int
crash_on_upgrade(DB* db,
                 u_int32_t old_version, const DBT *old_descriptor, const DBT *old_key, const DBT *old_val,
                 u_int32_t new_version, const DBT *new_descriptor, const DBT *new_key, const DBT *new_val) {
    db = db;
    old_version = old_version;
    old_descriptor = old_descriptor;
    old_key = old_key;
    old_val = old_val;
    new_version = new_version;
    new_descriptor = new_descriptor;
    new_key = new_key;
    new_val = new_val;
    assert(FALSE);
    return 0;
}

static int
put_multiple_generate(DB *dest_db, DB *src_db, DBT *dest_key, DBT *dest_val, const DBT *src_key, const DBT *src_val, void *extra) {
    if (extra == NULL) {
        if (src_db) {
            assert(src_db->descriptor);
            assert(src_db->descriptor->size == 4);
            assert((*(uint32_t*)src_db->descriptor->data) == 0);
        }
    }
    else {
        assert(src_db == NULL);
        assert(extra==&namea); //Verifying extra gets set right.
    }
    assert(dest_db->descriptor->size == 4);
    uint32_t which = *(uint32_t*)dest_db->descriptor->data;
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

    system("rm -rf " ENVDIR);
    toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);

    DB_ENV *env;
    r = db_env_create(&env, 0);                                                         CKERR(r);
    r = env->set_generate_row_callback_for_put(env, put_multiple_generate);
    CKERR(r);
    r = env->open(env, ENVDIR, envflags, S_IRWXU+S_IRWXG+S_IRWXO);                      CKERR(r);

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
    which = 0;
    r = dba->set_descriptor(dba, 1, &descriptor, crash_on_upgrade);                     CKERR(r);
    which = 1;
    r = dbb->set_descriptor(dbb, 1, &descriptor, crash_on_upgrade);                     CKERR(r);
    r = dba->open(dba, NULL, namea, NULL, DB_BTREE, DB_AUTO_COMMIT|DB_CREATE, 0666);    CKERR(r);
    r = dbb->open(dbb, NULL, nameb, NULL, DB_BTREE, DB_AUTO_COMMIT|DB_CREATE, 0666);    CKERR(r);

    DB *dbs[num_dbs] = {dba, dbb};
    uint32_t flags[num_dbs] = {DB_YESOVERWRITE, DB_YESOVERWRITE};
    // txn_begin; insert <a,a>; txn_abort
    {
        DB_TXN *txn;
        r = env->txn_begin(env, NULL, &txn, 0);                                         CKERR(r);
	DBT k={.data="a", .size=2};
	DBT v={.data="a", .size=2};

        r = env->put_multiple(env, dba, txn, &k, &v, num_dbs, dbs, dest_keys, dest_vals, flags, NULL);
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
	DBT k={.data="a", .size=2};
	DBT v={.data="b", .size=2};

        r = env->put_multiple(env, NULL, txn, &k, &v, num_dbs, dbs, dest_keys, dest_vals, flags, &namea);
        CKERR(r);
        r = txn->commit(txn, 0);                                                        CKERR(r);
    }
    {
        DB_TXN *txn;
        r = env->txn_begin(env, NULL, &txn, 0);                                         CKERR(r);
        r = dba->close(dbb, 0);                                                         CKERR(r);
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
    r = env->open(env, ENVDIR, envflags + DB_RECOVER, S_IRWXU+S_IRWXG+S_IRWXO);         CKERR(r);

    // verify the data
    {
        DB *db;
        r = db_create(&db, env, 0);                                                         CKERR(r);
        r = db->open(db, NULL, nameb, NULL, DB_UNKNOWN, DB_AUTO_COMMIT, 0666);              CKERR2(r, ENOENT);
        r = db->close(db, 0);                                                               CKERR(r);
    }
    {
        DB *db;
        r = db_create(&db, env, 0);                                                         CKERR(r);
        r = db->open(db, NULL, namea, NULL, DB_UNKNOWN, DB_AUTO_COMMIT, 0666);              CKERR(r);
        
        DB_TXN *txn;
        r = env->txn_begin(env, NULL, &txn, 0);                                             CKERR(r);
        DBC *cursor;
        r = db->cursor(db, txn, &cursor, 0);                                                CKERR(r);
        DBT k, v;
        r = cursor->c_get(cursor, dbt_init_malloc(&k), dbt_init_malloc(&v), DB_FIRST);
        CKERR(r);
        assert(k.size == 2);
        assert(v.size == 2);
        assert(memcmp(k.data, "a", 2) == 0);
        assert(memcmp(v.data, "b", 2) == 0);

        r = cursor->c_close(cursor);                                                        CKERR(r);

        r = txn->commit(txn, 0); CKERR(r);
        r = db->close(db, 0); CKERR(r);
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
	    do_test=TRUE;
        } else if (strcmp(argv[0], "--recover") == 0) {
            do_recover=TRUE;
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
