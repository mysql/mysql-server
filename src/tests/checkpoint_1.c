/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
#ident "$Id$"
#include "test.h"
#include <db.h>
#include <sys/stat.h>

DB_ENV *env;

enum {MAX_NAME=128};

typedef struct {
    DB*       db;
    u_int32_t flags;
    char      filename[MAX_NAME]; //Relative to ENVDIR/
    int       num;
} DICTIONARY_S, *DICTIONARY;

static void
verify_identical_dbts(const DBT *dbt1, const DBT *dbt2) {
    assert(dbt1->size == dbt2->size);
    assert(memcmp(dbt1->data, dbt2->data, dbt1->size)==0);
}

static void
compare_dbs(DB *compare_db1, DB *compare_db2) {
    //This does not lock the dbs/grab table locks.
    //This means that you CANNOT CALL THIS while another thread is modifying the db.
    //You CAN call it while a txn is open however.
    DB_TXN *compare_txn;
    int r = env->txn_begin(env, NULL, &compare_txn, DB_READ_UNCOMMITTED);
        CKERR(r);
    DBC *c1;
    DBC *c2;
    r = compare_db1->cursor(compare_db1, compare_txn, &c1, 0);
        CKERR(r);
    r = compare_db2->cursor(compare_db2, compare_txn, &c2, 0);
        CKERR(r);

    DBT key1, val1;
    DBT key2, val2;

    dbt_init_realloc(&key1);
    dbt_init_realloc(&val1);
    dbt_init_realloc(&key2);
    dbt_init_realloc(&val2);

    do {
        int r1 = c1->c_get(c1, &key1, &val1, DB_NEXT);
        int r2 = c2->c_get(c2, &key2, &val2, DB_NEXT);
        assert(r1==0 || r1==DB_NOTFOUND);
        assert(r2==0 || r2==DB_NOTFOUND);
        assert(r1==r2);
        r = r1;
        if (r==0) {
            //Both found
            verify_identical_dbts(&key1, &key2);
            verify_identical_dbts(&val1, &val2);
        }
    } while (r==0);
    c1->c_close(c1);
    c2->c_close(c2);
    if (key1.data) toku_free(key1.data);
    if (val1.data) toku_free(val1.data);
    if (key2.data) toku_free(key2.data);
    if (val2.data) toku_free(val2.data);
    compare_txn->commit(compare_txn, 0);
}

static void
env_startup(void) {
    int r;
    r = system("rm -rf " ENVDIR);
        CKERR(r);
    r = toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);
        CKERR(r);
    r = db_env_create(&env, 0);
        CKERR(r);
    r = env->set_default_bt_compare(env, int64_dbt_cmp);
        CKERR(r);
    r = env->set_default_dup_compare(env, int64_dbt_cmp);
        CKERR(r);
    r = env->open(env, ENVDIR, DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_CREATE|DB_PRIVATE, S_IRWXU+S_IRWXG+S_IRWXO);
        CKERR(r);
    env->set_errfile(env, stderr);
    r = env->checkpointing_set_period(env, 0); //Disable auto-checkpointing.
        CKERR(r);
}

static void
env_shutdown(void) {
    int r;
    r = env->close(env, 0);
        CKERR(r);
}

static void
fill_name(DICTIONARY d, char *buf, int bufsize) {
    int bytes;
    bytes = snprintf(buf, bufsize, "%s_%08x", d->filename, d->num);
        assert(bytes>0);
        assert(bytes>(int)strlen(d->filename));
        assert(bytes<bufsize);
}

static void
fill_full_name(DICTIONARY d, char *buf, int bufsize) {
    int bytes;
    bytes = snprintf(buf, bufsize, "%s/%s_%08x", ENVDIR, d->filename, d->num);
        assert(bytes>0);
        assert(bytes>(int)strlen(d->filename));
        assert(bytes<bufsize);
}

