/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include "brt.h"
#include "key.h"
#include "pma.h"
#include "brt-internal.h"
#include "memory.h"
#include "toku_assert.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "test.h"

static const char fname[]= __FILE__ ".brt";

static TOKUTXN const null_txn = 0;
static DB * const null_db = 0;

static int test_cursor_debug = 0;

static int test_brt_cursor_keycompare(DB *db __attribute__((unused)), const DBT *a, const DBT *b) {
    return toku_keycompare(a->data, a->size, b->data, b->size);
}

static void assert_cursor_notfound(BRT brt, int position) {
    BRT_CURSOR cursor;
    int r;
    DBT kbt, vbt;

    r = toku_brt_cursor(brt, &cursor);
    assert(r==0);

    toku_init_dbt(&kbt); kbt.flags = DB_DBT_MALLOC;
    toku_init_dbt(&vbt); vbt.flags = DB_DBT_MALLOC;
    r = toku_brt_cursor_get(cursor, &kbt, &vbt, position, null_txn);
    assert(r == DB_NOTFOUND);

    r = toku_brt_cursor_close(cursor);
    assert(r==0);
}

static void assert_cursor_value(BRT brt, int position, long long value) {
    BRT_CURSOR cursor;
    int r;
    DBT kbt, vbt;
    long long v;

    r = toku_brt_cursor(brt, &cursor);
    assert(r==0);

    if (test_cursor_debug && verbose) printf("key: ");
    toku_init_dbt(&kbt); kbt.flags = DB_DBT_MALLOC;
    toku_init_dbt(&vbt); vbt.flags = DB_DBT_MALLOC;
    r = toku_brt_cursor_get(cursor, &kbt, &vbt, position, null_txn);
    assert(r == 0);
    if (test_cursor_debug && verbose) printf("%s ", (char*)kbt.data);
    assert(vbt.size == sizeof v);
    memcpy(&v, vbt.data, vbt.size);
    assert(v == value);
    toku_free(kbt.data);
    toku_free(vbt.data);
    if (test_cursor_debug && verbose) printf("\n");

    r = toku_brt_cursor_close(cursor);
    assert(r==0);
}

static void assert_cursor_first_last(BRT brt, long long firstv, long long lastv) {
    BRT_CURSOR cursor;
    int r;
    DBT kbt, vbt;
    long long v;

    r = toku_brt_cursor(brt, &cursor);
    assert(r==0);

    if (test_cursor_debug && verbose) printf("first key: ");
    toku_init_dbt(&kbt); kbt.flags = DB_DBT_MALLOC;
    toku_init_dbt(&vbt); vbt.flags = DB_DBT_MALLOC;
    r = toku_brt_cursor_get(cursor, &kbt, &vbt, DB_FIRST, null_txn);
    assert(r == 0);
    if (test_cursor_debug && verbose) printf("%s ", (char*)kbt.data);
    assert(vbt.size == sizeof v);
    memcpy(&v, vbt.data, vbt.size);
    assert(v == firstv);
    toku_free(kbt.data);
    toku_free(vbt.data);
    if (test_cursor_debug && verbose) printf("\n");

    if (test_cursor_debug && verbose) printf("last key:");
    toku_init_dbt(&kbt); kbt.flags = DB_DBT_MALLOC;
    toku_init_dbt(&vbt); vbt.flags = DB_DBT_MALLOC;
    r = toku_brt_cursor_get(cursor, &kbt, &vbt, DB_LAST, null_txn);
    assert(r == 0);
    if (test_cursor_debug)printf("%s ", (char*)kbt.data);
    assert(vbt.size == sizeof v);
    memcpy(&v, vbt.data, vbt.size);
    assert(v == lastv);
    toku_free(kbt.data);
    toku_free(vbt.data);
    if (test_cursor_debug) printf("\n");

    r = toku_brt_cursor_close(cursor);
    assert(r==0);
}

