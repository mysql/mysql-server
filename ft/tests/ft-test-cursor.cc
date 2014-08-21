/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
/*
COPYING CONDITIONS NOTICE:

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation, and provided that the
  following conditions are met:

      * Redistributions of source code must retain this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below).

      * Redistributions in binary form must reproduce this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below) in the documentation and/or other materials
        provided with the distribution.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301, USA.

COPYRIGHT NOTICE:

  TokuFT, Tokutek Fractal Tree Indexing Library.
  Copyright (C) 2007-2013 Tokutek, Inc.

DISCLAIMER:

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

UNIVERSITY PATENT NOTICE:

  The technology is licensed by the Massachusetts Institute of
  Technology, Rutgers State University of New Jersey, and the Research
  Foundation of State University of New York at Stony Brook under
  United States of America Serial No. 11/760379 and to the patents
  and/or patent applications resulting from it.

PATENT MARKING NOTICE:

  This software is covered by US Patent No. 8,185,551.
  This software is covered by US Patent No. 8,489,638.

PATENT RIGHTS GRANT:

  "THIS IMPLEMENTATION" means the copyrightable works distributed by
  Tokutek as part of the Fractal Tree project.

  "PATENT CLAIMS" means the claims of patents that are owned or
  licensable by Tokutek, both currently or in the future; and that in
  the absence of this license would be infringed by THIS
  IMPLEMENTATION or by using or running THIS IMPLEMENTATION.

  "PATENT CHALLENGE" shall mean a challenge to the validity,
  patentability, enforceability and/or non-infringement of any of the
  PATENT CLAIMS or otherwise opposing any of the PATENT CLAIMS.

  Tokutek hereby grants to you, for the term and geographical scope of
  the PATENT CLAIMS, a non-exclusive, no-charge, royalty-free,
  irrevocable (except as stated in this section) patent license to
  make, have made, use, offer to sell, sell, import, transfer, and
  otherwise run, modify, and propagate the contents of THIS
  IMPLEMENTATION, where such license applies only to the PATENT
  CLAIMS.  This grant does not include claims that would be infringed
  only as a consequence of further modifications of THIS
  IMPLEMENTATION.  If you or your agent or licensee institute or order
  or agree to the institution of patent litigation against any entity
  (including a cross-claim or counterclaim in a lawsuit) alleging that
  THIS IMPLEMENTATION constitutes direct or contributory patent
  infringement, or inducement of patent infringement, then any rights
  granted to you under this License shall terminate as of the date
  such litigation is filed.  If you or your agent or exclusive
  licensee institute or order or agree to the institution of a PATENT
  CHALLENGE, then Tokutek may terminate any rights granted to you
  under this License.
*/

#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."


#include "test.h"

static const char *fname = TOKU_TEST_FILENAME;

static TOKUTXN const null_txn = 0;

static int test_cursor_debug = 0;

static int test_ft_cursor_keycompare(DB *desc __attribute__((unused)), const DBT *a, const DBT *b) {
    return toku_keycompare(a->data, a->size, b->data, b->size);
}

static void assert_cursor_notfound(FT_HANDLE ft, int position) {
    FT_CURSOR cursor=0;
    int r;

    r = toku_ft_cursor(ft, &cursor, NULL, false, false);
    assert(r==0);

    struct check_pair pair = {0,0,0,0,0};
    r = toku_ft_cursor_get(cursor, NULL, lookup_checkf, &pair, position);
    assert(r == DB_NOTFOUND);
    assert(pair.call_count==0);

    toku_ft_cursor_close(cursor);
}

static void assert_cursor_value(FT_HANDLE ft, int position, long long value) {
    FT_CURSOR cursor=0;
    int r;

    r = toku_ft_cursor(ft, &cursor, NULL, false, false);
    assert(r==0);

    if (test_cursor_debug && verbose) printf("key: ");
    struct check_pair pair = {len_ignore, 0, sizeof(value), &value, 0};
    r = toku_ft_cursor_get(cursor, NULL, lookup_checkf, &pair, position);
    assert(r == 0);
    assert(pair.call_count==1);

    toku_ft_cursor_close(cursor);
}

