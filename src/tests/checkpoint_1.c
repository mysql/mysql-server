/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
#ident "$Id$"
#include "test.h"
#include <db.h>
#include <sys/stat.h>

DB_ENV *env;

enum {MAX_NAME=128};

enum {NUM_FIXED_ROWS=1025};   // 4K + 1

typedef struct {
    DB*       db;
    u_int32_t flags;
    char      filename[MAX_NAME]; //Relative to ENVDIR/
    int       num;
} DICTIONARY_S, *DICTIONARY;

// Only useful for single threaded testing, 
// but can be accessed from checkpoint_callback.
static DICTIONARY test_dictionary = NULL;
static int iter = 0;  // horrible technique, but quicker than putting in extra (this is just test code, not product code)


// return 0 if same
static int
verify_identical_dbts(const DBT *dbt1, const DBT *dbt2) {
    int r = 0;
    if (dbt1->size != dbt2->size) r = 1;
    else if (memcmp(dbt1->data, dbt2->data, dbt1->size)!=0) r = 1;
    return r;
}

// return 0 if same
static int
compare_dbs(DB *compare_db1, DB *compare_db2) {
    //This does not lock the dbs/grab table locks.
    //This means that you CANNOT CALL THIS while another thread is modifying the db.
    //You CAN call it while a txn is open however.
    int rval = 0;
    DB_TXN *compare_txn;
    int r, r1, r2;
    r = env->txn_begin(env, NULL, &compare_txn, DB_READ_UNCOMMITTED);
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
        r1 = c1->c_get(c1, &key1, &val1, DB_NEXT);
        r2 = c2->c_get(c2, &key2, &val2, DB_NEXT);
        assert(r1==0 || r1==DB_NOTFOUND);
        assert(r2==0 || r2==DB_NOTFOUND);
	if (r1!=r2) rval = 1;
        else if (r1==0 && r2==0) {
            //Both found
            rval = verify_identical_dbts(&key1, &key2) |
		   verify_identical_dbts(&val1, &val2);
        }
    } while (r1==0 && r2==0 && rval==0);
    c1->c_close(c1);
    c2->c_close(c2);
    if (key1.data) toku_free(key1.data);
    if (val1.data) toku_free(val1.data);
    if (key2.data) toku_free(key2.data);
    if (val2.data) toku_free(val2.data);
    compare_txn->commit(compare_txn, 0);
    return rval;
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
	assert(buf[bytes] == 0);
}