static void test_brt_cursor_first(int n, DB *db) {
    CACHETABLE ct;
    BRT brt;
    int r;
    int i;

    if (verbose) printf("test_brt_cursor_first:%d %p\n", n, db);

    unlink(fname);

    r = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER);
    assert(r==0);

    r = toku_open_brt(fname, 0, 1, &brt, 1<<12, ct, null_txn, test_brt_cursor_keycompare, db);
    assert(r==0);

    /* insert a bunch of kv pairs */
    for (i=0; i<n; i++) {
        char key[8]; long long v;
        DBT kbt, vbt;

        snprintf(key, sizeof key, "%4.4d", i);
        toku_fill_dbt(&kbt, key, strlen(key)+1);
        v = i;
        toku_fill_dbt(&vbt, &v, sizeof v);
        r = toku_brt_insert(brt, &kbt, &vbt, 0);
        assert(r==0);
    }

    if (n == 0)
        assert_cursor_notfound(brt, DB_FIRST);
    else
        assert_cursor_value(brt, DB_FIRST, 0);

    r = toku_close_brt(brt);
    assert(r==0);

    r = toku_cachetable_close(&ct);
    assert(r==0);
}

static void test_brt_cursor_last(int n, DB *db) {
    CACHETABLE ct;
    BRT brt;
    int r;
    int i;

    if (verbose) printf("test_brt_cursor_last:%d %p\n", n, db);

    unlink(fname);

    r = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER);
    assert(r==0);

    r = toku_open_brt(fname, 0, 1, &brt, 1<<12, ct, null_txn, test_brt_cursor_keycompare, db);
    assert(r==0);

    /* insert keys 0, 1, .. (n-1) */
    for (i=0; i<n; i++) {
        char key[8]; long long v;
        DBT kbt, vbt;

        snprintf(key, sizeof key, "%4.4d", i);
        toku_fill_dbt(&kbt, key, strlen(key)+1);
        v = i;
        toku_fill_dbt(&vbt, &v, sizeof v);
        r = toku_brt_insert(brt, &kbt, &vbt, 0);
        assert(r==0);
    }

    if (n == 0)
        assert_cursor_notfound(brt, DB_LAST);
    else
        assert_cursor_value(brt, DB_LAST, n-1);

    r = toku_close_brt(brt);
    assert(r==0);

    r = toku_cachetable_close(&ct);
    assert(r==0);
}

static void test_brt_cursor_first_last(int n, DB *db) {
    CACHETABLE ct;
    BRT brt;
    int r;
    int i;

    if (verbose) printf("test_brt_cursor_first_last:%d %p\n", n, db);

    unlink(fname);

    r = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER);
    assert(r==0);

    r = toku_open_brt(fname, 0, 1, &brt, 1<<12, ct, null_txn, test_brt_cursor_keycompare, db);
    assert(r==0);

    /* insert a bunch of kv pairs */
    for (i=0; i<n; i++) {
        char key[8]; long long v;
        DBT kbt, vbt;

        snprintf(key, sizeof key, "%4.4d", i);
        toku_fill_dbt(&kbt, key, strlen(key)+1);
        v = i;
        toku_fill_dbt(&vbt, &v, sizeof v);

        r = toku_brt_insert(brt, &kbt, &vbt, 0);
        assert(r==0);
    }

    if (n == 0) {
        assert_cursor_notfound(brt, DB_FIRST);
        assert_cursor_notfound(brt, DB_LAST);
    } else
        assert_cursor_first_last(brt, 0, n-1);

    r = toku_close_brt(brt);
    assert(r==0);

    r = toku_cachetable_close(&ct);
    assert(r==0);


}

static void test_brt_cursor_rfirst(int n, DB *db) {
    CACHETABLE ct;
    BRT brt;
    int r;
    int i;

    if (verbose) printf("test_brt_cursor_rfirst:%d %p\n", n, db);

    unlink(fname);

    r = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER);
    assert(r==0);

    r = toku_open_brt(fname, 0, 1, &brt, 1<<12, ct, null_txn, test_brt_cursor_keycompare, db);
    assert(r==0);

    /* insert keys n-1, n-2, ... , 0 */
    for (i=n-1; i>=0; i--) {
        char key[8]; long long v;
        DBT kbt, vbt;


        snprintf(key, sizeof key, "%4.4d", i);
        toku_fill_dbt(&kbt, key, strlen(key)+1);
        v = i;
        toku_fill_dbt(&vbt, &v, sizeof v);
        r = toku_brt_insert(brt, &kbt, &vbt, 0);
        assert(r==0);
    }

    if (n == 0)
        assert_cursor_notfound(brt, DB_FIRST);
    else
        assert_cursor_value(brt, DB_FIRST, 0);

    r = toku_close_brt(brt);
    assert(r==0);

    r = toku_cachetable_close(&ct);
    assert(r==0);
}