static void
db_startup(DICTIONARY d, DB_TXN *open_txn) {
    int r;
    r = db_create(&d->db, env, 0);
        CKERR(r);
    DB *db = d->db;
    if (d->flags) {
        r = db->set_flags(db, d->flags);
            CKERR(r);
    }
    //Want to simulate much larger test.
    //Small nodesize means many nodes.
    db->set_pagesize(db, 1<<10);
    {
        DBT desc;
        dbt_init(&desc, "foo", sizeof("foo"));
        r = db->set_descriptor(db, 1, &desc, abort_on_upgrade);
            CKERR(r);
    }
    {
        char name[MAX_NAME*2];
        fill_name(d, name, sizeof(name));
        r = db->open(db, open_txn, name, NULL, DB_BTREE, DB_CREATE, 0666);
            CKERR(r);
    }
}

static void
db_shutdown(DICTIONARY d) {
    int r;
    r = d->db->close(d->db, 0);
        CKERR(r);
    d->db = NULL;
}

static void
null_dictionary(DICTIONARY d) {
    memset(d, 0, sizeof(*d));
}

static void
init_dictionary(DICTIONARY d, u_int32_t flags, char *name) {
    null_dictionary(d);
    d->flags = flags;
    strcpy(d->filename, name);
}


static void
db_delete(DICTIONARY d) {
    db_shutdown(d);
    int r;
    r = db_create(&d->db, env, 0);
        CKERR(r);
    DB *db = d->db;
    {
        char name[MAX_NAME*2];
        fill_name(d, name, sizeof(name));
        r = db->remove(db, name, NULL, 0);
            CKERR(r);
    }
    null_dictionary(d);
}


static void UU()
dbcpy(DICTIONARY dest, DICTIONARY src, DB_TXN *open_txn) {
    assert(dest->db == NULL);

    char source[MAX_NAME*2 + sizeof(ENVDIR "/")];
    fill_full_name(src, source, sizeof(source));

    *dest = *src;
    dest->db = NULL;
    dest->num++;

    char target[MAX_NAME*2 + sizeof(ENVDIR "/")];
    fill_full_name(dest, target, sizeof(target));

    int bytes;

    char command[sizeof("cp ") + sizeof(source)+ sizeof(" ") + sizeof(target)];
    bytes = snprintf(command, sizeof(command), "cp %s %s", source, target);
        assert(bytes<(int)sizeof(command));

    int r;
    r = system(command);
        CKERR(r);
    db_startup(dest, open_txn);
}

static void UU()
db_replace(DICTIONARY d, DB_TXN *open_txn) {
    //Replaces a dictionary with a physical copy that is reopened.
    //Filename is changed by incrementing the number.
    //This should be equivalent to 'rollback to checkpoint'.
    //The DB* disappears.
    DICTIONARY_S temp;
    null_dictionary(&temp);
    dbcpy(&temp, d, open_txn);
    db_delete(d);
    *d = temp;
}

static void
insert_random(DB *db1, DB *db2, DB_TXN *txn) {
    int64_t k = random64();
    int64_t v = random64();
    int r;
    DBT key;
    DBT val;
    dbt_init(&key, &k, sizeof(k));
    dbt_init(&val, &v, sizeof(v));

    if (db1) {
        r = db1->put(db1, txn, &key, &val, DB_YESOVERWRITE);
            CKERR(r);
    }
    if (db2) {
        r = db2->put(db2, txn, &key, &val, DB_YESOVERWRITE);
            CKERR(r);
    }
}

static void
snapshot(DICTIONARY d, int do_checkpoint) {
    if (do_checkpoint) {
        env->txn_checkpoint(env, 0, 0, 0);
    }
    else {
        db_shutdown(d);
        db_startup(d, NULL);
    }
}

// Only useful for single threaded testing, 
// but can be accessed from checkpoint_callback.
static DICTIONARY test_dictionary = NULL;

