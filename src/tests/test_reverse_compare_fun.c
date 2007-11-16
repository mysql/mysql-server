/* try a reverse compare function to verify that the database always uses the application's
   compare function */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <db.h>

DBT *dbt_init(DBT *dbt, void *data, u_int32_t size) {
    memset(dbt, 0, sizeof *dbt);
    dbt->data = data;
    dbt->size = size;
    return dbt;
}

DBT *dbt_init_malloc(DBT *dbt) {
    memset(dbt, 0, sizeof *dbt);
    dbt->flags = DB_DBT_MALLOC;
    return dbt;
}

int keycompare (const void *key1, unsigned int key1len, const void *key2, unsigned int key2len) {
    if (key1len==key2len) {
	return memcmp(key1,key2,key1len);
    } else if (key1len<key2len) {
	int r = memcmp(key1,key2,key1len);
	if (r<=0) return -1; /* If the keys are the same up to 1's length, then return -1, since key1 is shorter than key2. */
	else return 1;
    } else {
	return -keycompare(key2,key2len,key1,key1len);
    }
}

int reverse_compare(DB *db __attribute__((__unused__)), const DBT *a, const DBT*b) {
    return -keycompare(a->data, a->size, b->data, b->size);
}

void expect(DBC *cursor, int k, int v) {
    DBT key, val;
    int r = cursor->c_get(cursor, dbt_init_malloc(&key), dbt_init_malloc(&val), DB_NEXT);
    assert(r == 0);
    assert(key.size == sizeof k);
    int kk;
    memcpy(&kk, key.data, key.size);
    assert(val.size == sizeof v);
    int vv;
    memcpy(&vv, val.data, val.size);
    if (kk != k || vv != v) printf("expect key %d got %d - %d %d\n", htonl(k), htonl(kk), htonl(v), htonl(vv));
    assert(kk == k);
    assert(vv == v);

    free(key.data);
    free(val.data);
}

void test_reverse_compare(int n) {
    printf("test_reverse_compare:%d\n", n);

    DB_ENV * const null_env = 0;
    DB *db;
    DB_TXN * const null_txn = 0;
    const char * const fname = "test.reverse.compare.brt";
    int r;
    int i;

    unlink(fname);

    /* create the dup database file */
    r = db_create(&db, null_env, 0);
    assert(r == 0);
    r = db->set_flags(db, 0);
    assert(r == 0);
    r = db->set_pagesize(db, 4096);
    assert(r == 0);
    r = db->set_bt_compare(db, reverse_compare);
    assert(r == 0);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, DB_CREATE, 0666);
    assert(r == 0);

    /* insert n unique keys {0, 1,  n-1} */
    for (i=0; i<n; i++) {
        DBT key, val;
        int k, v;
        k = htonl(i);
        dbt_init(&key, &k, sizeof k);
        v = i;
        dbt_init(&val, &v, sizeof v);
        r = db->put(db, null_txn, &key, &val, 0);
        assert(r == 0);
    }

    /* reopen the database to force nonleaf buffering */
    r = db->close(db, 0);
    assert(r == 0);
    r = db_create(&db, null_env, 0);
    assert(r == 0);
    r = db->set_flags(db, 0);
    assert(r == 0);
    r = db->set_pagesize(db, 4096);
    assert(r == 0);
    r = db->set_bt_compare(db, reverse_compare);
    assert(r == 0);
    r = db->open(db, null_txn, fname, "main", DB_BTREE, 0, 0666);
    assert(r == 0);

    /* insert n unique keys {n, n+1,  2*n-1} */
    for (i=n; i<2*n; i++) {
        DBT key, val;
        int k, v;
        k = htonl(i);
        dbt_init(&key, &k, sizeof k);
        v = i;
        dbt_init(&val, &v, sizeof v);
        r = db->put(db, null_txn, &key, &val, 0);
        assert(r == 0);
    }

    /* verify the sort order with a cursor */
    DBC *cursor;
    r = db->cursor(db, null_txn, &cursor, 0);
    assert(r == 0);

    //for (i=0; i<2*n; i++) 
    for (i=2*n-1; i>=0; i--)
        expect(cursor, htonl(i), i);

    r = cursor->c_close(cursor);
    assert(r == 0);

    r = db->close(db, 0);
    assert(r == 0);
}

int main() {
    int i;

    for (i = 1; i <= (1<<16); i *= 2) {
        test_reverse_compare(i);
    }
    return 0;
}