static void assert_cursor_walk(BRT brt, int n) {
    BRT_CURSOR cursor;
    int i;
    int r;

    r = toku_brt_cursor(brt, &cursor);
    assert(r==0);

    if (test_cursor_debug && verbose) printf("key: ");
    for (i=0; ; i++) {
        DBT kbt, vbt;
        long long v;

        toku_init_dbt(&kbt); kbt.flags = DB_DBT_MALLOC;
        toku_init_dbt(&vbt); vbt.flags = DB_DBT_MALLOC;
        r = toku_brt_cursor_get(cursor, &kbt, &vbt, DB_NEXT, null_txn);
        if (r != 0)
            break;
        if (test_cursor_debug && verbose) printf("%s ", (char*)kbt.data);
        assert(vbt.size == sizeof v);
        memcpy(&v, vbt.data, vbt.size);
        assert(v == i);
        toku_free(kbt.data);
        toku_free(vbt.data);
    }
    if (test_cursor_debug && verbose) printf("\n");
    assert(i == n);

    r = toku_brt_cursor_close(cursor);
    assert(r==0);
}

static void test_brt_cursor_walk(int n, DB *db) {
    CACHETABLE ct;
    BRT brt;
    int r;
    int i;

    if (verbose) printf("test_brt_cursor_walk:%d %p\n", n, db);

    unlink(fname);

    r = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER);
    assert(r==0);

    r = toku_open_brt(fname, 0, 1, &brt, 1<<12, ct, null_txn, test_brt_cursor_keycompare, db);
    assert(r==0);

    /* insert a bunch of kv pairs */
    for (i=0; i<n; i++) {
        char key[8]; long long v;
        DBT kbt, vbt;

        snprintf(key, sizeof key, "%4.4d", i);
        toku_fill_dbt(&kbt, key, strlen(key)+1);
        v = i;
        toku_fill_dbt(&vbt, &v, sizeof v);
        r = toku_brt_insert(brt, &kbt, &vbt, 0);
        assert(r==0);
    }

    /* walk the tree */
    assert_cursor_walk(brt, n);

    r = toku_close_brt(brt);
    assert(r==0);

    r = toku_cachetable_close(&ct);
    assert(r==0);

}

static void assert_cursor_rwalk(BRT brt, int n) {
    BRT_CURSOR cursor;
    int i;
    int r;

    r = toku_brt_cursor(brt, &cursor);
    assert(r==0);

    if (test_cursor_debug && verbose) printf("key: ");
    for (i=n-1; ; i--) {
        DBT kbt, vbt;
        long long v;

        toku_init_dbt(&kbt); kbt.flags = DB_DBT_MALLOC;
        toku_init_dbt(&vbt); vbt.flags = DB_DBT_MALLOC;
        r = toku_brt_cursor_get(cursor, &kbt, &vbt, DB_PREV, null_txn);
        if (r != 0)
            break;
        if (test_cursor_debug && verbose) printf("%s ", (char*)kbt.data);
        assert(vbt.size == sizeof v);
        memcpy(&v, vbt.data, vbt.size);
        assert(v == i);
        toku_free(kbt.data);
        toku_free(vbt.data);
    }
    if (test_cursor_debug && verbose) printf("\n");
    assert(i == -1);

    r = toku_brt_cursor_close(cursor);
    assert(r==0);
}

static void test_brt_cursor_rwalk(int n, DB *db) {
    CACHETABLE ct;
    BRT brt;
    int r;
    int i;

    if (verbose) printf("test_brt_cursor_rwalk:%d %p\n", n, db);

    unlink(fname);

    r = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER);
    assert(r==0);

    r = toku_open_brt(fname, 0, 1, &brt, 1<<12, ct, null_txn, test_brt_cursor_keycompare, db);
    assert(r==0);

    /* insert a bunch of kv pairs */
    for (i=0; i<n; i++) {
        int k; long long v;
        DBT kbt, vbt;

        k = htonl(i);
        toku_fill_dbt(&kbt, &k, sizeof k);
        v = i;
        toku_fill_dbt(&vbt, &v, sizeof v);
        r = toku_brt_insert(brt, &kbt, &vbt, 0);
        assert(r==0);
    }

    /* walk the tree */
    assert_cursor_rwalk(brt, n);

    r = toku_close_brt(brt);
    assert(r==0);

    r = toku_cachetable_close(&ct);
    assert(r==0);

}