static void
checkpoint_test_1(u_int32_t flags, u_int32_t n, int snap_all) {
    env_startup();
    int runs;
    DICTIONARY_S db_control;
    init_dictionary(&db_control, flags, "control");
    DICTIONARY_S db_test;
    init_dictionary(&db_test, flags, "test");
    test_dictionary = &db_test;

    db_startup(&db_test, NULL);
    db_startup(&db_control, NULL);
    const int num_runs = 4;
    for (runs = 0; runs < num_runs; runs++) {
        u_int32_t i;
        for (i=0; i < n/2/num_runs; i++)
            insert_random(db_test.db, db_control.db, NULL);
        snapshot(&db_test, snap_all);
        for (i=0; i < n/2/num_runs; i++)
            insert_random(db_test.db, NULL, NULL);
        db_replace(&db_test, NULL);
        compare_dbs(db_test.db, db_control.db);
    }
    db_shutdown(&db_test);
    db_shutdown(&db_control);
    env_shutdown();
}

static void
runtests(u_int32_t flags, u_int32_t n, int snap_all) {
    if (verbose) {
        printf("%s(%s):%d, n=%03x, checkpoint=%01x, flags=%05x\n",
               __FILE__, __FUNCTION__, __LINE__, 
               n, snap_all, flags);
        fflush(stdout);
    }
    checkpoint_test_1(flags, n, snap_all);
}

// Purpose is to scribble over test db while checkpoint is 
// in progress.
void checkpoint_callback(void * extra) {
    DICTIONARY d = (DICTIONARY) extra;
    int i;
    char name[MAX_NAME*2];
    fill_name(d, name, sizeof(name));

    if (verbose) {
	printf("checkpoint callback inserting randomly into %s\n",
	       name);
	fflush(stdout);
    }
    for (i=0; i < 1024; i++)
	insert_random(d->db, NULL, NULL);
    
}

int
test_main (int argc, const char *argv[]) {
    parse_args(argc, argv);
    u_int32_t n;
    int snap;

    n = 0;
    for (snap = 0; snap < 2; snap++) {
        runtests(0, n, snap);
        runtests(DB_DUP|DB_DUPSORT, n, snap);
    }
    for (n = 1; n <= 1<<9; n*= 2) {
        for (snap = 0; snap < 2; snap++) {
            runtests(0, n, snap);
            runtests(DB_DUP|DB_DUPSORT, n, snap);
        }
    }

    db_env_set_checkpoint_callback(checkpoint_callback, (void*) test_dictionary);
    runtests(0,4,1);

    return 0;
}

#if 0
 checkpoint_1:
   create two dbs, db_test (observed) and db_control (expected)
   loop n times:
     modify both dbs
     checkpoint db_test
     modify db_test only
     copy db_test file (system(cp)) to db_temp
     compare db_temp with db_control
     continue test using db_temp as db_test instead of db_test
     delete old db_test
 checkpoint_2,3 were subsumed into 1.


TODO: Add callback to toku_checkpoint(), called after ydb lock is released.
      (Note, checkpoint_safe_lock is still held.)

 checkpoint_4:
   Callback can do some inserts, guaranteeing that we are testing that inserts
   are done "during" a checkpoint.

 checkpoint_5:
    Callback does unrelated open, close, and related close.

 checkpoint_6: 
    Callback triggers a thread that will perform same operations as:
     * checkpoint_4
     * checkpoint_5
     * delete (relevant db)
     * delete (irrelevant db)
     * take checkpoint safe lock using YDB api, do something like insert and release it
    but those operations happen during execution of toku_cachetable_end_checkpoint().
    
 checkpoint_7
   take atomic operation lock
   perform some inserts
   on another thread, call toku_checkpoint()
   sleep a few seconds
   perform more inserts
   release atomic operations lock
   wait for checkpoint thread to complete
   verify that checkpointed db has all data inserted


#endif
