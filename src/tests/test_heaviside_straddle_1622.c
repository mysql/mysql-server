/* -*- mode: C; c-basic-offset: 4 -*- */
#include <toku_portability.h>
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include <memory.h>
#include <toku_portability.h>
#include <db.h>

#include <errno.h>
#include <sys/stat.h>

#include "test.h"

#define INT_BLOWUP 16

struct heavi_extra {
    DBT key;
    DBT val;
    DB* db;
};

static int
int_ignore_dbt_cmp(DB *db, const DBT *a, const DBT *b) {
    assert(db && a && b);
    assert(a->size == INT_BLOWUP * sizeof(int));
    assert(b->size == INT_BLOWUP * sizeof(int));

    int x = *(int *) a->data;
    int y = *(int *) b->data;

    if (x<y) return -1;
    if (x>y) return 1;
    return 0;
}

static int
heavi_find (const DBT *key, const DBT *val, void *extra) {
    //Assumes cmp is int_dbt_cmp
    struct heavi_extra *info = extra;
    int cmp = int_ignore_dbt_cmp(info->db, key, &info->key);
    if (cmp!=0) return cmp;
    if (val) cmp = int_ignore_dbt_cmp(info->db, val, &info->val);
    return cmp;
}

// ENVDIR is defined in the Makefile

static DB *db;
static DB_TXN* txns[(int)256];
static DB_ENV* dbenv;
static DBC*    cursors[(int)256];

static void
put(char txn, int _key, int _val) {
    assert(txns[(int)txn]);
    static int waste_key[INT_BLOWUP];
    static int waste_val[INT_BLOWUP];
    waste_key[0] = _key;
    waste_val[0] = _val;
    

    int r;
    DBT key;
    DBT val;
    
    r = db->put(db, txns[(int)txn],
                    dbt_init(&key, &waste_key[0], sizeof(waste_key)),
                    dbt_init(&val, &waste_val[0], sizeof(waste_val)),
                    DB_YESOVERWRITE);

    CKERR(r);
}

static void
init_txn (char name) {
    int r;
    assert(!txns[(int)name]);
    r = dbenv->txn_begin(dbenv, NULL, &txns[(int)name], DB_TXN_NOWAIT);
        CKERR(r);
    assert(txns[(int)name]);
}

static void
init_dbc (char name) {
    int r;

    assert(!cursors[(int)name] && txns[(int)name]);
    r = db->cursor(db, txns[(int)name], &cursors[(int)name], 0);
        CKERR(r);
    assert(cursors[(int)name]);
}

static void
commit_txn (char name) {
    int r;
    assert(txns[(int)name] && !cursors[(int)name]);

    r = txns[(int)name]->commit(txns[(int)name], 0);
        CKERR(r);
    txns[(int)name] = NULL;
}

static void
close_dbc (char name) {
    int r;

    assert(cursors[(int)name]);
    r = cursors[(int)name]->c_close(cursors[(int)name]);
        CKERR(r);
    cursors[(int)name] = NULL;
}

static void
setup_dbs (u_int32_t dup_flags) {
    int r;

    r = system("rm -rf " ENVDIR);
    CKERR(r);
    toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);
    dbenv   = NULL;
    db      = NULL;
    /* Open/create primary */
    r = db_env_create(&dbenv, 0);
        CKERR(r);
    r = dbenv->set_cachesize(dbenv, 0, 4096, 1);
    u_int32_t env_txn_flags  = DB_INIT_TXN | DB_INIT_LOCK;
    u_int32_t env_open_flags = DB_CREATE | DB_PRIVATE | DB_INIT_MPOOL;
	r = dbenv->open(dbenv, ENVDIR, env_open_flags | env_txn_flags, 0600);
        CKERR(r);
    
    r = db_create(&db, dbenv, 0);
        CKERR(r);
    if (dup_flags) {
        r = db->set_flags(db, dup_flags);
            CKERR(r);
    }
    r = db->set_bt_compare( db, int_ignore_dbt_cmp);
    CKERR(r);
    r = db->set_dup_compare(db, int_ignore_dbt_cmp);
    CKERR(r);
    r = db->set_pagesize(db, 4096);
    char a;
    for (a = 'a'; a <= 'z'; a++) init_txn(a);
    init_txn('\0');
    r = db->open(db, txns[(int)'\0'], "foobar.db", NULL, DB_BTREE, DB_CREATE, 0600);
        CKERR(r);
    commit_txn('\0');
    for (a = 'a'; a <= 'z'; a++) init_dbc(a);
}

static void
close_dbs(void) {
    char a;
    for (a = 'a'; a <= 'z'; a++) {
        if (cursors[(int)a]) close_dbc(a);
        if (txns[(int)a])    commit_txn(a);
    }

    int r;
    r = db->close(db, 0);
        CKERR(r);
    db      = NULL;
    r = dbenv->close(dbenv, 0);
        CKERR(r);
    dbenv   = NULL;
}


struct dbt_pair {
    DBT key;
    DBT val;
};

struct int_pair {
    int key;
    int val;
};

int got_r_h;

static __attribute__((__unused__))
int
f_heavi (DBT const *key, DBT const *val, void *extra_f, int r_h) {
    struct int_pair *info = extra_f;

    if (r_h==0) got_r_h = 0;
    assert(key->size == INT_BLOWUP * sizeof(int));
    assert(val->size == INT_BLOWUP * sizeof(int));
    
    info->key = *(int*)key->data;
    info->val = *(int*)val->data;
    int r = 0;
    return r;
}

static __attribute__((__unused__))
void
ignore (void *ignore __attribute__((__unused__))) {
}
#define TOKU_IGNORE(x) ignore((void*)x)

static void
cget_heavi (char txn, int _key, int _val, 
	    int _key_expect, int _val_expect, int direction,
	    int r_h_expect) {
    assert(txns[(int)txn] && cursors[(int)txn]);

    static int waste_key[INT_BLOWUP];
    static int waste_val[INT_BLOWUP];
    waste_key[0] = _key;
    waste_val[0] = _val;
    int r;
    struct heavi_extra input;
    struct int_pair output;
    dbt_init(&input.key, &waste_key[0], sizeof(waste_key));
    dbt_init(&input.val, &waste_val[0], sizeof(waste_val));
    input.db = db;
    output.key = 0;
    output.val = 0;
    
    got_r_h = direction;

    r = cursors[(int)txn]->c_getf_heaviside(cursors[(int)txn], 0, //No prelocking
               f_heavi, &output,
               heavi_find, &input, direction);
    CKERR(r);
    assert(got_r_h == r_h_expect);
    assert(output.key == _key_expect);
    assert(output.val == _val_expect);
}


static void
test(u_int32_t dup_flags) {
    /* ********************************************************************** */
    int i;
    int j;
    const int max_inserts = 2*4096/(INT_BLOWUP*sizeof(int));
    for (i=1; i <= max_inserts ; i++) {
        setup_dbs(dup_flags);
        if (verbose) {
            printf("%s: put %d\n", __FILE__, i);
            fflush(stdout);
        }
        for (j=0; j < i; j++) {
            put('a', j, j);
        }
        cget_heavi('a', i-1, i-1, i-1, i-1, 1, 0);
        close_dbs();
    }
}

int
test_main(int argc, char *const argv[]) {
    parse_args(argc, argv);
    test(0);
    test(DB_DUP | DB_DUPSORT);
    return 0;
}