static void assert_cursor_walk_inorder(BRT brt, int n) {
    BRT_CURSOR cursor;
    int i;
    int r;
    char *prevkey;

    r = toku_brt_cursor(brt, &cursor);
    assert(r==0);

    prevkey = 0;
    if (test_cursor_debug && verbose) printf("key: ");
    for (i=0; ; i++) {
        DBT kbt, vbt;
        long long v;

        toku_init_dbt(&kbt); kbt.flags = DB_DBT_MALLOC;
        toku_init_dbt(&vbt); vbt.flags = DB_DBT_MALLOC;
        r = toku_brt_cursor_get(cursor, &kbt, &vbt, DB_NEXT, null_txn);
        if (r != 0)
            break;
        if (test_cursor_debug && verbose) printf("%s ", (char*)kbt.data);
        assert(vbt.size == sizeof v);
        memcpy(&v, vbt.data, vbt.size);
        if (i != 0) {
	    assert(strcmp(prevkey, kbt.data) < 0);
	    toku_free(prevkey);
	}
        prevkey = kbt.data;
        toku_free(vbt.data);
    }
    if (prevkey) toku_free(prevkey);
    if (test_cursor_debug && verbose) printf("\n");
    assert(i == n);

    r = toku_brt_cursor_close(cursor);
    assert(r==0);
}

static void test_brt_cursor_rand(int n, DB *db) {
    CACHETABLE ct;
    BRT brt;
    int r;
    int i;

    if (verbose) printf("test_brt_cursor_rand:%d %p\n", n, db);

    unlink(fname);

    r = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER);
    assert(r==0);

    r = toku_open_brt(fname, 0, 1, &brt, 1<<12, ct, null_txn, test_brt_cursor_keycompare, db);
    assert(r==0);

    /* insert a bunch of kv pairs */
    for (i=0; i<n; i++) {
        char key[8]; long long v;
        DBT kbt, vbt;

        for (;;) {
	    v = ((long long) random() << 32) + random();
	    snprintf(key, sizeof key, "%lld", v);
	    toku_fill_dbt(&kbt, key, strlen(key)+1);
	    v = i;
	    toku_fill_dbt(&vbt, &v, sizeof v);
	    r = toku_brt_lookup(brt, &kbt, &vbt);
	    if (r == 0) {
                if (verbose) printf("dup");
                continue;
	    }
	    r = toku_brt_insert(brt, &kbt, &vbt, 0);
	    assert(r==0);
	    break;
        }
    }

    /* walk the tree */
    assert_cursor_walk_inorder(brt, n);

    r = toku_close_brt(brt);
    assert(r==0);

    r = toku_cachetable_close(&ct);
    assert(r==0);

}

static void test_brt_cursor_split(int n, DB *db) {
    CACHETABLE ct;
    BRT brt;
    BRT_CURSOR cursor;
    int r;
    int keyseqnum;
    int i;
    DBT kbt, vbt;

    if (verbose) printf("test_brt_cursor_split:%d %p\n", n, db);

    unlink(fname);

    r = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER);
    assert(r==0);

    r = toku_open_brt(fname, 0, 1, &brt, 1<<12, ct, null_txn, test_brt_cursor_keycompare, db);
    assert(r==0);

    /* insert a bunch of kv pairs */
    for (keyseqnum=0; keyseqnum < n/2; keyseqnum++) {
        char key[8]; long long v;

        snprintf(key, sizeof key, "%4.4d", keyseqnum);
        toku_fill_dbt(&kbt, key, strlen(key)+1);
        v = keyseqnum;
        toku_fill_dbt(&vbt, &v, sizeof v);
        r = toku_brt_insert(brt, &kbt, &vbt, 0);
        assert(r==0);
    }

    r = toku_brt_cursor(brt, &cursor);
    assert(r==0);

    if (test_cursor_debug && verbose) printf("key: ");
    for (i=0; i<n/2; i++) {
        toku_init_dbt(&kbt); kbt.flags = DB_DBT_MALLOC;
        toku_init_dbt(&vbt); vbt.flags = DB_DBT_MALLOC;
        r = toku_brt_cursor_get(cursor, &kbt, &vbt, DB_NEXT, null_txn);
        assert(r==0);
        if (test_cursor_debug && verbose) printf("%s ", (char*)kbt.data);
        toku_free(kbt.data);
        toku_free(vbt.data);
    }
    if (test_cursor_debug && verbose) printf("\n");

    for (; keyseqnum<n; keyseqnum++) {
        char key[8]; long long v;

        snprintf(key, sizeof key, "%4.4d", keyseqnum);
        toku_fill_dbt(&kbt, key, strlen(key)+1);
        v = keyseqnum;
        toku_fill_dbt(&vbt, &v, sizeof v);
        r = toku_brt_insert(brt, &kbt, &vbt, 0);
        assert(r==0);
    }

    if (test_cursor_debug && verbose) printf("key: ");
    for (;;) {
        toku_init_dbt(&kbt); kbt.flags = DB_DBT_MALLOC;
        toku_init_dbt(&vbt); vbt.flags = DB_DBT_MALLOC;
        r = toku_brt_cursor_get(cursor, &kbt, &vbt, DB_NEXT, null_txn);
        if (r != 0)
            break;
        if (test_cursor_debug && verbose) printf("%s ", (char*)kbt.data);
        toku_free(kbt.data);
        toku_free(vbt.data);
    }
    if (test_cursor_debug && verbose) printf("\n");

    r = toku_brt_cursor_close(cursor);
    assert(r==0);

    r = toku_close_brt(brt);
    assert(r==0);

    r = toku_cachetable_close(&ct);
    assert(r==0);
}

