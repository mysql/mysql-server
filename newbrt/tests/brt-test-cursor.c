/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id$"
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include "includes.h"
#include "test.h"

static const char fname[]= __FILE__ ".brt";

static TOKUTXN const null_txn = 0;
static DB * const null_db = 0;

static int test_cursor_debug = 0;

static int test_brt_cursor_keycompare(DB *db __attribute__((unused)), const DBT *a, const DBT *b) {
    return toku_keycompare(a->data, a->size, b->data, b->size);
}

static void assert_cursor_notfound(BRT brt, int position) {
    BRT_CURSOR cursor=0;
    int r;

    r = toku_brt_cursor(brt, &cursor, NULL, FALSE);
    assert(r==0);

    struct check_pair pair = {0,0,0,0,0};
    r = toku_brt_cursor_get(cursor, NULL, lookup_checkf, &pair, position);
    assert(r == DB_NOTFOUND);
    assert(pair.call_count==0);

    r = toku_brt_cursor_close(cursor);
    assert(r==0);
}

static void assert_cursor_value(BRT brt, int position, long long value) {
    BRT_CURSOR cursor=0;
    int r;

    r = toku_brt_cursor(brt, &cursor, NULL, FALSE);
    assert(r==0);

    if (test_cursor_debug && verbose) printf("key: ");
    struct check_pair pair = {len_ignore, 0, sizeof(value), &value, 0};
    r = toku_brt_cursor_get(cursor, NULL, lookup_checkf, &pair, position);
    assert(r == 0);
    assert(pair.call_count==1);

    r = toku_brt_cursor_close(cursor);
    assert(r==0);
}

static void assert_cursor_first_last(BRT brt, long long firstv, long long lastv) {
    BRT_CURSOR cursor=0;
    int r;

    r = toku_brt_cursor(brt, &cursor, NULL, FALSE);
    assert(r==0);

    if (test_cursor_debug && verbose) printf("first key: ");
    {
	struct check_pair pair = {len_ignore, 0, sizeof(firstv), &firstv, 0};
	r = toku_brt_cursor_get(cursor, NULL, lookup_checkf, &pair, DB_FIRST);
	assert(r == 0);
	assert(pair.call_count==1);
    }

    if (test_cursor_debug && verbose) printf("last key:");
    {
	struct check_pair pair = {len_ignore, 0, sizeof(lastv), &lastv, 0};
	r = toku_brt_cursor_get(cursor, NULL, lookup_checkf, &pair, DB_LAST);
	assert(r == 0);
	assert(pair.call_count==1);
    }
    if (test_cursor_debug && verbose) printf("\n");

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

    r = toku_open_brt(fname, 1, &brt, 1<<12, 1<<9, ct, null_txn, test_brt_cursor_keycompare, db);
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

    r = toku_close_brt(brt, 0);
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

    r = toku_open_brt(fname, 1, &brt, 1<<12, 1<<9, ct, null_txn, test_brt_cursor_keycompare, db);
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

    r = toku_close_brt(brt, 0);
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

    r = toku_open_brt(fname, 1, &brt, 1<<12, 1<<9, ct, null_txn, test_brt_cursor_keycompare, db);
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

    r = toku_close_brt(brt, 0);
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

    r = toku_open_brt(fname, 1, &brt, 1<<12, 1<<9, ct, null_txn, test_brt_cursor_keycompare, db);
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

    r = toku_close_brt(brt, 0);
    assert(r==0);

    r = toku_cachetable_close(&ct);
    assert(r==0);
}

