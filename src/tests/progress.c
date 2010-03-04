/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2010 Tokutek Inc.  All rights reserved."
#include "test.h"


/*
 - ydb layer test of progress report on commit, abort.
  - test1:
    create two txns
    perform operations (inserts and deletes)
    commit or abort inner txn
    if abort, verify progress callback was called with correct args
    if commit, verify progress callback was not called
    commit or abort outer txn
    verify progress callback was called with correct args

    Note: inner loop ends with commit, so when outer loop completes,
    it should be called for all operations performed by inner loop.

   perform_ops {
      for i = 0 -> 5 {
          for j = 0 -> 1023
              if (j & 0x20) insert
              else delete
   }

   verify (n) {
       verify that callback was called n times with correct args
   }

   test1:
    for c0 = 0, 1 {
        for c1 = 0, 1 {
            begin txn0
            perform_ops (txn0)
            begin txn1
            perform ops (tnx1)
            if c1 
                abort txn1
                verify (n)
            else
                commit txn1
                verify (0)
        }
        if c0
             abort txn0
             verify (2n)
        else 
             commit txn0
             verify (2n)
    }


 - test2
  - create empty dictionary
  - begin txn
  - lock empty dictionary (full range lock)
  - abort 
  - verify that callback was called twice, first with stalled-on-checkpoint true, then with stalled-on-checkpoint false


*/


#define DICT_0 "dict_0.db"
static DB_ENV *env = NULL;
static DB_TXN *txn_parent = NULL;
static DB_TXN *txn_child  = NULL;
static DB     *db;
static char *dname = DICT_0;
static DBT key;
static DBT val;


static void start_txn(void);
static void commit_txn(int);
static void open_db(void);
static void close_db(void);
static void insert(void);
static void delete(void);
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
    dbt_init(&val, "val", strlen("val")+1);

    open_db();
    close_db();
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
    int r;
    if (!txn_parent) {
        r=env->txn_begin(env, 0, &txn_parent, 0);
    }
    else {
	assert(!txn_child);
        r=env->txn_begin(env, txn_parent, &txn_child, 0);
    }
    CKERR(r);
}

struct progress_expect {
    int      num_calls;
    uint8_t  is_commit_expected;
    uint8_t  stalled_on_checkpoint_expected;
    uint64_t min_entries_total_expected;
    uint64_t last_entries_processed;
};

static void poll(TOKU_TXN_PROGRESS progress, void *extra) {
    struct progress_expect *info = extra;
    info->num_calls++;
    assert(progress->is_commit == info->is_commit_expected);
    assert(progress->stalled_on_checkpoint == info->stalled_on_checkpoint_expected);
    assert(progress->entries_total >= info->min_entries_total_expected);
    assert(progress->entries_processed == 1024 + info->last_entries_processed);
    info->last_entries_processed = progress->entries_processed;
}

//expect_number_polls is number of times polling function should be called.
static void
abort_txn(int expect_number_polls) {
    assert(env!=NULL);
    DB_TXN *txn;
    BOOL child;
    if (txn_child) {
        txn = txn_child;
        child = TRUE;
    }
    else {
        txn = txn_parent;
        child = FALSE;
    }
    assert(txn);
    
    struct progress_expect extra = {
        .num_calls = 0,
        .is_commit_expected = 0,
        .stalled_on_checkpoint_expected = 0,
        .min_entries_total_expected = expect_number_polls * 1024,
        .last_entries_processed = 0
    };

    int r;
    r=txn->abort_with_progress(txn, poll, &extra);
    CKERR(r);
    assert(extra.num_calls == expect_number_polls);
    if (child)
        txn_child = NULL;
    else
        txn_parent = NULL;
}