static void assert_cursor_first_last(FT_HANDLE ft, long long firstv, long long lastv) {
    FT_CURSOR cursor=0;
    int r;

    r = toku_ft_cursor(ft, &cursor, NULL, false, false);
    assert(r==0);

    if (test_cursor_debug && verbose) printf("first key: ");
    {
	struct check_pair pair = {len_ignore, 0, sizeof(firstv), &firstv, 0};
	r = toku_ft_cursor_get(cursor, NULL, lookup_checkf, &pair, DB_FIRST);
	assert(r == 0);
	assert(pair.call_count==1);
    }

    if (test_cursor_debug && verbose) printf("last key:");
    {
	struct check_pair pair = {len_ignore, 0, sizeof(lastv), &lastv, 0};
	r = toku_ft_cursor_get(cursor, NULL, lookup_checkf, &pair, DB_LAST);
	assert(r == 0);
	assert(pair.call_count==1);
    }
    if (test_cursor_debug && verbose) printf("\n");

    toku_ft_cursor_close(cursor);
}

static void test_ft_cursor_first(int n) {
    CACHETABLE ct;
    FT_HANDLE ft;
    int r;
    int i;

    if (verbose) printf("test_ft_cursor_first:%d\n", n);

    unlink(fname);

    toku_cachetable_create(&ct, 0, ZERO_LSN, nullptr);

    r = toku_open_ft_handle(fname, 1, &ft, 1<<12, 1<<9, TOKU_DEFAULT_COMPRESSION_METHOD, ct, null_txn, test_ft_cursor_keycompare);
    assert(r==0);

    /* insert a bunch of kv pairs */
    for (i=0; i<n; i++) {
        char key[8]; long long v;
        DBT kbt, vbt;

        snprintf(key, sizeof key, "%4.4d", i);
        toku_fill_dbt(&kbt, key, strlen(key)+1);
        v = i;
        toku_fill_dbt(&vbt, &v, sizeof v);
        toku_ft_insert(ft, &kbt, &vbt, 0);
    }

    if (n == 0)
        assert_cursor_notfound(ft, DB_FIRST);
    else
        assert_cursor_value(ft, DB_FIRST, 0);

    r = toku_close_ft_handle_nolsn(ft, 0);
    assert(r==0);

    toku_cachetable_close(&ct);
}

static void test_ft_cursor_last(int n) {
    CACHETABLE ct;
    FT_HANDLE ft;
    int r;
    int i;

    if (verbose) printf("test_ft_cursor_last:%d\n", n);

    unlink(fname);

    toku_cachetable_create(&ct, 0, ZERO_LSN, nullptr);

    r = toku_open_ft_handle(fname, 1, &ft, 1<<12, 1<<9, TOKU_DEFAULT_COMPRESSION_METHOD, ct, null_txn, test_ft_cursor_keycompare);
    assert(r==0);

    /* insert keys 0, 1, .. (n-1) */
    for (i=0; i<n; i++) {
        char key[8]; long long v;
        DBT kbt, vbt;

        snprintf(key, sizeof key, "%4.4d", i);
        toku_fill_dbt(&kbt, key, strlen(key)+1);
        v = i;
        toku_fill_dbt(&vbt, &v, sizeof v);
        toku_ft_insert(ft, &kbt, &vbt, 0);
        assert(r==0);
    }

    if (n == 0)
        assert_cursor_notfound(ft, DB_LAST);
    else
        assert_cursor_value(ft, DB_LAST, n-1);

    r = toku_close_ft_handle_nolsn(ft, 0);
    assert(r==0);

    toku_cachetable_close(&ct);
}