static void test_multiple_brt_cursors(int n, DB *db) {
    if (verbose) printf("test_multiple_brt_cursors:%d %p\n", n, db);

    int r;
    CACHETABLE ct;
    BRT brt;
    BRT_CURSOR cursors[n];

    unlink(fname);

    r = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER);
    assert(r==0);

    r = toku_open_brt(fname, 0, 1, &brt, 1<<12, ct, null_txn, test_brt_cursor_keycompare, db);
    assert(r==0);

    int i;
    for (i=0; i<n; i++) {
        r = toku_brt_cursor(brt, &cursors[i]);
        assert(r == 0);
    }

    for (i=0; i<n; i++) {
        r = toku_brt_cursor_close(cursors[i]);
        assert(r == 0);
    }

    r = toku_close_brt(brt);
    assert(r==0);

    r = toku_cachetable_close(&ct);
    assert(r==0);
}

static int log16(int n) {
    int r = 0;
    int b = 1;
    while (b < n) {
        b *= 16;
        r += 1;
    }
    return r;
}

static void test_multiple_brt_cursor_walk(int n, DB *db) {
    if (verbose) printf("test_multiple_brt_cursor_walk:%d %p\n", n, db);

    int r;
    CACHETABLE ct;
    BRT brt;
    const int cursor_gap = 1000;
    const int ncursors = n/cursor_gap;
    BRT_CURSOR cursors[ncursors];

    unlink(fname);

    int nodesize = 1<<12;
    int h = log16(n);
    int cachesize = 2 * h * ncursors * nodesize;
    r = toku_brt_create_cachetable(&ct, cachesize, ZERO_LSN, NULL_LOGGER);
    assert(r==0);

    r = toku_open_brt(fname, 0, 1, &brt, 1<<12, ct, null_txn, test_brt_cursor_keycompare, db);
    assert(r==0);

    int c;
    /* create the cursors */
    for (c=0; c<ncursors; c++) {
        r = toku_brt_cursor(brt, &cursors[c]);
        assert(r == 0);
    }

    DBT key, val;
    int k, v;

    /* insert keys 0, 1, 2, ... n-1 */
    int i;
    for (i=0; i<n; i++) {
        k = htonl(i);
        v = i;
        toku_fill_dbt(&key, &k, sizeof k);
        toku_fill_dbt(&val, &v, sizeof v);
        r = toku_brt_insert(brt, &key, &val, 0);
        assert(r == 0);

        /* point cursor i / cursor_gap to the current last key i */
        if ((i % cursor_gap) == 0) {
            c = i / cursor_gap;
            toku_init_dbt(&key); key.flags = DB_DBT_MALLOC;
            toku_init_dbt(&val); val.flags = DB_DBT_MALLOC;
            r = toku_brt_cursor_get(cursors[c], &key, &val, DB_LAST, null_txn);
            assert(r == 0);
            toku_free(key.data);
            toku_free(val.data);
        }
    }

    /* walk the cursors by cursor_gap */
    for (i=0; i<cursor_gap; i++) {
        for (c=0; c<ncursors; c++) {
            toku_init_dbt(&key); key.flags = DB_DBT_MALLOC;
            toku_init_dbt(&val); val.flags = DB_DBT_MALLOC;
            r = toku_brt_cursor_get(cursors[c], &key, &val, DB_NEXT, null_txn);
            if (r == DB_NOTFOUND) {
                /* we already consumed 1 previously */
                assert(i == cursor_gap-1);
            } else {
                assert(r == 0);
                int vv;
                assert(val.size == sizeof vv);
                memcpy(&vv, val.data, val.size);
                assert(vv == c*cursor_gap + i + 1);
                toku_free(key.data);
                toku_free(val.data);
            }
        }
    }

    for (i=0; i<ncursors; i++) {
        r = toku_brt_cursor_close(cursors[i]);
        assert(r == 0);
    }

    r = toku_close_brt(brt);
    assert(r==0);

    r = toku_cachetable_close(&ct);
    assert(r==0);
}

