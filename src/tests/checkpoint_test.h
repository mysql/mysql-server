/* -*- mode: C; c-basic-offset: 4 -*- */

#ident "Copyright (c) 2009 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#ifndef CHECKPOINT_TEST_H
#define CHECKPOINT_TEST_H


DB_ENV *env;

enum {MAX_NAME=128};

enum {NUM_FIXED_ROWS=1025};   // 4K + 1

typedef struct {
    DB*       db;
    u_int32_t flags;
    char      filename[MAX_NAME]; //Relative to ENVDIR/
    int       num;
} DICTIONARY_S, *DICTIONARY;


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

static inline int64_t
generate_val(int64_t key) {
    int64_t val = key + 314;
    return val;
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
	v = generate_val(k);
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


#endif