static void test_ft_cursor_first_last(int n) {
    CACHETABLE ct;
    FT_HANDLE ft;
    int r;
    int i;

    if (verbose) printf("test_ft_cursor_first_last:%d\n", n);

    unlink(fname);

    toku_cachetable_create(&ct, 0, ZERO_LSN, nullptr);

    r = toku_open_ft_handle(fname, 1, &ft, 1<<12, 1<<9, TOKU_DEFAULT_COMPRESSION_METHOD, ct, null_txn, test_ft_cursor_keycompare);
    assert(r==0);

    /* insert a bunch of kv pairs */
    for (i=0; i<n; i++) {
        char key[8]; long long v;
        DBT kbt, vbt;

        snprintf(key, sizeof key, "%4.4d", i);
        toku_fill_dbt(&kbt, key, strlen(key)+1);
        v = i;
        toku_fill_dbt(&vbt, &v, sizeof v);

        toku_ft_insert(ft, &kbt, &vbt, 0);
    }

    if (n == 0) {
        assert_cursor_notfound(ft, DB_FIRST);
        assert_cursor_notfound(ft, DB_LAST);
    } else
        assert_cursor_first_last(ft, 0, n-1);

    r = toku_close_ft_handle_nolsn(ft, 0);
    assert(r==0);

    toku_cachetable_close(&ct);


}

static void test_ft_cursor_rfirst(int n) {
    CACHETABLE ct;
    FT_HANDLE ft;
    int r;
    int i;

    if (verbose) printf("test_ft_cursor_rfirst:%d\n", n);

    unlink(fname);

    toku_cachetable_create(&ct, 0, ZERO_LSN, nullptr);

    r = toku_open_ft_handle(fname, 1, &ft, 1<<12, 1<<9, TOKU_DEFAULT_COMPRESSION_METHOD, ct, null_txn, test_ft_cursor_keycompare);
    assert(r==0);

    /* insert keys n-1, n-2, ... , 0 */
    for (i=n-1; i>=0; i--) {
        char key[8]; long long v;
        DBT kbt, vbt;


        snprintf(key, sizeof key, "%4.4d", i);
        toku_fill_dbt(&kbt, key, strlen(key)+1);
        v = i;
        toku_fill_dbt(&vbt, &v, sizeof v);
        toku_ft_insert(ft, &kbt, &vbt, 0);
    }

    if (n == 0)
        assert_cursor_notfound(ft, DB_FIRST);
    else
        assert_cursor_value(ft, DB_FIRST, 0);

    r = toku_close_ft_handle_nolsn(ft, 0);
    assert(r==0);

    toku_cachetable_close(&ct);
}

static void assert_cursor_walk(FT_HANDLE ft, int n) {
    FT_CURSOR cursor=0;
    int i;
    int r;

    r = toku_ft_cursor(ft, &cursor, NULL, false, false);
    assert(r==0);

    if (test_cursor_debug && verbose) printf("key: ");
    for (i=0; ; i++) {
        long long v = i;
	struct check_pair pair = {len_ignore, 0, sizeof(v), &v, 0};	
        r = toku_ft_cursor_get(cursor, NULL, lookup_checkf, &pair, DB_NEXT);
        if (r != 0) {
	    assert(pair.call_count==0);
            break;
	}
	assert(pair.call_count==1);
    }
    if (test_cursor_debug && verbose) printf("\n");
    assert(i == n);

    toku_ft_cursor_close(cursor);
}

static void test_ft_cursor_walk(int n) {
    CACHETABLE ct;
    FT_HANDLE ft;
    int r;
    int i;

    if (verbose) printf("test_ft_cursor_walk:%d\n", n);

    unlink(fname);

    toku_cachetable_create(&ct, 0, ZERO_LSN, nullptr);

    r = toku_open_ft_handle(fname, 1, &ft, 1<<12, 1<<9, TOKU_DEFAULT_COMPRESSION_METHOD, ct, null_txn, test_ft_cursor_keycompare);
    assert(r==0);

    /* insert a bunch of kv pairs */
    for (i=0; i<n; i++) {
        char key[8]; long long v;
        DBT kbt, vbt;

        snprintf(key, sizeof key, "%4.4d", i);
        toku_fill_dbt(&kbt, key, strlen(key)+1);
        v = i;
        toku_fill_dbt(&vbt, &v, sizeof v);
        toku_ft_insert(ft, &kbt, &vbt, 0);
    }

    /* walk the tree */
    assert_cursor_walk(ft, n);

    r = toku_close_ft_handle_nolsn(ft, 0);
    assert(r==0);

    toku_cachetable_close(&ct);

}

