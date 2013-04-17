/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2010 Tokutek Inc.  All rights reserved."
#include "test.h"


/*
  - ydb layer test of redirection:
   - create two dictionaries, close
   - create txn
   - open dictionary A
   - redirect (using test-only wrapper in ydb)
   - verify now open to dictionary B
   - abort
   - verify now open to dictionary A
*/

/*
 for N = 0 .. n
     for X == 0 .. x
         for Y == 0 .. N+X
            for c == 0 .. 1
                create two dictionaries (iname A,B), close.
                create txn
                Open N DB handles to dictionary A
                redirect from A to B
                open X more DB handles to dictionary B
                close Y DB handles to dictionary B
                if c ==1 commit else abort
*/

#define DICT_0 "dict_0.db"
#define DICT_1 "dict_1.db"
enum {MAX_DBS = 3};
static DB_ENV *env = NULL;
static DB_TXN *txn = NULL;
static DB     *dbs[MAX_DBS];
static int num_open_dbs = 0;
static char *dname = DICT_0;
static DBT key;


static void start_txn(void);
static void commit_txn(void);
static void open_db(void);
static void close_db(void);
static void insert(int index, int64_t i);
static void
start_env(void) {
    assert(env==NULL);
    system("rm -rf " ENVDIR);
    toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);

    int r;
    r = db_env_create(&env, 0);
    CKERR(r);
    r = env->open(env, ENVDIR, DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_CREATE|DB_PRIVATE, S_IRWXU+S_IRWXG+S_IRWXO);
    CKERR(r);

    dname = DICT_0;

    dbt_init(&key, "key", strlen("key")+1);

    start_txn();
    open_db();
    insert(0, 0);
    dname = DICT_1;
    open_db();
    insert(1, 1);
    close_db();
    close_db();
    commit_txn();

    dname = DICT_0;
}

static void
end_env(void) {
    int r;
    r=env->close(env, 0);
    CKERR(r);
    env = NULL;
}

static void
start_txn(void) {
    assert(env!=NULL);
    assert(txn==NULL);
    int r;
    r=env->txn_begin(env, 0, &txn, 0);
    CKERR(r);
}

static void
abort_txn(void) {
    assert(env!=NULL);
    assert(txn!=NULL);
    int r;
    r=txn->abort(txn);
    CKERR(r);
    txn = NULL;
}

static void
commit_txn(void) {
    assert(env!=NULL);
    assert(txn!=NULL);
    int r;
    r=txn->commit(txn, 0);
    CKERR(r);
    txn = NULL;
}

static void
open_db(void) {
    assert(env!=NULL);
    assert(txn!=NULL);
    assert(num_open_dbs < MAX_DBS);
    assert(dbs[num_open_dbs] == NULL);

    int r;

    r = db_create(&dbs[num_open_dbs], env, 0);
    CKERR(r);

    DB *db = dbs[num_open_dbs];

    r=db->open(db, txn, dname, 0, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO);
    CKERR(r);

    num_open_dbs++;
}

static void
close_db(void) {
    assert(env!=NULL);
    assert(num_open_dbs > 0);
    assert(dbs[num_open_dbs-1] != NULL);

    num_open_dbs--;
    int r;
    DB *db = dbs[num_open_dbs];
    r = db->close(db, 0);
    CKERR(r);
    dbs[num_open_dbs] = NULL;
}

static void
insert(int idx, int64_t i) {
    assert(env!=NULL);
    assert(txn!=NULL);
    assert(idx>=0);
    assert(idx<num_open_dbs);

    DB *db = dbs[idx];
    DBT val;
    dbt_init(&val, &i, sizeof(i));
    int r=db->put(db, txn,
		  &key,
		  &val,
		  DB_YESOVERWRITE);
    CKERR(r);
}

//Verify that ALL dbs point to expected dictionary.
static void
verify(int64_t i) {
    assert(env!=NULL);
    assert(txn!=NULL);
    int r;
    int which;
    for (which = 0; which < num_open_dbs; which++) {
        DB *db = dbs[which];
        assert(db);
        DBT val_expected, val_observed;
        dbt_init(&val_expected, &i, sizeof(i));
        dbt_init(&val_observed, NULL, 0);
        r = db->get(db, txn, &key, &val_observed, 0);
        CKERR(r);
        r = int64_dbt_cmp(db, &val_expected, &val_observed);
        assert(r==0);
    }
}

static void
redirect_dictionary(char *new_dname, int r_expect) {
    assert(env!=NULL);
    assert(txn!=NULL);
    assert(num_open_dbs>0);
    int r;
    DB *db = dbs[0];
    assert(db!=NULL);
    r = test_db_redirect_dictionary(db, new_dname, txn);      // ydb-level wrapper gets iname of new file and redirects
    CKERR2(r, r_expect);
    if (r==0) {
        dname = new_dname;
    }
}

static void
redirect_EINVAL(void) {
    start_env();
    start_txn();
    dname = DICT_0;
    open_db();
    dname = DICT_1;
    open_db();
    redirect_dictionary(DICT_1, EINVAL);
    insert(1, 1);
    redirect_dictionary(DICT_1, EINVAL);
    close_db(); //Still open as zombie after this.
    redirect_dictionary(DICT_1, EINVAL); //Fail due to zombie
    close_db();
    commit_txn();
    end_env();
}

static void
redirect_test(uint8_t num_open_before, uint8_t num_open_after, uint8_t num_close_after, uint8_t commit) {
    int i;
    start_env();
    start_txn();

    assert(num_open_before > 0);

    for (i = 0; i < num_open_before; i++) {
        open_db();
    }
    verify(0);
    redirect_dictionary(DICT_1, 0);
    verify(1);
    for (i = 0; i < num_open_after; i++) {
        open_db();
    }
    verify(1);
    assert(num_close_after <= num_open_before + num_open_after);
    for (i = 0; i < num_close_after; i++) {
        close_db();
    }
    verify(1);
    if (commit) {
        commit_txn();
        start_txn();
        verify(1);
        commit_txn();
        {
            //Close any remaining open dbs.
            int still_open = num_open_dbs;
            assert(still_open == (num_open_before + num_open_after) - num_close_after);
            for (i = 0; i < still_open; i++) {
                close_db();
            }
        }
    }
    else {
        {
            //Close any remaining open dbs.
            int still_open = num_open_dbs;
            assert(still_open == (num_open_before + num_open_after) - num_close_after);
            for (i = 0; i < still_open; i++) {
                close_db();
            }
        }
        abort_txn();
        start_txn();
        verify(0);
        commit_txn();
    }
    end_env();
}


int
test_main (int argc, char *const argv[])
{
    parse_args(argc, argv);
    redirect_EINVAL();
    int num_open_before;  // number of dbs open before redirect
    int num_open_after;   // number of dbs opened after redirect
    int num_close_after;  // number of dbs closed after redirect
    int commit;
    for (num_open_before = 1; num_open_before <= 2; num_open_before++) {
        for (num_open_after = 0; num_open_after <= 1; num_open_after++) {
            for (num_close_after = 0; num_close_after <= num_open_before+num_open_after; num_close_after++) {
                for (commit = 0; commit <= 1; commit++) {
                    redirect_test(num_open_before, num_open_after, num_close_after, commit);
                }
            }
        }
    }
    return 0;
}