static void test_brt_cursor_set(int n, int cursor_op, DB *db) {
    if (verbose) printf("test_brt_cursor_set:%d %d %p\n", n, cursor_op, db);

    int r;
    CACHETABLE ct;
    BRT brt;
    BRT_CURSOR cursor;

    unlink(fname);

    r = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER);
    assert(r==0);

    r = toku_open_brt(fname, 0, 1, &brt, 1<<12, ct, null_txn, test_brt_cursor_keycompare, db);
    assert(r==0);

    int i;
    DBT key, val;
    int k, v;

    /* insert keys 0, 10, 20 .. 10*(n-1) */
    for (i=0; i<n; i++) {
        k = htonl(10*i);
        v = 10*i;
        toku_fill_dbt(&key, &k, sizeof k);
        toku_fill_dbt(&val, &v, sizeof v);
        r = toku_brt_insert(brt, &key, &val, 0);
        assert(r == 0);
    }

    r = toku_brt_cursor(brt, &cursor);
    assert(r==0);

    /* set cursor to random keys in set { 0, 10, 20, .. 10*(n-1) } */
    for (i=0; i<n; i++) {
        int vv;

        v = 10*(random() % n);
        k = htonl(v);
        toku_fill_dbt(&key, &k, sizeof k);
        toku_init_dbt(&val); val.flags = DB_DBT_MALLOC;
        r = toku_brt_cursor_get(cursor, &key, &val, cursor_op, null_txn);
        assert(r == 0);
        assert(val.size == sizeof vv);
        memcpy(&vv, val.data, val.size);
        assert(vv == v);
        toku_free(val.data);
        if (cursor_op == DB_SET) assert(key.data == &k);
    }

    /* try to set cursor to keys not in the tree, all should fail */
    for (i=0; i<10*n; i++) {
        if (i % 10 == 0)
            continue;
        k = htonl(i);
        toku_fill_dbt(&key, &k, sizeof k);
        toku_init_dbt(&val); val.flags = DB_DBT_MALLOC;
        r = toku_brt_cursor_get(cursor, &key, &val, DB_SET, null_txn);
        assert(r == DB_NOTFOUND);
        assert(key.data == &k);
    }

    r = toku_brt_cursor_close(cursor);
    assert(r==0);

    r = toku_close_brt(brt);
    assert(r==0);

    r = toku_cachetable_close(&ct);
    assert(r==0);
}

static void test_brt_cursor_set_range(int n, DB *db) {
    if (verbose) printf("test_brt_cursor_set_range:%d %p\n", n, db);

    int r;
    CACHETABLE ct;
    BRT brt;
    BRT_CURSOR cursor;

    unlink(fname);

    r = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER);
    assert(r==0);

    r = toku_open_brt(fname, 0, 1, &brt, 1<<12, ct, null_txn, test_brt_cursor_keycompare, db);
    assert(r==0);

    int i;
    DBT key, val;
    int k, v;

    /* insert keys 0, 10, 20 .. 10*(n-1) */
    int max_key = 10*(n-1);
    for (i=0; i<n; i++) {
        k = htonl(10*i);
        v = 10*i;
        toku_fill_dbt(&key, &k, sizeof k);
        toku_fill_dbt(&val, &v, sizeof v);
        r = toku_brt_insert(brt, &key, &val, 0);
        assert(r == 0);
    }

    r = toku_brt_cursor(brt, &cursor);
    assert(r==0);

    /* pick random keys v in 0 <= v < 10*n, the cursor should point
       to the smallest key in the tree that is >= v */
    for (i=0; i<n; i++) {
        int vv;

        v = random() % (10*n);
        k = htonl(v);
        toku_fill_dbt(&key, &k, sizeof k);
        toku_init_dbt(&val); val.flags = DB_DBT_MALLOC;
        r = toku_brt_cursor_get(cursor, &key, &val, DB_SET_RANGE, null_txn);
        if (v > max_key)
            /* there is no smallest key if v > the max key */
            assert(r == DB_NOTFOUND);
        else {
            assert(r == 0);
            assert(val.size == sizeof vv);
            memcpy(&vv, val.data, val.size);
            assert(vv == (((v+9)/10)*10));
            toku_free(val.data);
        }
    }

    r = toku_brt_cursor_close(cursor);
    assert(r==0);

    r = toku_close_brt(brt);
    assert(r==0);

    r = toku_cachetable_close(&ct);
    assert(r==0);
}