static void assert_cursor_rwalk(FT_HANDLE ft, int n) {
    FT_CURSOR cursor=0;
    int i;
    int r;

    r = toku_ft_cursor(ft, &cursor, NULL, false, false);
    assert(r==0);

    if (test_cursor_debug && verbose) printf("key: ");
    for (i=n-1; ; i--) {
        long long v = i;
	struct check_pair pair = {len_ignore, 0, sizeof v, &v, 0};
        r = toku_ft_cursor_get(cursor, NULL, lookup_checkf, &pair, DB_PREV);
        if (r != 0) {
	    assert(pair.call_count==0);
            break;
	}
	assert(pair.call_count==1);
    }
    if (test_cursor_debug && verbose) printf("\n");
    assert(i == -1);

    toku_ft_cursor_close(cursor);
}

static void test_ft_cursor_rwalk(int n) {
    CACHETABLE ct;
    FT_HANDLE ft;
    int r;
    int i;

    if (verbose) printf("test_ft_cursor_rwalk:%d\n", n);

    unlink(fname);

    toku_cachetable_create(&ct, 0, ZERO_LSN, nullptr);

    r = toku_open_ft_handle(fname, 1, &ft, 1<<12, 1<<9, TOKU_DEFAULT_COMPRESSION_METHOD, ct, null_txn, test_ft_cursor_keycompare);
    assert(r==0);

    /* insert a bunch of kv pairs */
    for (i=0; i<n; i++) {
        int k; long long v;
        DBT kbt, vbt;

        k = toku_htonl(i);
        toku_fill_dbt(&kbt, &k, sizeof k);
        v = i;
        toku_fill_dbt(&vbt, &v, sizeof v);
        toku_ft_insert(ft, &kbt, &vbt, 0);
    }

    /* walk the tree */
    assert_cursor_rwalk(ft, n);

    r = toku_close_ft_handle_nolsn(ft, 0);
    assert(r==0);

    toku_cachetable_close(&ct);

}

static int
ascending_key_string_checkf (uint32_t keylen, const void *key, uint32_t UU(vallen), const void *UU(val), void *v, bool lock_only)
// the keys are strings.  Verify that they keylen matches the key, that the keys are ascending.  Use (char**)v  to hold a
// malloc'd previous string.
{
    if (lock_only) return 0;
    if (key!=NULL) {
	assert(keylen == 1+strlen((char*)key));
	char **CAST_FROM_VOIDP(prevkeyp, v);
	char *prevkey = *prevkeyp;
	if (prevkey!=0) {
	    assert(strcmp(prevkey, (char*)key)<0);
	    toku_free(prevkey);
	}
	*prevkeyp = toku_strdup((char*) key);
    }
    return 0;
}

// The keys are strings (null terminated)
static void assert_cursor_walk_inorder(FT_HANDLE ft, int n) {
    FT_CURSOR cursor=0;
    int i;
    int r;
    char *prevkey = 0;

    r = toku_ft_cursor(ft, &cursor, NULL, false, false);
    assert(r==0);

    if (test_cursor_debug && verbose) printf("key: ");
    for (i=0; ; i++) {
        r = toku_ft_cursor_get(cursor, NULL, ascending_key_string_checkf, &prevkey, DB_NEXT);
        if (r != 0) {
            break;
	}
	assert(prevkey!=0);
    }
    if (prevkey) toku_free(prevkey);
    if (test_cursor_debug && verbose) printf("\n");
    assert(i == n);

    toku_ft_cursor_close(cursor);
}

static void test_ft_cursor_rand(int n) {
    CACHETABLE ct;
    FT_HANDLE ft;
    int r;
    int i;

    if (verbose) printf("test_ft_cursor_rand:%d\n", n);

    unlink(fname);

    toku_cachetable_create(&ct, 0, ZERO_LSN, nullptr);

    r = toku_open_ft_handle(fname, 1, &ft, 1<<12, 1<<9, TOKU_DEFAULT_COMPRESSION_METHOD, ct, null_txn, test_ft_cursor_keycompare);
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
	    r = toku_ft_lookup(ft, &kbt, lookup_checkf, &pair);
	    if (r == 0) {
		assert(pair.call_count==1);
                if (verbose) printf("dup");
                continue;
	    }
	    assert(pair.call_count==0);
	    toku_ft_insert(ft, &kbt, &vbt, 0);
	    break;
        }
    }

    /* walk the tree */
    assert_cursor_walk_inorder(ft, n);

    r = toku_close_ft_handle_nolsn(ft, 0);
    assert(r==0);

    toku_cachetable_close(&ct);
}

