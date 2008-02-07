/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

/* try a reverse compare function to verify that the database always uses the application's
   compare function */

#include <arpa/inet.h>
#include <assert.h>
#include <db_cxx.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define DIR "dir_test_reverse_compare_fun"
int verbose;

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

int reverse_compare(Db *db __attribute__((__unused__)), const Dbt *a, const Dbt*b) {
    return -keycompare(a->get_data(), a->get_size(), b->get_data(), b->get_size());
}

void expect(Dbc *cursor, int k, int v) {
    Dbt key; key.set_flags(DB_DBT_MALLOC);
    Dbt val; val.set_flags(DB_DBT_MALLOC);
    int r = cursor->get(&key, &val, DB_NEXT);
    assert(r == 0);
    assert(key.get_size() == sizeof k);
    int kk;
    memcpy(&kk, key.get_data(), key.get_size());
    assert(val.get_size() == sizeof v);
    int vv;
    memcpy(&vv, val.get_data(), val.get_size());
    if (kk != k || vv != v) printf("expect key %d got %d - %d %d\n", htonl(k), htonl(kk), htonl(v), htonl(vv));
    assert(kk == k);
    assert(vv == v);

    free(key.get_data());
    free(val.get_data());
}

void test_reverse_compare(int n, int dup_flags) {
    if (verbose) printf("test_reverse_compare:%d %d\n", n, dup_flags);

    DbEnv * const null_env = 0;
    Db *db;
    DbTxn * const null_txn = 0;
    const char * const fname = DIR "/reverse.compare.db";

    int r;
    int i;

    system("rm -rf " DIR);
    mkdir(DIR, 0777);

    /* create the dup database file */
    db = new Db(null_env, 0);
    assert(db);
    r = db->set_flags(dup_flags);
    assert(r == 0);
    r = db->set_pagesize(4096);
    assert(r == 0);
    r = db->set_bt_compare(reverse_compare);
    assert(r == 0);
    r = db->set_dup_compare(reverse_compare);
    assert(r == 0);
    r = db->open(null_txn, fname, "main", DB_BTREE, DB_CREATE, 0666);
    assert(r == 0);

    /* insert n unique keys {0, 1,  n-1} */
    for (i=0; i<n; i++) {
        int k, v;
        k = htonl(dup_flags ? n : i);
        Dbt key(&k, sizeof k);
        v = htonl(i);
        Dbt val(&v, sizeof v);
        r = db->put(null_txn, &key, &val, DB_YESOVERWRITE);
        assert(r == 0);
    }

    /* reopen the database to force nonleaf buffering */
    r = db->close(0);
    assert(r == 0);
    delete db;

    db = new Db(null_env, 0);
    assert(db);
    r = db->set_flags(dup_flags);
    assert(r == 0);
    r = db->set_pagesize(4096);
    assert(r == 0);
    r = db->set_bt_compare(reverse_compare);
    assert(r == 0);
    r = db->set_dup_compare(reverse_compare);
    assert(r == 0);
    r = db->open(null_txn, fname, "main", DB_BTREE, 0, 0666);
    assert(r == 0);

    /* insert n unique keys {n, n+1,  2*n-1} */
    for (i=n; i<2*n; i++) {
        int k, v;
        k = htonl(dup_flags ? n : i);
        Dbt key(&k, sizeof k);
        v = htonl(i);
        Dbt val(&v, sizeof v);
        r = db->put(null_txn, &key, &val, DB_YESOVERWRITE);
        assert(r == 0);
    }

    /* verify the sort order with a cursor */
    Dbc *cursor;
    r = db->cursor(null_txn, &cursor, 0);
    assert(r == 0);

    //for (i=0; i<2*n; i++) 
    for (i=2*n-1; i>=0; i--)
        expect(cursor, htonl(dup_flags ? n : i), htonl(i));

    r = cursor->close();
    assert(r == 0);

    r = db->close(0);
    assert(r == 0);
    delete db;
}

int main(int argc, const char *argv[]) {
    int i;
    for (i = 1; i <= (1<<16); i *= 2) {
        test_reverse_compare(i, 0);
        test_reverse_compare(i, DB_DUP + DB_DUPSORT);
    }
    return 0;
}
