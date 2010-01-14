// this test makes sure the LSN filtering is used during recovery of put_multiple

#include <sys/stat.h>
#include <fcntl.h>
#include "test.h"

const int envflags = DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE;

char *namea="a.db";
char *nameb="b.db";
enum {num_dbs = 2};

BOOL do_test=FALSE, do_recover=FALSE;

static int
put_multiple_generate(DBT *row, uint32_t num_dbs_in, DB **UU(dbs_in), DBT *keys, DBT *vals, void *extra) {
    assert(num_dbs_in > 0);
    if (do_recover)
        assert(extra==NULL);
    else
        assert(extra==&namea); //Verifying extra gets set right.
    assert(row->size >= 4);
    int32_t keysize = *(int32_t*)row->data;
    assert((int)row->size >= 4+keysize);
    int32_t valsize = row->size - 4 - keysize;
    void *key = ((uint8_t*)row->data)+4;
    void *val = ((uint8_t*)row->data)+4 + keysize;
    uint32_t which;
    for (which = 0; which < num_dbs_in; which++) {
        keys[which].size = keysize;
        keys[which].data = key;
        vals[which].size = valsize;
        vals[which].data = val;
    }
    return 0;
}

static int
put_multiple_clean(DBT *UU(row), uint32_t UU(num_dbs_in), DB **UU(dbs_in), DBT *UU(keys), DBT *UU(vals), void *extra) {
    if (do_recover)
        assert(extra==NULL);
    else
        assert(extra==&namea); //Verifying extra gets set right.
    return 0;
}

static void run_test (void) {
    int r;

    system("rm -rf " ENVDIR);
    toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);

    DB_ENV *env;
    r = db_env_create(&env, 0);                                                         CKERR(r);
    r = env->set_multiple_callbacks(env,
                                    put_multiple_generate, put_multiple_clean,
                                    NULL, NULL);
    CKERR(r);
    r = env->open(env, ENVDIR, envflags, S_IRWXU+S_IRWXG+S_IRWXO);                      CKERR(r);

    // create a txn that never closes, forcing recovery to run from the beginning of the log
    {
        DB_TXN *oldest_living_txn;
        r = env->txn_begin(env, NULL, &oldest_living_txn, 0);                                         CKERR(r);
    }

    DB *dba;
    DB *dbb;
    r = db_create(&dba, env, 0);                                                        CKERR(r);
    r = db_create(&dbb, env, 0);                                                        CKERR(r);
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
        uint8_t row[4+k.size+v.size];
        *(uint32_t*)&row[0] = k.size;
        memcpy(row+4,        k.data, k.size);
        memcpy(row+4+k.size, v.data, v.size);
        DBT rowdbt = {.data = row, .size = sizeof(row)};

        r = env->put_multiple(env, txn, &rowdbt, num_dbs, dbs, flags, &namea);          CKERR(r);
        r = txn->abort(txn);                                                            CKERR(r);
    }
    r = dbb->close(dbb, 0);                                                             CKERR(r);
    r = db_create(&dbb, env, 0);                                                        CKERR(r);
    r = dbb->open(dbb, NULL, nameb, NULL, DB_BTREE, DB_AUTO_COMMIT|DB_CREATE, 0666);    CKERR(r);
    dbs[1] = dbb;

    // txn_begin; insert <a,b>;
    {
        DB_TXN *txn;
        r = env->txn_begin(env, NULL, &txn, 0);                                         CKERR(r);
	DBT k={.data="a", .size=2};
	DBT v={.data="b", .size=2};
        uint8_t row[4+k.size+v.size];
        *(uint32_t*)&row[0] = k.size;
        memcpy(row+4,        k.data, k.size);
        memcpy(row+4+k.size, v.data, v.size);
        DBT rowdbt = {.data = row, .size = sizeof(row)};

        r = env->put_multiple(env, txn, &rowdbt, num_dbs, dbs, flags, &namea);          CKERR(r);
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
    r = env->set_multiple_callbacks(env,
                                    put_multiple_generate, put_multiple_clean,
                                    NULL, NULL);
    CKERR(r);
    r = env->open(env, ENVDIR, envflags + DB_RECOVER, S_IRWXU+S_IRWXG+S_IRWXO);         CKERR(r);

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

static void test_parse_args (int argc, char *argv[]) {
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

int test_main (int argc, char *argv[]) {
    test_parse_args(argc, argv);
    if (do_test) {
	run_test();
    } else if (do_recover) {
        run_recover();
    }
    return 0;
}