static void test_ft_cursor_split(int n) {
    CACHETABLE ct;
    FT_HANDLE ft;
    FT_CURSOR cursor=0;
    int r;
    int keyseqnum;
    int i;

    if (verbose) printf("test_ft_cursor_split:%d\n", n);

    unlink(fname);

    toku_cachetable_create(&ct, 0, ZERO_LSN, nullptr);

    r = toku_open_ft_handle(fname, 1, &ft, 1<<12, 1<<9, TOKU_DEFAULT_COMPRESSION_METHOD, ct, null_txn, test_ft_cursor_keycompare);
    assert(r==0);

    /* insert a bunch of kv pairs */
    for (keyseqnum=0; keyseqnum < n/2; keyseqnum++) {
	DBT kbt, vbt;
        char key[8]; long long v;

        snprintf(key, sizeof key, "%4.4d", keyseqnum);
        toku_fill_dbt(&kbt, key, strlen(key)+1);
        v = keyseqnum;
        toku_fill_dbt(&vbt, &v, sizeof v);
        toku_ft_insert(ft, &kbt, &vbt, 0);
    }

    r = toku_ft_cursor(ft, &cursor, NULL, false, false);
    assert(r==0);

    if (test_cursor_debug && verbose) printf("key: ");
    for (i=0; i<n/2; i++) {
	struct check_pair pair = {len_ignore, 0, len_ignore, 0, 0};
        r = toku_ft_cursor_get(cursor, NULL, lookup_checkf, &pair, DB_NEXT);
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
        toku_ft_insert(ft, &kbt, &vbt, 0);
    }

    if (test_cursor_debug && verbose) printf("key: ");
    // Just loop through the cursor
    for (;;) {
	struct check_pair pair = {len_ignore, 0, len_ignore, 0, 0};
        r = toku_ft_cursor_get(cursor, NULL, lookup_checkf, &pair, DB_NEXT);
        if (r != 0) {
	    assert(pair.call_count==0);
            break;
	}
	assert(pair.call_count==1);
    }
    if (test_cursor_debug && verbose) printf("\n");

    toku_ft_cursor_close(cursor);

    r = toku_close_ft_handle_nolsn(ft, 0);
    assert(r==0);

    toku_cachetable_close(&ct);
}