static void test_brt_cursor_delete(int n, DB *db) {
    if (verbose) printf("test_brt_cursor_delete:%d %p\n", n, db);

    int error;
    CACHETABLE ct;
    BRT brt;
    BRT_CURSOR cursor;

    unlink(fname);

    error = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER);
    assert(error == 0);

    error = toku_open_brt(fname, 0, 1, &brt, 1<<12, ct, null_txn, test_brt_cursor_keycompare, db);
    assert(error == 0);

    error = toku_brt_cursor(brt, &cursor);
    assert(error == 0);

    DBT key, val;
    int k, v;

    int i;
    /* insert keys 0, 1, 2, .. (n-1) */
    for (i=0; i<n; i++) {
        k = htonl(i);
        v = i;
        toku_fill_dbt(&key, &k, sizeof k);
        toku_fill_dbt(&val, &v, sizeof v);
        error = toku_brt_insert(brt, &key, &val, 0);
        assert(error == 0);
    }

    /* walk the tree and delete under the cursor */
    for (;;) {
        toku_init_dbt(&key); key.flags = DB_DBT_MALLOC;
        toku_init_dbt(&val); val.flags = DB_DBT_MALLOC;
        error = toku_brt_cursor_get(cursor, &key, &val, DB_NEXT, null_txn);
        if (error == DB_NOTFOUND)
            break;
        assert(error == 0);
        toku_free(key.data);
        toku_free(val.data);

        error = toku_brt_cursor_delete(cursor, 0, null_txn);
        assert(error == 0);
    }

    error = toku_brt_cursor_delete(cursor, 0, null_txn);
    assert(error != 0);

    error = toku_brt_cursor_close(cursor);
    assert(error == 0);

    error = toku_close_brt(brt);
    assert(error == 0);

    error = toku_cachetable_close(&ct);
    assert(error == 0);
}

static void test_brt_cursor_get_both(int n, DB *db) {
    if (verbose) printf("test_brt_cursor_get_both:%d %p\n", n, db);

    int error;
    CACHETABLE ct;
    BRT brt;
    BRT_CURSOR cursor;

    unlink(fname);

    error = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER);
    assert(error == 0);

    error = toku_open_brt(fname, 0, 1, &brt, 1<<12, ct, null_txn, test_brt_cursor_keycompare, db);
    assert(error == 0);

    error = toku_brt_cursor(brt, &cursor);
    assert(error == 0);

    DBT key, val;
    int k, v;

    /* verify get_both on an empty tree fails */
    k = htonl(n+1);
    v = n+1;
    toku_fill_dbt(&key, &k, sizeof k);
    toku_fill_dbt(&val, &v, sizeof v);
    error = toku_brt_cursor_get(cursor, &key, &val, DB_GET_BOTH, null_txn);
    assert(error == DB_NOTFOUND);

    int i;
    /* insert keys 0, 1, 2, .. (n-1) */
    for (i=0; i<n; i++) {
        k = htonl(i);
        v = i;
        toku_fill_dbt(&key, &k, sizeof k);
        toku_fill_dbt(&val, &v, sizeof v);
        error = toku_brt_insert(brt, &key, &val, 0);
        assert(error == 0);
    }

    /* verify that keys not in the tree fail */
    k = htonl(n+1);
    v = n-1;
    toku_fill_dbt(&key, &k, sizeof k);
    toku_fill_dbt(&val, &v, sizeof v);
    error = toku_brt_cursor_get(cursor, &key, &val, DB_GET_BOTH, null_txn);
    assert(error == DB_NOTFOUND);

    /* verify that key match but data mismatch fails */
    for (i=0; i<n; i++) {
        k = htonl(i);
        v = i+1;
        toku_fill_dbt(&key, &k, sizeof k);
        toku_fill_dbt(&val, &v, sizeof v);
        error = toku_brt_cursor_get(cursor, &key, &val, DB_GET_BOTH, null_txn);
        assert(error == DB_NOTFOUND);
    }

    /* verify that key and data matches succeeds */
    for (i=0; i<n; i++) {
        k = htonl(i);
        v = i;
        toku_fill_dbt(&key, &k, sizeof k);
        toku_fill_dbt(&val, &v, sizeof v);
        error = toku_brt_cursor_get(cursor, &key, &val, DB_GET_BOTH, null_txn);
        assert(error == 0);
#ifdef DB_CURRENT
        toku_init_dbt(&key); key.flags = DB_DBT_MALLOC;
        toku_init_dbt(&val); val.flags = DB_DBT_MALLOC;
        error = toku_brt_cursor_get(cursor, &key, &val, DB_CURRENT, 0);
        assert(error == 0);
        int vv;
        assert(val.size == sizeof vv);
        memcpy(&vv, val.data, val.size);
        assert(vv == i);
        toku_free(key.data);
        toku_free(val.data);
#endif
        error = toku_brt_cursor_delete(cursor, 0, null_txn);
        assert(error == 0);

        k = htonl(i);
        v = i;
        toku_fill_dbt(&key, &k, sizeof k);
        toku_fill_dbt(&val, &v, sizeof v);
        error = toku_brt_cursor_get(cursor, &key, &val, DB_GET_BOTH, null_txn);
        assert(error == DB_NOTFOUND);
    }

    error = toku_brt_cursor_delete(cursor, 0, null_txn);
    assert(error != 0);

    error = toku_brt_cursor_close(cursor);
    assert(error == 0);

    error = toku_close_brt(brt);
    assert(error == 0);

    error = toku_cachetable_close(&ct);
    assert(error == 0);
}