static void assert_cursor_walk(BRT brt, int n) {
    BRT_CURSOR cursor=0;
    int i;
    int r;

    r = toku_brt_cursor(brt, &cursor, NULL, FALSE);
    assert(r==0);

    if (test_cursor_debug && verbose) printf("key: ");
    for (i=0; ; i++) {
        long long v = i;
	struct check_pair pair = {len_ignore, 0, sizeof(v), &v, 0};	
        r = toku_brt_cursor_get(cursor, NULL, lookup_checkf, &pair, DB_NEXT);
        if (r != 0) {
	    assert(pair.call_count==0);
            break;
	}
	assert(pair.call_count==1);
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

    r = toku_open_brt(fname, 1, &brt, 1<<12, 1<<9, ct, null_txn, test_brt_cursor_keycompare, db);
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

    r = toku_close_brt(brt, 0);
    assert(r==0);

    r = toku_cachetable_close(&ct);
    assert(r==0);

}

static void assert_cursor_rwalk(BRT brt, int n) {
    BRT_CURSOR cursor=0;
    int i;
    int r;

    r = toku_brt_cursor(brt, &cursor, NULL, FALSE);
    assert(r==0);

    if (test_cursor_debug && verbose) printf("key: ");
    for (i=n-1; ; i--) {
        long long v = i;
	struct check_pair pair = {len_ignore, 0, sizeof v, &v, 0};
        r = toku_brt_cursor_get(cursor, NULL, lookup_checkf, &pair, DB_PREV);
        if (r != 0) {
	    assert(pair.call_count==0);
            break;
	}
	assert(pair.call_count==1);
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

    r = toku_open_brt(fname, 1, &brt, 1<<12, 1<<9, ct, null_txn, test_brt_cursor_keycompare, db);
    assert(r==0);

    /* insert a bunch of kv pairs */
    for (i=0; i<n; i++) {
        int k; long long v;
        DBT kbt, vbt;

        k = toku_htonl(i);
        toku_fill_dbt(&kbt, &k, sizeof k);
        v = i;
        toku_fill_dbt(&vbt, &v, sizeof v);
        r = toku_brt_insert(brt, &kbt, &vbt, 0);
        assert(r==0);
    }

    /* walk the tree */
    assert_cursor_rwalk(brt, n);

    r = toku_close_brt(brt, 0);
    assert(r==0);

    r = toku_cachetable_close(&ct);
    assert(r==0);

}

static int
ascending_key_string_checkf (ITEMLEN keylen, bytevec key, ITEMLEN UU(vallen), bytevec UU(val), void *v)
// the keys are strings.  Verify that they keylen matches the key, that the keys are ascending.  Use (char**)v  to hold a
// malloc'd previous string.
{
    if (key!=NULL) {
	assert(keylen == 1+strlen(key));
	char **prevkeyp = v;
	char *prevkey = *prevkeyp;
	if (prevkey!=0) {
	    assert(strcmp(prevkey, key)<0);
	    toku_free(prevkey);
	}
	*prevkeyp = toku_strdup(key);
    }
    return 0;
}

// The keys are strings (null terminated)
static void assert_cursor_walk_inorder(BRT brt, int n) {
    BRT_CURSOR cursor=0;
    int i;
    int r;
    char *prevkey = 0;

    r = toku_brt_cursor(brt, &cursor, NULL, FALSE);
    assert(r==0);

    if (test_cursor_debug && verbose) printf("key: ");
    for (i=0; ; i++) {
        r = toku_brt_cursor_get(cursor, NULL, ascending_key_string_checkf, &prevkey, DB_NEXT);
        if (r != 0) {
            break;
	}
	assert(prevkey!=0);
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

    r = toku_open_brt(fname, 1, &brt, 1<<12, 1<<9, ct, null_txn, test_brt_cursor_keycompare, db);
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
	    struct check_pair pair = {kbt.size, key, len_ignore, 0, 0};
	    r = toku_brt_lookup(brt, &kbt, lookup_checkf, &pair);
	    if (r == 0) {
		assert(pair.call_count==1);
                if (verbose) printf("dup");
                continue;
	    }
	    assert(pair.call_count==0);
	    r = toku_brt_insert(brt, &kbt, &vbt, 0);
	    assert(r==0);
	    break;
        }
    }

    /* walk the tree */
    assert_cursor_walk_inorder(brt, n);

    r = toku_close_brt(brt, 0);
    assert(r==0);

    r = toku_cachetable_close(&ct);
    assert(r==0);

}

static void test_brt_cursor_split(int n, DB *db) {
    CACHETABLE ct;
    BRT brt;
    BRT_CURSOR cursor=0;
    int r;
    int keyseqnum;
    int i;

    if (verbose) printf("test_brt_cursor_split:%d %p\n", n, db);

    unlink(fname);

    r = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER);
    assert(r==0);

    r = toku_open_brt(fname, 1, &brt, 1<<12, 1<<9, ct, null_txn, test_brt_cursor_keycompare, db);
    assert(r==0);

    /* insert a bunch of kv pairs */
    for (keyseqnum=0; keyseqnum < n/2; keyseqnum++) {
	DBT kbt, vbt;
        char key[8]; long long v;

        snprintf(key, sizeof key, "%4.4d", keyseqnum);
        toku_fill_dbt(&kbt, key, strlen(key)+1);
        v = keyseqnum;
        toku_fill_dbt(&vbt, &v, sizeof v);
        r = toku_brt_insert(brt, &kbt, &vbt, 0);
        assert(r==0);
    }

    r = toku_brt_cursor(brt, &cursor, NULL, FALSE);
    assert(r==0);

    if (test_cursor_debug && verbose) printf("key: ");
    for (i=0; i<n/2; i++) {
	struct check_pair pair = {len_ignore, 0, len_ignore, 0, 0};
        r = toku_brt_cursor_get(cursor, NULL, lookup_checkf, &pair, DB_NEXT);
        assert(r==0);
	assert(pair.call_count==1);
    }
    if (test_cursor_debug && verbose) printf("\n");

    for (; keyseqnum<n; keyseqnum++) {
	DBT kbt,vbt;
        char key[8]; long long v;

        snprintf(key, sizeof key, "%4.4d", keyseqnum);
        toku_fill_dbt(&kbt, key, strlen(key)+1);
        v = keyseqnum;
        toku_fill_dbt(&vbt, &v, sizeof v);
        r = toku_brt_insert(brt, &kbt, &vbt, 0);
        assert(r==0);
    }

    if (test_cursor_debug && verbose) printf("key: ");
    // Just loop through the cursor
    for (;;) {
	struct check_pair pair = {len_ignore, 0, len_ignore, 0, 0};
        r = toku_brt_cursor_get(cursor, NULL, lookup_checkf, &pair, DB_NEXT);
        if (r != 0) {
	    assert(pair.call_count==0);
            break;
	}
	assert(pair.call_count==1);
    }
    if (test_cursor_debug && verbose) printf("\n");

    r = toku_brt_cursor_close(cursor);
    assert(r==0);

    r = toku_close_brt(brt, 0);
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

    r = toku_open_brt(fname, 1, &brt, 1<<12, 1<<9, ct, null_txn, test_brt_cursor_keycompare, db);
    assert(r==0);

    int i;
    for (i=0; i<n; i++) {
        r = toku_brt_cursor(brt, &cursors[i], NULL, FALSE);
        assert(r == 0);
    }

    for (i=0; i<n; i++) {
        r = toku_brt_cursor_close(cursors[i]);
        assert(r == 0);
    }

    r = toku_close_brt(brt, 0);
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

    r = toku_open_brt(fname, 1, &brt, 1<<12, 1<<9, ct, null_txn, test_brt_cursor_keycompare, db);
    assert(r==0);

    int c;
    /* create the cursors */
    for (c=0; c<ncursors; c++) {
        r = toku_brt_cursor(brt, &cursors[c], NULL, FALSE);
        assert(r == 0);
    }


    /* insert keys 0, 1, 2, ... n-1 */
    int i;
    for (i=0; i<n; i++) {
	{
	    int k = toku_htonl(i);
	    int v = i;
	    DBT key, val;
	    toku_fill_dbt(&key, &k, sizeof k);
	    toku_fill_dbt(&val, &v, sizeof v);

	    r = toku_brt_insert(brt, &key, &val, 0);
	    assert(r == 0);
	}

        /* point cursor i / cursor_gap to the current last key i */
        if ((i % cursor_gap) == 0) {
            c = i / cursor_gap;
	    struct check_pair pair = {len_ignore, 0, len_ignore, 0, 0};
            r = toku_brt_cursor_get(cursors[c], NULL, lookup_checkf, &pair, DB_LAST);
            assert(r == 0);
	    assert(pair.call_count==1);
        }
    }

    /* walk the cursors by cursor_gap */
    for (i=0; i<cursor_gap; i++) {
        for (c=0; c<ncursors; c++) {
	    int vv = c*cursor_gap + i + 1;
	    struct check_pair pair = {len_ignore, 0, sizeof vv, &vv, 0};
            r = toku_brt_cursor_get(cursors[c], NULL, lookup_checkf, &pair, DB_NEXT);
            if (r == DB_NOTFOUND) {
                /* we already consumed 1 previously */
		assert(pair.call_count==0);
                assert(i == cursor_gap-1);
            } else {
                assert(r == 0);
		assert(pair.call_count==1);
            }
        }
    }

    for (i=0; i<ncursors; i++) {
        r = toku_brt_cursor_close(cursors[i]);
        assert(r == 0);
    }

    r = toku_close_brt(brt, 0);
    assert(r==0);

    r = toku_cachetable_close(&ct);
    assert(r==0);
}