static void test_multiple_ft_cursors(int n) {
    if (verbose) printf("test_multiple_ft_cursors:%d\n", n);

    int r;
    CACHETABLE ct;
    FT_HANDLE ft;
    FT_CURSOR cursors[n];

    unlink(fname);

    toku_cachetable_create(&ct, 0, ZERO_LSN, nullptr);

    r = toku_open_ft_handle(fname, 1, &ft, 1<<12, 1<<9, TOKU_DEFAULT_COMPRESSION_METHOD, ct, null_txn, test_ft_cursor_keycompare);
    assert(r==0);

    int i;
    for (i=0; i<n; i++) {
        r = toku_ft_cursor(ft, &cursors[i], NULL, false, false);
        assert(r == 0);
    }

    for (i=0; i<n; i++) {
        toku_ft_cursor_close(cursors[i]);
    }

    r = toku_close_ft_handle_nolsn(ft, 0);
    assert(r==0);

    toku_cachetable_close(&ct);
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

static void test_multiple_ft_cursor_walk(int n) {
    if (verbose) printf("test_multiple_ft_cursor_walk:%d\n", n);

    int r;
    CACHETABLE ct;
    FT_HANDLE ft;
    const int cursor_gap = 1000;
    const int ncursors = n/cursor_gap;
    FT_CURSOR cursors[ncursors];

    unlink(fname);

    int nodesize = 1<<12;
    int h = log16(n);
    int cachesize = 2 * h * ncursors * nodesize;
    toku_cachetable_create(&ct, cachesize, ZERO_LSN, nullptr);

    r = toku_open_ft_handle(fname, 1, &ft, 1<<12, 1<<9, TOKU_DEFAULT_COMPRESSION_METHOD, ct, null_txn, test_ft_cursor_keycompare);
    assert(r==0);

    int c;
    /* create the cursors */
    for (c=0; c<ncursors; c++) {
        r = toku_ft_cursor(ft, &cursors[c], NULL, false, false);
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

	    toku_ft_insert(ft, &key, &val, 0);
	}

        /* point cursor i / cursor_gap to the current last key i */
        if ((i % cursor_gap) == 0) {
            c = i / cursor_gap;
	    struct check_pair pair = {len_ignore, 0, len_ignore, 0, 0};
            r = toku_ft_cursor_get(cursors[c], NULL, lookup_checkf, &pair, DB_LAST);
            assert(r == 0);
	    assert(pair.call_count==1);
        }
    }

    /* walk the cursors by cursor_gap */
    for (i=0; i<cursor_gap; i++) {
        for (c=0; c<ncursors; c++) {
	    int vv = c*cursor_gap + i + 1;
	    struct check_pair pair = {len_ignore, 0, sizeof vv, &vv, 0};
            r = toku_ft_cursor_get(cursors[c], NULL, lookup_checkf, &pair, DB_NEXT);
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
        toku_ft_cursor_close(cursors[i]);
    }

    r = toku_close_ft_handle_nolsn(ft, 0);
    assert(r==0);

    toku_cachetable_close(&ct);
}

static void test_ft_cursor_set(int n, int cursor_op) {
    if (verbose) printf("test_ft_cursor_set:%d %d\n", n, cursor_op);

    int r;
    CACHETABLE ct;
    FT_HANDLE ft;
    FT_CURSOR cursor=0;

    unlink(fname);

    toku_cachetable_create(&ct, 0, ZERO_LSN, nullptr);

    r = toku_open_ft_handle(fname, 1, &ft, 1<<12, 1<<9, TOKU_DEFAULT_COMPRESSION_METHOD, ct, null_txn, test_ft_cursor_keycompare);
    assert(r==0);

    int i;

    /* insert keys 0, 10, 20 .. 10*(n-1) */
    for (i=0; i<n; i++) {
        int k = toku_htonl(10*i);
        int v = 10*i;
	DBT key,val;
        toku_fill_dbt(&key, &k, sizeof k);
        toku_fill_dbt(&val, &v, sizeof v);
        toku_ft_insert(ft, &key, &val, 0);
    }

    r = toku_ft_cursor(ft, &cursor, NULL, false, false);
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
        r = toku_ft_cursor_get(cursor, &key, lookup_checkf, &pair, cursor_op);
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
        r = toku_ft_cursor_get(cursor, &key, lookup_checkf, &pair, DB_SET);
        CKERR2(r,DB_NOTFOUND);
	assert(pair.call_count==0);
        assert(key.data == &k); // make sure that no side effect happened on key
	assert((unsigned int)k==toku_htonl(i));
    }

    toku_ft_cursor_close(cursor);

    r = toku_close_ft_handle_nolsn(ft, 0);
    assert(r==0);

    toku_cachetable_close(&ct);
}

static void test_ft_cursor_set_range(int n) {
    if (verbose) printf("test_ft_cursor_set_range:%d\n", n);

    int r;
    CACHETABLE ct;
    FT_HANDLE ft;
    FT_CURSOR cursor=0;

    unlink(fname);

    toku_cachetable_create(&ct, 0, ZERO_LSN, nullptr);

    r = toku_open_ft_handle(fname, 1, &ft, 1<<12, 1<<9, TOKU_DEFAULT_COMPRESSION_METHOD, ct, null_txn, test_ft_cursor_keycompare);
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
        toku_ft_insert(ft, &key, &val, 0);
    }

    r = toku_ft_cursor(ft, &cursor, NULL, false, false);
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
        r = toku_ft_cursor_get(cursor, &key, lookup_checkf, &pair, DB_SET_RANGE);
        if (v > max_key) {
            /* there is no smallest key if v > the max key */
            assert(r == DB_NOTFOUND);
	    assert(pair.call_count==0);
	} else {
            assert(r == 0);
	    assert(pair.call_count==1);
        }
    }

    toku_ft_cursor_close(cursor);

    r = toku_close_ft_handle_nolsn(ft, 0);
    assert(r==0);

    toku_cachetable_close(&ct);
}