static void
fill_full_name(DICTIONARY d, char *buf, int bufsize) {
    int bytes;
    bytes = snprintf(buf, bufsize, "%s/%s_%08x", ENVDIR, d->filename, d->num);
        assert(bytes>0);
        assert(bytes>(int)strlen(d->filename));
        assert(bytes<bufsize);
	assert(buf[bytes] == 0);
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
insert_n_fixed(DB *db1, DB *db2, DB_TXN *txn, int firstkey, int n) {
    int64_t k;
    int64_t v;
    int r;
    DBT key;
    DBT val;
    int i;

    //    printf("enter %s, iter = %d\n", __FUNCTION__, iter);
    //    printf("db1 = 0x%08lx, db2 = 0x%08lx, *txn = 0x%08lx, firstkey = %d, n = %d\n",
    //	   (unsigned long) db1, (unsigned long) db2, (unsigned long) txn, firstkey, n);
	

    fflush(stdout);

    for (i = 0; i<n; i++) {
	k = firstkey + i;
	v = k + 271828;          // arbitrary difference between key and value, naturally
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

static void
checkpoint_test_1(u_int32_t flags, u_int32_t n, int snap_all) {
    if (verbose) { 
        printf("%s(%s):%d, n=0x%03x, checkpoint=%01x, flags=0x%05x\n", 
               __FILE__, __FUNCTION__, __LINE__, 
               n, snap_all, flags); 
        fflush(stdout); 
    }
    env_startup();
    int run;
    int r;
    DICTIONARY_S db_control;
    init_dictionary(&db_control, flags, "control");
    DICTIONARY_S db_test;
    init_dictionary(&db_test, flags, "test");
    test_dictionary = &db_test;

    db_startup(&db_test, NULL);
    db_startup(&db_control, NULL);
    const int num_runs = 4;
    for (run = 0; run < num_runs; run++) {
        u_int32_t i;
        for (i=0; i < n/2/num_runs; i++)
            insert_random(db_test.db, db_control.db, NULL);
        snapshot(&db_test, snap_all);
        for (i=0; i < n/2/num_runs; i++)
            insert_random(db_test.db, NULL, NULL);
        db_replace(&db_test, NULL);
        r = compare_dbs(db_test.db, db_control.db);
	assert(r==0);
    }
    db_shutdown(&db_test);
    db_shutdown(&db_control);
    env_shutdown();
}

static void
checkpoint_test_2(u_int32_t flags, u_int32_t n) {
    if (verbose) { 
        printf("%s(%s):%d, n=0x%03x, checkpoint=%01x, flags=0x%05x\n", 
               __FILE__, __FUNCTION__, __LINE__, 
               n, 1, flags); 
	printf("Verify that inserts done during checkpoint are effective\n");
        fflush(stdout); 
    }
    env_startup();
    int run;
    int r;
    DICTIONARY_S db_control;
    init_dictionary(&db_control, flags, "control");
    DICTIONARY_S db_test;
    init_dictionary(&db_test, flags, "test");
    test_dictionary = &db_test;

    db_startup(&db_test, NULL);
    db_startup(&db_control, NULL);
    const int num_runs = 4;
    for (run = 0; run < num_runs; run++) {
	iter = run;
        u_int32_t i;
        for (i=0; i < n/2/num_runs; i++)
            insert_random(db_test.db, db_control.db, NULL);
	r = compare_dbs(db_test.db, db_control.db);
	assert(r==0);
        snapshot(&db_test, TRUE);  // take checkpoint, insert into test db during checkpoint callback
	r = compare_dbs(db_test.db, db_control.db);
	// test and control should be different 
	assert(r!=0);
	// now insert same rows into control and they should be same 
	insert_n_fixed(db_control.db, NULL, NULL, iter * NUM_FIXED_ROWS, NUM_FIXED_ROWS);
	r = compare_dbs(db_test.db, db_control.db);
	assert(r==0);
    }
    // now close db_test via checkpoint callback (i.e. during checkpoint)
    iter = -1;  
    snapshot(&db_test, TRUE);
    db_shutdown(&db_control);
    env_shutdown();
}


 

// Purpose is to scribble over test db while checkpoint is 
// in progress.
void checkpoint_callback_1(void * extra) {
    DICTIONARY d = *(DICTIONARY*) extra;
    int i;
    char name[MAX_NAME*2];
    fill_name(d, name, sizeof(name));

    if (verbose) {
	printf("checkpoint_callback_1 inserting randomly into %s\n",
	       name);
	fflush(stdout);
    }
    for (i=0; i < 1024; i++)
	insert_random(d->db, NULL, NULL);
    
}

void checkpoint_callback_2(void * extra) {
    DICTIONARY d = *(DICTIONARY*) extra;
    assert(d==test_dictionary);
    char name[MAX_NAME*2];
    fill_name(d, name, sizeof(name));

    if (iter >= 0) {
	if (verbose) {
	    printf("checkpoint_callback_2 inserting fixed rows into %s\n",
		   name);
	    fflush(stdout);
	}
	insert_n_fixed(d->db, NULL, NULL, iter * NUM_FIXED_ROWS, NUM_FIXED_ROWS);
    }
    else {
	if (verbose) {
	    printf("checkpoint_callback_2 closing %s\n",
		   name);
	    fflush(stdout);
	}
	db_shutdown(d);
    }
}


int
test_main (int argc, const char *argv[]) {
    parse_args(argc, argv);
    u_int32_t n;
    int snap;

    n = 0;
    for (snap = 0; snap < 2; snap++) {
        checkpoint_test_1(0, n, snap);
        checkpoint_test_1(DB_DUP|DB_DUPSORT, n, snap);
    }
    for (n = 1; n <= 1<<9; n*= 2) {
        for (snap = 0; snap < 2; snap++) {
            checkpoint_test_1(0, n, snap);
            checkpoint_test_1(DB_DUP|DB_DUPSORT, n, snap);
        }
    }

    db_env_set_checkpoint_callback(checkpoint_callback_1, &test_dictionary);
    checkpoint_test_1(0,4096,1);
    db_env_set_checkpoint_callback(checkpoint_callback_2, &test_dictionary);
    checkpoint_test_2(0,4096);
    db_env_set_checkpoint_callback(NULL, NULL);

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