static void test_brt_cursor_set(int n, int cursor_op, DB *db) {
    if (verbose) printf("test_brt_cursor_set:%d %d %p\n", n, cursor_op, db);

    int r;
    CACHETABLE ct;
    BRT brt;
    BRT_CURSOR cursor=0;

    unlink(fname);

    r = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER);
    assert(r==0);

    r = toku_open_brt(fname, 1, &brt, 1<<12, 1<<9, ct, null_txn, test_brt_cursor_keycompare, db);
    assert(r==0);

    int i;

    /* insert keys 0, 10, 20 .. 10*(n-1) */
    for (i=0; i<n; i++) {
        int k = toku_htonl(10*i);
        int v = 10*i;
	DBT key,val;
        toku_fill_dbt(&key, &k, sizeof k);
        toku_fill_dbt(&val, &v, sizeof v);
        r = toku_brt_insert(brt, &key, &val, 0);
        assert(r == 0);
    }

    r = toku_brt_cursor(brt, &cursor, NULL, FALSE);
    assert(r==0);

    /* set cursor to random keys in set { 0, 10, 20, .. 10*(n-1) } */
    for (i=0; i<n; i++) {
        int vv;

        int v = 10*(random() % n);
        int k = toku_htonl(v);
	DBT key;
        toku_fill_dbt(&key, &k, sizeof k);
	struct check_pair pair = {sizeof k, 0, sizeof vv, &v, 0};
	if (cursor_op == DB_SET) pair.key = &k; // if it is a set operation, make sure that the result we get is the right one.
        r = toku_brt_cursor_get(cursor, &key, lookup_checkf, &pair, cursor_op);
        assert(r == 0);
	assert(pair.call_count==1);
    }

    /* try to set cursor to keys not in the tree, all should fail */
    for (i=0; i<10*n; i++) {
        if (i % 10 == 0)
            continue;
        int k = toku_htonl(i);
        DBT key;
	toku_fill_dbt(&key, &k, sizeof k);
	struct check_pair pair = {0, 0, 0, 0, 0};
        r = toku_brt_cursor_get(cursor, &key, lookup_checkf, &pair, DB_SET);
        CKERR2(r,DB_NOTFOUND);
	assert(pair.call_count==0);
        assert(key.data == &k); // make sure that no side effect happened on key
	assert((unsigned int)k==toku_htonl(i));
    }

    r = toku_brt_cursor_close(cursor);
    assert(r==0);

    r = toku_close_brt(brt, 0);
    assert(r==0);

    r = toku_cachetable_close(&ct);
    assert(r==0);
}