static void test_ft_cursor_delete(int n) {
    if (verbose) printf("test_ft_cursor_delete:%d\n", n);

    int error;
    CACHETABLE ct;
    FT_HANDLE ft;
    FT_CURSOR cursor=0;

    unlink(fname);

    toku_cachetable_create(&ct, 0, ZERO_LSN, nullptr);

    error = toku_open_ft_handle(fname, 1, &ft, 1<<12, 1<<9, TOKU_DEFAULT_COMPRESSION_METHOD, ct, null_txn, test_ft_cursor_keycompare);
    assert(error == 0);

    error = toku_ft_cursor(ft, &cursor, NULL, false, false);
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
        toku_ft_insert(ft, &key, &val, 0);
    }

    /* walk the tree and delete under the cursor */
    for (;;) {
	struct check_pair pair = {len_ignore, 0, len_ignore, 0, 0};
        error = toku_ft_cursor_get(cursor, &key, lookup_checkf, &pair, DB_NEXT);
        if (error == DB_NOTFOUND) {
	    assert(pair.call_count==0);
            break;
	}
        assert(error == 0);
	assert(pair.call_count==1);

        error = toku_ft_cursor_delete(cursor, 0, null_txn);
        assert(error == 0);
    }

    error = toku_ft_cursor_delete(cursor, 0, null_txn);
    assert(error != 0);

    toku_ft_cursor_close(cursor);

    error = toku_close_ft_handle_nolsn(ft, 0);
    assert(error == 0);

    toku_cachetable_close(&ct);
}

static int test_ft_cursor_inc = 1000;
static int test_ft_cursor_limit = 10000;

static void test_ft_cursor(void) {
    int n;

    test_multiple_ft_cursors(1);
    test_multiple_ft_cursors(2);
    test_multiple_ft_cursors(3);

    for (n=0; n<test_ft_cursor_limit; n += test_ft_cursor_inc) {
        test_ft_cursor_first(n); 
     }
    for (n=0; n<test_ft_cursor_limit; n += test_ft_cursor_inc) {
        test_ft_cursor_rfirst(n); 
    }
    for (n=0; n<test_ft_cursor_limit; n += test_ft_cursor_inc) {
        test_ft_cursor_walk(n); 
    }
    for (n=0; n<test_ft_cursor_limit; n += test_ft_cursor_inc) {
        test_ft_cursor_last(n); 
    }
    for (n=0; n<test_ft_cursor_limit; n += test_ft_cursor_inc) {
        test_ft_cursor_first_last(n); 
    }
    for (n=0; n<test_ft_cursor_limit; n += test_ft_cursor_inc) {
        test_ft_cursor_split(n); 
    }
    for (n=0; n<test_ft_cursor_limit; n += test_ft_cursor_inc) {
        test_ft_cursor_rand(n); 
    }
    for (n=0; n<test_ft_cursor_limit; n += test_ft_cursor_inc) {
        test_ft_cursor_rwalk(n); 
    }

    test_ft_cursor_set(1000, DB_SET); 
    test_ft_cursor_set(10000, DB_SET); 
    test_ft_cursor_set(1000, DB_SET_RANGE); 
    test_ft_cursor_set_range(1000); 
    test_ft_cursor_set_range(10000); 


    test_ft_cursor_delete(1000); 
    test_multiple_ft_cursor_walk(10000); 
    test_multiple_ft_cursor_walk(100000); 
}


int
test_main (int argc , const char *argv[]) {
    default_parse_args(argc, argv);
    test_ft_cursor();
    if (verbose) printf("test ok\n");
    return 0;
}