static void
commit_txn(int expect_number_polls) {
    assert(env!=NULL);
    DB_TXN *txn;
    BOOL child;
    if (txn_child) {
        txn = txn_child;
        child = TRUE;
    }
    else {
        txn = txn_parent;
        child = FALSE;
    }
    assert(txn);
    if (child)
        assert(expect_number_polls == 0);
    
    struct progress_expect extra = {
        .num_calls = 0,
        .is_commit_expected = 1,
        .stalled_on_checkpoint_expected = 0,
        .min_entries_total_expected = expect_number_polls * 1024,
        .last_entries_processed = 0
    };

    int r;
    r=txn->commit_with_progress(txn, 0, poll, &extra);
    CKERR(r);
    assert(extra.num_calls == expect_number_polls);
    if (child)
        txn_child = NULL;
    else
        txn_parent = NULL;
}

static void
open_db(void) {
    assert(env!=NULL);
    assert(db == NULL);

    int r;

    r = db_create(&db, env, 0);
    CKERR(r);

    r=db->open(db, NULL, dname, 0, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO);
    CKERR(r);
}

static void
close_db(void) {
    assert(env!=NULL);
    assert(db != NULL);

    int r;
    r = db->close(db, 0);
    CKERR(r);
    db = NULL;
}

static void
insert(void) {
    assert(env!=NULL);
    assert(db!=NULL);
    DB_TXN *txn = txn_child ? txn_child : txn_parent;
    assert(txn);

    int r=db->put(db, txn,
		  &key,
		  &val,
		  DB_YESOVERWRITE);
    CKERR(r);
}

static void
delete(void) {
    assert(env!=NULL);
    assert(db!=NULL);
    DB_TXN *txn = txn_child ? txn_child : txn_parent;
    assert(txn);

    int r=db->del(db, txn,
		  &key,
		  DB_DELETE_ANY);
    CKERR(r);
}

static void
perform_ops(int n) {
    int i;
    int j;
    for (i = 0; i < n; i++) {
        for (j = 0; j < 1024; j++) {
            if (j & 0x20)
                delete();
            else
                insert();
        }
    }
}

static void
progress_test_1(int n, int commit) {
    start_env();
    open_db();
    {
        start_txn();
        {
            start_txn();
            perform_ops(n);
            abort_txn(n);
        }
        {
            start_txn();
            perform_ops(n);
            commit_txn(0);
        }
        perform_ops(n);
        if (commit)
            commit_txn(2*n);
        else
            abort_txn(2*n);
    }
    close_db();
    end_env();
}

struct progress_stall_expect {
    int  num_calls;
    BOOL has_been_stalled;
};

static void stall_poll(TOKU_TXN_PROGRESS progress, void *extra) {
    struct progress_stall_expect *info = extra;
    info->num_calls++;
    assert(info->num_calls <= 2);
    assert(progress->is_commit == FALSE);
    if (!info->has_been_stalled) {
        assert(info->num_calls==1);
        assert(progress->stalled_on_checkpoint); 
        info->has_been_stalled = TRUE;
    }
    else {
        assert(info->num_calls==2);
        assert(!progress->stalled_on_checkpoint); 
    }
}


static void
abort_txn_stall_checkpoint(void) {
    assert(env!=NULL);
    assert(txn_parent);
    assert(!txn_child);
    
    struct progress_stall_expect extra = {
        .num_calls = 0,
        .has_been_stalled = FALSE
    };

    int r;
    r=txn_parent->abort_with_progress(txn_parent, stall_poll, &extra);
    CKERR(r);
    assert(extra.num_calls == 2);
    txn_parent = NULL;
}


static void
lock(void) {
    assert(env!=NULL);
    assert(db!=NULL);
    assert(txn_parent);
    assert(!txn_child);

    int r=db->pre_acquire_table_lock(db, txn_parent);
    CKERR(r);
}

static void
progress_test_2(void) {
    start_env();
    open_db();
    start_txn();
    lock();
    abort_txn_stall_checkpoint();
    close_db();
    end_env();
}

int
test_main (int argc, char * const argv[])
{
    parse_args(argc, argv);
    int commit;
    for (commit = 0; commit <= 1; commit++) {
        progress_test_1(4, commit);
    }
    progress_test_2();
    return 0;
}