static void test_brt_cursor_set_range(int n, DB *db) {
    if (verbose) printf("test_brt_cursor_set_range:%d %p\n", n, db);

    int r;
    CACHETABLE ct;
    BRT brt;
    BRT_CURSOR cursor=0;

    unlink(fname);

    r = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER);
    assert(r==0);

    r = toku_open_brt(fname, 1, &brt, 1<<12, 1<<9, ct, null_txn, test_brt_cursor_keycompare, db);
    assert(r==0);

    int i;

    /* insert keys 0, 10, 20 .. 10*(n-1) */
    int max_key = 10*(n-1);
    for (i=0; i<n; i++) {
        int k = toku_htonl(10*i);
        int v = 10*i;
	DBT key, val;
        toku_fill_dbt(&key, &k, sizeof k);
        toku_fill_dbt(&val, &v, sizeof v);
        r = toku_brt_insert(brt, &key, &val, 0);
        assert(r == 0);
    }

    r = toku_brt_cursor(brt, &cursor, NULL, FALSE);
    assert(r==0);

    /* pick random keys v in 0 <= v < 10*n, the cursor should point
       to the smallest key in the tree that is >= v */
    for (i=0; i<n; i++) {

        int v = random() % (10*n);
        int k = toku_htonl(v);
        DBT key;
	toku_fill_dbt(&key, &k, sizeof k);
	int vv = ((v+9)/10)*10;
	struct check_pair pair = {sizeof k, 0, sizeof vv, &vv, 0};
        r = toku_brt_cursor_get(cursor, &key, lookup_checkf, &pair, DB_SET_RANGE);
        if (v > max_key) {
            /* there is no smallest key if v > the max key */
            assert(r == DB_NOTFOUND);
	    assert(pair.call_count==0);
	} else {
            assert(r == 0);
	    assert(pair.call_count==1);
        }
    }

    r = toku_brt_cursor_close(cursor);
    assert(r==0);

    r = toku_close_brt(brt, 0);
    assert(r==0);

    r = toku_cachetable_close(&ct);
    assert(r==0);
}