static int test_brt_cursor_inc = 1000;
static int test_brt_cursor_limit = 10000;

static void test_brt_cursor(DB *db) {
    int n;

    test_multiple_brt_cursors(1, db);
    test_multiple_brt_cursors(2, db);
    test_multiple_brt_cursors(3, db);

    for (n=0; n<test_brt_cursor_limit; n += test_brt_cursor_inc) {
        test_brt_cursor_first(n, db); toku_memory_check_all_free();
     }
    for (n=0; n<test_brt_cursor_limit; n += test_brt_cursor_inc) {
        test_brt_cursor_rfirst(n, db); toku_memory_check_all_free();
    }
    for (n=0; n<test_brt_cursor_limit; n += test_brt_cursor_inc) {
        test_brt_cursor_walk(n, db); toku_memory_check_all_free();
    }
    for (n=0; n<test_brt_cursor_limit; n += test_brt_cursor_inc) {
        test_brt_cursor_last(n, db); toku_memory_check_all_free();
    }
    for (n=0; n<test_brt_cursor_limit; n += test_brt_cursor_inc) {
        test_brt_cursor_first_last(n, db); toku_memory_check_all_free();
    }
    for (n=0; n<test_brt_cursor_limit; n += test_brt_cursor_inc) {
        test_brt_cursor_split(n, db); toku_memory_check_all_free();
    }
    for (n=0; n<test_brt_cursor_limit; n += test_brt_cursor_inc) {
        test_brt_cursor_rand(n, db); toku_memory_check_all_free();
    }
    for (n=0; n<test_brt_cursor_limit; n += test_brt_cursor_inc) {
        test_brt_cursor_rwalk(n, db); toku_memory_check_all_free();
    }

    test_brt_cursor_set(1000, DB_SET, db); toku_memory_check_all_free();
    test_brt_cursor_set(10000, DB_SET, db); toku_memory_check_all_free();
    test_brt_cursor_set(1000, DB_SET_RANGE, db); toku_memory_check_all_free();
    test_brt_cursor_set_range(1000, db); toku_memory_check_all_free();
    test_brt_cursor_set_range(10000, db); toku_memory_check_all_free();


    test_brt_cursor_delete(1000, db); toku_memory_check_all_free();
    test_multiple_brt_cursor_walk(10000, db); toku_memory_check_all_free();
    test_multiple_brt_cursor_walk(100000, db); toku_memory_check_all_free();
    test_brt_cursor_get_both(1000, db); toku_memory_check_all_free();
}


int main (int argc , const char *argv[]) {
    default_parse_args(argc, argv);

    DB a_db;
    DB *db = &a_db;
    test_brt_cursor(db);

    toku_malloc_cleanup();
    if (verbose) printf("test ok\n");
    return 0;
}