static void test_brt_cursor_delete(int n, DB *db) {
    if (verbose) printf("test_brt_cursor_delete:%d %p\n", n, db);

    int error;
    CACHETABLE ct;
    BRT brt;
    BRT_CURSOR cursor=0;

    unlink(fname);

    error = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER);
    assert(error == 0);

    error = toku_open_brt(fname, 1, &brt, 1<<12, 1<<9, ct, null_txn, test_brt_cursor_keycompare, db);
    assert(error == 0);

    error = toku_brt_cursor(brt, &cursor, NULL, FALSE);
    assert(error == 0);

    DBT key, val;
    int k, v;

    int i;
    /* insert keys 0, 1, 2, .. (n-1) */
    for (i=0; i<n; i++) {
        k = toku_htonl(i);
        v = i;
        toku_fill_dbt(&key, &k, sizeof k);
        toku_fill_dbt(&val, &v, sizeof v);
        error = toku_brt_insert(brt, &key, &val, 0);
        assert(error == 0);
    }

    /* walk the tree and delete under the cursor */
    for (;;) {
	struct check_pair pair = {len_ignore, 0, len_ignore, 0, 0};
        error = toku_brt_cursor_get(cursor, &key, lookup_checkf, &pair, DB_NEXT);
        if (error == DB_NOTFOUND) {
	    assert(pair.call_count==0);
            break;
	}
        assert(error == 0);
	assert(pair.call_count==1);

        error = toku_brt_cursor_delete(cursor, 0, null_txn);
        assert(error == 0);
    }

    error = toku_brt_cursor_delete(cursor, 0, null_txn);
    assert(error != 0);

    error = toku_brt_cursor_close(cursor);
    assert(error == 0);

    error = toku_close_brt(brt, 0);
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
        test_brt_cursor_first(n, db); 
     }
    for (n=0; n<test_brt_cursor_limit; n += test_brt_cursor_inc) {
        test_brt_cursor_rfirst(n, db); 
    }
    for (n=0; n<test_brt_cursor_limit; n += test_brt_cursor_inc) {
        test_brt_cursor_walk(n, db); 
    }
    for (n=0; n<test_brt_cursor_limit; n += test_brt_cursor_inc) {
        test_brt_cursor_last(n, db); 
    }
    for (n=0; n<test_brt_cursor_limit; n += test_brt_cursor_inc) {
        test_brt_cursor_first_last(n, db); 
    }
    for (n=0; n<test_brt_cursor_limit; n += test_brt_cursor_inc) {
        test_brt_cursor_split(n, db); 
    }
    for (n=0; n<test_brt_cursor_limit; n += test_brt_cursor_inc) {
        test_brt_cursor_rand(n, db); 
    }
    for (n=0; n<test_brt_cursor_limit; n += test_brt_cursor_inc) {
        test_brt_cursor_rwalk(n, db); 
    }

    test_brt_cursor_set(1000, DB_SET, db); 
    test_brt_cursor_set(10000, DB_SET, db); 
    test_brt_cursor_set(1000, DB_SET_RANGE, db); 
    test_brt_cursor_set_range(1000, db); 
    test_brt_cursor_set_range(10000, db); 


    test_brt_cursor_delete(1000, db); 
    test_multiple_brt_cursor_walk(10000, db); 
    test_multiple_brt_cursor_walk(100000, db); 
}


int
test_main (int argc , const char *argv[]) {
    default_parse_args(argc, argv);

    DB a_db;
    DB *db = &a_db;
    test_brt_cursor(db);
    if (verbose) printf("test ok\n");
    return 0;
}
