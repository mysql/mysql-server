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

  TokuDB, Tokutek Fractal Tree Indexing Library.
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

static TOKUTXN const null_txn = 0;
static DB * const null_db = 0;

static const char *fname = TOKU_TEST_FILENAME;

static void test_dump_empty_db (void) {
    FT_HANDLE t;
    CACHETABLE ct;
    int r;

    toku_cachetable_create(&ct, 0, ZERO_LSN, NULL_LOGGER);
    unlink(fname);
    r = toku_open_ft_handle(fname, 1, &t, 1024, 256, TOKU_DEFAULT_COMPRESSION_METHOD, ct, null_txn, toku_builtin_compare_fun);
    assert(r==0);
    if (verbose) { r=toku_dump_ft(stdout, t); assert(r==0); }
    r = toku_close_ft_handle_nolsn(t, 0);          assert(r==0);
    toku_cachetable_close(&ct);
    
}

/* Test running multiple trees in different files */
static void test_multiple_files_of_size (int size) {
    char n0[TOKU_PATH_MAX+1];
    toku_path_join(n0, 2, TOKU_TEST_FILENAME, "test0.dat");
    char n1[TOKU_PATH_MAX+1];
    toku_path_join(n1, 2, TOKU_TEST_FILENAME, "test1.dat");
    CACHETABLE ct;
    FT_HANDLE t0,t1;
    int r,i;
    if (verbose) printf("test_multiple_files_of_size(%d)\n", size);
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    r = toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU); assert(r == 0);
    
    toku_cachetable_create(&ct, 0, ZERO_LSN, NULL_LOGGER);
    r = toku_open_ft_handle(n0, 1, &t0, size, size / 4, TOKU_DEFAULT_COMPRESSION_METHOD, ct, null_txn, toku_builtin_compare_fun); assert(r==0);
    r = toku_open_ft_handle(n1, 1, &t1, size, size / 4, TOKU_DEFAULT_COMPRESSION_METHOD, ct, null_txn, toku_builtin_compare_fun); assert(r==0);
    for (i=0; i<10000; i++) {
	char key[100],val[100];
	DBT k,v;
	snprintf(key, 100, "key%d", i);
	snprintf(val, 100, "val%d", i);
	toku_ft_insert(t0, toku_fill_dbt(&k, key, 1+strlen(key)), toku_fill_dbt(&v, val, 1+strlen(val)), null_txn);
	snprintf(val, 100, "Val%d", i);
	toku_ft_insert(t1, toku_fill_dbt(&k, key, 1+strlen(key)), toku_fill_dbt(&v, val, 1+strlen(val)), null_txn);
    }
    //toku_verify_ft(t0);
    //dump_ft(t0);
    //dump_ft(t1);
    r = toku_verify_ft(t0); assert(r==0);
    r = toku_verify_ft(t1); assert(r==0);

    r = toku_close_ft_handle_nolsn(t0, 0); assert(r==0);
    r = toku_close_ft_handle_nolsn(t1, 0); assert(r==0);
    toku_cachetable_close(&ct);
    

    /* Now see if the data is all there. */
    toku_cachetable_create(&ct, 0, ZERO_LSN, NULL_LOGGER);
    r = toku_open_ft_handle(n0, 0, &t0, 1<<12, 1<<9, TOKU_DEFAULT_COMPRESSION_METHOD, ct, null_txn, toku_builtin_compare_fun);
    if (verbose) printf("%s:%d r=%d\n", __FILE__, __LINE__,r);
    assert(r==0);
    r = toku_open_ft_handle(n1, 0, &t1, 1<<12, 1<<9, TOKU_DEFAULT_COMPRESSION_METHOD, ct, null_txn, toku_builtin_compare_fun); assert(r==0);

    for (i=0; i<10000; i++) {
	char key[100],val[100];
	snprintf(key, 100, "key%d", i);
	snprintf(val, 100, "val%d", i);
	ft_lookup_and_check_nodup(t0, key, val);
	snprintf(val, 100, "Val%d", i);
	ft_lookup_and_check_nodup(t1, key, val);
    }

    r = toku_close_ft_handle_nolsn(t0, 0); assert(r==0);
    r = toku_close_ft_handle_nolsn(t1, 0); assert(r==0);
    toku_cachetable_close(&ct);
    
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
}

static void test_multiple_files (void) {
    test_multiple_files_of_size (1<<12);
    test_multiple_files_of_size (1<<20);
}

/* Test to see that a single db can be opened many times.  */
static void test_multiple_ft_handles_one_db_one_file (void) {
    enum { MANYN = 2 };
    int i, r;
    CACHETABLE ct;
    FT_HANDLE trees[MANYN];
    if (verbose) printf("test_multiple_ft_handles_one_db_one_file:");
    
    unlink(fname);
    toku_cachetable_create(&ct, 32, ZERO_LSN, NULL_LOGGER);
    for (i=0; i<MANYN; i++) {
	r = toku_open_ft_handle(fname, (i==0), &trees[i], 1<<12, 1<<9, TOKU_DEFAULT_COMPRESSION_METHOD, ct, null_txn, toku_builtin_compare_fun);
	assert(r==0);
    }
    for (i=0; i<MANYN; i++) {
	char k[20], v[20];
	DBT kb, vb;
	snprintf(k, 20, "key%d", i);
	snprintf(v, 20, "val%d", i);
	toku_ft_insert(trees[i], toku_fill_dbt(&kb, k, strlen(k)+1), toku_fill_dbt(&vb, v, strlen(v)+1), null_txn);
    }
    for (i=0; i<MANYN; i++) {
	char k[20],vexpect[20];
	snprintf(k, 20, "key%d", i);
	snprintf(vexpect, 20, "val%d", i);
	ft_lookup_and_check_nodup(trees[0], k, vexpect);
    }
    for (i=0; i<MANYN; i++) {
	r=toku_close_ft_handle_nolsn(trees[i], 0); assert(r==0);
    }
    toku_cachetable_close(&ct);
    
    if (verbose) printf(" ok\n");
}


/* Check to see if data can be read that was written. */
static void  test_read_what_was_written (void) {
    CACHETABLE ct;
    FT_HANDLE brt;
    int r;
    const int NVALS=10000;

    if (verbose) printf("test_read_what_was_written(): "); fflush(stdout);

    unlink(fname);
    

    toku_cachetable_create(&ct, 0, ZERO_LSN, NULL_LOGGER);
    r = toku_open_ft_handle(fname, 1, &brt, 1<<12, 1<<9, TOKU_DEFAULT_COMPRESSION_METHOD, ct, null_txn, toku_builtin_compare_fun);  assert(r==0);
    r = toku_close_ft_handle_nolsn(brt, 0); assert(r==0);
    toku_cachetable_close(&ct);

    

    /* Now see if we can read an empty tree in. */
    toku_cachetable_create(&ct, 0, ZERO_LSN, NULL_LOGGER);
    r = toku_open_ft_handle(fname, 0, &brt, 1<<12, 1<<9, TOKU_DEFAULT_COMPRESSION_METHOD, ct, null_txn, toku_builtin_compare_fun);  assert(r==0);

    /* See if we can put something in it. */
    {
	DBT k,v;
	toku_ft_insert(brt, toku_fill_dbt(&k, "hello", 6), toku_fill_dbt(&v, "there", 6), null_txn);
    }

    r = toku_close_ft_handle_nolsn(brt, 0); assert(r==0);
    toku_cachetable_close(&ct);

    

    /* Now see if we can read it in and get the value. */
    toku_cachetable_create(&ct, 0, ZERO_LSN, NULL_LOGGER);
    r = toku_open_ft_handle(fname, 0, &brt, 1<<12, 1<<9, TOKU_DEFAULT_COMPRESSION_METHOD, ct, null_txn, toku_builtin_compare_fun); assert(r==0);

    ft_lookup_and_check_nodup(brt, "hello", "there");

    assert(toku_verify_ft(brt)==0);

    /* Now put a bunch (NVALS) of things in. */
    {
	int i;
	for (i=0; i<NVALS; i++) {
	    char key[100],val[100];
	    DBT k,v;
	    snprintf(key, 100, "key%d", i);
	    snprintf(val, 100, "val%d", i);
	    if (i<600) {
		int verify_result=toku_verify_ft(brt);;
		assert(verify_result==0);
	    }
	    toku_ft_insert(brt, toku_fill_dbt(&k, key, strlen(key)+1), toku_fill_dbt(&v, val, strlen(val)+1), null_txn);
	    if (i<600) {
		int verify_result=toku_verify_ft(brt);
		if (verify_result) {
		    r = toku_dump_ft(stdout, brt);
		    assert(r==0);
		    assert(0);
		}
		{
		    int j;
		    for (j=0; j<=i; j++) {
			char expectedval[100];
			snprintf(key, 100, "key%d", j);
			snprintf(expectedval, 100, "val%d", j);
			ft_lookup_and_check_nodup(brt, key, expectedval);
		    }
		}
	    }
	}
    }
    if (verbose) printf("Now read them out\n");

    r = toku_verify_ft(brt);
    assert(r==0);
    //dump_ft(brt);

    /* See if we can read them all out again. */
    {
	int i;
	for (i=0; i<NVALS; i++) {
	    char key[100],expectedval[100];
	    snprintf(key, 100, "key%d", i);
	    snprintf(expectedval, 100, "val%d", i);
	    ft_lookup_and_check_nodup(brt, key, expectedval);
	}
    }

    r = toku_close_ft_handle_nolsn(brt, 0); assert(r==0);
    if (verbose) printf("%s:%d About to close %p\n", __FILE__, __LINE__, ct);
    toku_cachetable_close(&ct);

    

    toku_cachetable_create(&ct, 0, ZERO_LSN, NULL_LOGGER);
    r = toku_open_ft_handle(fname, 0, &brt, 1<<12, 1<<9, TOKU_DEFAULT_COMPRESSION_METHOD, ct, null_txn, toku_builtin_compare_fun); assert(r==0);

    ft_lookup_and_check_nodup(brt, "hello", "there");
    {
	int i;
	for (i=0; i<NVALS; i++) {
	    char key[100],expectedval[100];
	    snprintf(key, 100, "key%d", i);
	    snprintf(expectedval, 100, "val%d", i);
	    ft_lookup_and_check_nodup(brt, key, expectedval);
	}
    }

    r = toku_close_ft_handle_nolsn(brt, 0); assert(r==0);
    toku_cachetable_close(&ct);

    


    if (verbose) printf(" ok\n");
}

/* Test c_get(DB_LAST) on an empty tree */
static void test_cursor_last_empty(void) {
    CACHETABLE ct;
    FT_HANDLE brt;
    FT_CURSOR cursor=0;
    int r;
    if (verbose) printf("%s", __FUNCTION__);
    unlink(fname);
    
    //printf("%s:%d %d alloced\n", __FILE__, __LINE__, toku_get_n_items_malloced()); toku_print_malloced_items();
    toku_cachetable_create(&ct, 0, ZERO_LSN, NULL_LOGGER);
    //printf("%s:%d %d alloced\n", __FILE__, __LINE__, toku_get_n_items_malloced()); toku_print_malloced_items();
    r = toku_open_ft_handle(fname, 1, &brt, 1<<12, 1<<9, TOKU_DEFAULT_COMPRESSION_METHOD, ct, null_txn, toku_builtin_compare_fun);  assert(r==0);
    //printf("%s:%d %d alloced\n", __FILE__, __LINE__, toku_get_n_items_malloced()); toku_print_malloced_items();
    r = toku_ft_cursor(brt, &cursor, NULL, false, false);            assert(r==0);
    {
	struct check_pair pair = {0,0,0,0,0};
	r = toku_ft_cursor_get(cursor, NULL, lookup_checkf, &pair, DB_LAST);
	assert(pair.call_count==0);
	assert(r==DB_NOTFOUND);
    }
    {
	struct check_pair pair = {0,0,0,0,0};
	r = toku_ft_cursor_get(cursor, NULL, lookup_checkf, &pair, DB_FIRST);
	assert(pair.call_count==0);
	assert(r==DB_NOTFOUND);
    }
    toku_ft_cursor_close(cursor);
    r = toku_close_ft_handle_nolsn(brt, 0);
    //printf("%s:%d %d alloced\n", __FILE__, __LINE__, toku_get_n_items_malloced()); toku_print_malloced_items();
    toku_cachetable_close(&ct);
    //printf("%s:%d %d alloced\n", __FILE__, __LINE__, toku_get_n_items_malloced()); toku_print_malloced_items();
    
}

static void test_cursor_next (void) {
    CACHETABLE ct;
    FT_HANDLE brt;
    FT_CURSOR cursor=0;
    int r;
    DBT kbt, vbt;

    unlink(fname);
    
    toku_cachetable_create(&ct, 0, ZERO_LSN, NULL_LOGGER);
    //printf("%s:%d %d alloced\n", __FILE__, __LINE__, toku_get_n_items_malloced()); toku_print_malloced_items();
    r = toku_open_ft_handle(fname, 1, &brt, 1<<12, 1<<9, TOKU_DEFAULT_COMPRESSION_METHOD, ct, null_txn, toku_builtin_compare_fun);  assert(r==0);
    //printf("%s:%d %d alloced\n", __FILE__, __LINE__, toku_get_n_items_malloced()); toku_print_malloced_items();
    toku_ft_insert(brt, toku_fill_dbt(&kbt, "hello", 6), toku_fill_dbt(&vbt, "there", 6), null_txn);
    toku_ft_insert(brt, toku_fill_dbt(&kbt, "byebye", 7), toku_fill_dbt(&vbt, "byenow", 7), null_txn);
    if (verbose) printf("%s:%d calling toku_ft_cursor(...)\n", __FILE__, __LINE__);
    r = toku_ft_cursor(brt, &cursor, NULL, false, false);            assert(r==0);
    toku_init_dbt(&kbt);
    //printf("%s:%d %d alloced\n", __FILE__, __LINE__, toku_get_n_items_malloced()); toku_print_malloced_items();
    toku_init_dbt(&vbt);
    //printf("%s:%d %d alloced\n", __FILE__, __LINE__, toku_get_n_items_malloced()); toku_print_malloced_items();

    if (verbose) printf("%s:%d calling toku_ft_cursor_get(...)\n", __FILE__, __LINE__);
    {
	struct check_pair pair = {7, "byebye", 7, "byenow", 0};
	r = toku_ft_cursor_get(cursor, NULL, lookup_checkf, &pair, DB_NEXT);
	if (verbose) printf("%s:%d called toku_ft_cursor_get(...)\n", __FILE__, __LINE__);
	assert(r==0);
	assert(pair.call_count==1);
    }

    {
	struct check_pair pair = {6, "hello", 6, "there", 0};
	r = toku_ft_cursor_get(cursor, NULL, lookup_checkf, &pair, DB_NEXT);
	assert(r==0);
	assert(pair.call_count==1);
    }
    {
	struct check_pair pair = {0, 0, 0, 0, 0};
	r = toku_ft_cursor_get(cursor, NULL, lookup_checkf, &pair, DB_NEXT);
	assert(r==DB_NOTFOUND);
	assert(pair.call_count==0);
    }

    toku_ft_cursor_close(cursor);
    r = toku_close_ft_handle_nolsn(brt, 0);
    //printf("%s:%d %d alloced\n", __FILE__, __LINE__, toku_get_n_items_malloced()); toku_print_malloced_items();
    toku_cachetable_close(&ct);
    //printf("%s:%d %d alloced\n", __FILE__, __LINE__, toku_get_n_items_malloced()); toku_print_malloced_items();
    

}

static int wrong_compare_fun(DB* UU(desc), const DBT *a, const DBT *b) {
    unsigned int i;
    unsigned char *CAST_FROM_VOIDP(ad, a->data);
    unsigned char *CAST_FROM_VOIDP(bd, b->data);
    unsigned int siz=a->size;
    assert(a->size==b->size);
    //assert(db==&nonce_db); // make sure the db was passed  down correctly
    for (i=0; i<siz; i++) {
	if (ad[siz-1-i]<bd[siz-1-i]) return -1;
	if (ad[siz-1-i]>bd[siz-1-i]) return +1;
    }
    return 0;

}

static void test_wrongendian_compare (int wrong_p, unsigned int N) {
    CACHETABLE ct;
    FT_HANDLE brt;
    int r;
    unsigned int i;

    unlink(fname);
    

    {
	char a[4]={0,1,0,0};
	char b[4]={1,0,0,0};
	DBT at, bt;
	assert(wrong_compare_fun(NULL, toku_fill_dbt(&at, a, 4), toku_fill_dbt(&bt, b, 4))>0);
	assert(wrong_compare_fun(NULL, toku_fill_dbt(&at, b, 4), toku_fill_dbt(&bt, a, 4))<0);
    }

    toku_cachetable_create(&ct, 0, ZERO_LSN, NULL_LOGGER);
    //printf("%s:%d WRONG=%d\n", __FILE__, __LINE__, wrong_p);

    if (0) { // ???? Why is this commented out?
        r = toku_open_ft_handle(fname, 1, &brt, 1<<20, 1<<17, TOKU_DEFAULT_COMPRESSION_METHOD, ct, null_txn, wrong_p ? wrong_compare_fun : toku_builtin_compare_fun);  assert(r==0);
    for (i=1; i<257; i+=255) {
	unsigned char a[4],b[4];
	b[3] = a[0] = (unsigned char)(i&255);
	b[2] = a[1] = (unsigned char)((i>>8)&255);
	b[1] = a[2] = (unsigned char)((i>>16)&255);
	b[0] = a[3] = (unsigned char)((i>>24)&255);
	DBT kbt;
        toku_fill_dbt(&kbt, a, sizeof a);
	DBT vbt;
        toku_fill_dbt(&vbt, b, sizeof b);
	if (verbose)
	    printf("%s:%d insert: %02x%02x%02x%02x -> %02x%02x%02x%02x\n", __FILE__, __LINE__,
		   ((char*)kbt.data)[0], ((char*)kbt.data)[1], ((char*)kbt.data)[2], ((char*)kbt.data)[3],
		   ((char*)vbt.data)[0], ((char*)vbt.data)[1], ((char*)vbt.data)[2], ((char*)vbt.data)[3]);
	toku_ft_insert(brt, &kbt, &vbt, null_txn);
    }
    {
	FT_CURSOR cursor=0;
	r = toku_ft_cursor(brt, &cursor, NULL, false, false);            assert(r==0);

	for (i=0; i<2; i++) {
	    unsigned char a[4],b[4];
	    struct check_pair pair = {4, &a, 4, &b, 0};
	    b[3] = a[0] = (unsigned char)(i&255);
	    b[2] = a[1] = (unsigned char)((i>>8)&255);
	    b[1] = a[2] = (unsigned char)((i>>16)&255);
	    b[0] = a[3] = (unsigned char)((i>>24)&255);
	    r = toku_ft_cursor_get(cursor, NULL, lookup_checkf, &pair, DB_NEXT);
	    assert(r==0);
	    assert(pair.call_count==1);
	}


        r = toku_close_ft_handle_nolsn(brt, 0);
    }
    }

    {
	toku_cachetable_verify(ct);
	r = toku_open_ft_handle(fname, 1, &brt, 1<<20, 1<<17, TOKU_DEFAULT_COMPRESSION_METHOD, ct, null_txn, wrong_p ? wrong_compare_fun : toku_builtin_compare_fun);  assert(r==0);
	toku_cachetable_verify(ct);

	for (i=0; i<N; i++) {
	    unsigned char a[4],b[4];
	    b[3] = a[0] = (unsigned char)(i&255);
	    b[2] = a[1] = (unsigned char)((i>>8)&255);
	    b[1] = a[2] = (unsigned char)((i>>16)&255);
	    b[0] = a[3] = (unsigned char)((i>>24)&255);
	    DBT kbt;
            toku_fill_dbt(&kbt, a, sizeof a);
	    DBT vbt;
            toku_fill_dbt(&vbt, b, sizeof b);
	    if (0) printf("%s:%d insert: %02x%02x%02x%02x -> %02x%02x%02x%02x\n", __FILE__, __LINE__,
			  ((unsigned char*)kbt.data)[0], ((unsigned char*)kbt.data)[1], ((unsigned char*)kbt.data)[2], ((unsigned char*)kbt.data)[3],
			  ((unsigned char*)vbt.data)[0], ((unsigned char*)vbt.data)[1], ((unsigned char*)vbt.data)[2], ((unsigned char*)vbt.data)[3]);
	    toku_ft_insert(brt, &kbt, &vbt, null_txn);
	    toku_cachetable_verify(ct);
	}
	FT_CURSOR cursor=0;
	r = toku_ft_cursor(brt, &cursor, NULL, false, false);            assert(r==0);
	
	for (i=0; i<N; i++) {
	    unsigned char a[4],b[4];
	    struct check_pair pair = {4, &a, 4, &b, 0};
	    b[3] = a[0] = (unsigned char)(i&255);
	    b[2] = a[1] = (unsigned char)((i>>8)&255);
	    b[1] = a[2] = (unsigned char)((i>>16)&255);
	    b[0] = a[3] = (unsigned char)((i>>24)&255);
	    r = toku_ft_cursor_get(cursor, NULL, lookup_checkf, &pair, DB_NEXT);
	    assert(r==0);
	    assert(pair.call_count==1);
	    toku_cachetable_verify(ct);
	}
        toku_ft_cursor_close(cursor);
	r = toku_close_ft_handle_nolsn(brt, 0);
	assert(r==0);
    }
    toku_cachetable_close(&ct);
    
}

static int test_ft_cursor_keycompare(DB *desc __attribute__((unused)), const DBT *a, const DBT *b) {
    return toku_keycompare(a->data, a->size, b->data, b->size);
}

static void test_large_kv(int bsize, int ksize, int vsize) {
    FT_HANDLE t;
    int r;
    CACHETABLE ct;

    if (verbose) printf("test_large_kv: %d %d %d\n", bsize, ksize, vsize);

    toku_cachetable_create(&ct, 0, ZERO_LSN, NULL_LOGGER);
    unlink(fname);
    r = toku_open_ft_handle(fname, 1, &t, bsize, bsize / 4, TOKU_DEFAULT_COMPRESSION_METHOD, ct, null_txn, toku_builtin_compare_fun);
    assert(r==0);

    DBT key, val;
    char *k, *v;
    XCALLOC_N(ksize, k);
    XCALLOC_N(vsize, v);
    toku_fill_dbt(&key, k, ksize);
    toku_fill_dbt(&val, v, vsize);

    toku_ft_insert(t, &key, &val, 0);

    toku_free(k);
    toku_free(v);

    r = toku_close_ft_handle_nolsn(t, 0);        assert(r==0);
    toku_cachetable_close(&ct);
}

/*
 * test the key and value limits
 * the current implementation crashes when kvsize == bsize/2 rather than fails
 */
static void test_ft_limits(void) {
    int bsize = 1024;
    int kvsize = 4;
    while (kvsize < bsize/2) {
        test_large_kv(bsize, kvsize, kvsize);        
        kvsize *= 2;
    }
}

/*
 * verify that a delete on an empty tree fails
 */
static void test_ft_delete_empty(void) {
    if (verbose) printf("test_ft_delete_empty\n");

    FT_HANDLE t;
    int r;
    CACHETABLE ct;

    toku_cachetable_create(&ct, 0, ZERO_LSN, NULL_LOGGER);
    unlink(fname);
    r = toku_open_ft_handle(fname, 1, &t, 4096, 1024, TOKU_DEFAULT_COMPRESSION_METHOD, ct, null_txn, toku_builtin_compare_fun);
    assert(r==0);

    DBT key;
    int k = toku_htonl(1);
    toku_fill_dbt(&key, &k, sizeof k);
    toku_ft_delete(t, &key, null_txn);

    r = toku_close_ft_handle_nolsn(t, 0);        assert(r==0);
    toku_cachetable_close(&ct);
}

/*
 * insert n keys, delete all n keys, verify that lookups for all the keys fail,
 * verify that a cursor walk of the tree finds nothing
 */
static void test_ft_delete_present(int n) {
    if (verbose) printf("test_ft_delete_present:%d\n", n);

    FT_HANDLE t;
    int r;
    CACHETABLE ct;
    int i;

    toku_cachetable_create(&ct, 0, ZERO_LSN, NULL_LOGGER);
    unlink(fname);
    r = toku_open_ft_handle(fname, 1, &t, 4096, 1024, TOKU_DEFAULT_COMPRESSION_METHOD, ct, null_txn, toku_builtin_compare_fun);
    assert(r==0);

    /* insert 0 .. n-1 */
    for (i=0; i<n; i++) {
        int k = toku_htonl(i);
	int v = i;
	DBT key;
        toku_fill_dbt(&key, &k, sizeof k);
	DBT val;
        toku_fill_dbt(&val, &v, sizeof v);
        toku_ft_insert(t, &key, &val, 0);
    }

    /* delete 0 .. n-1 */
    for (i=0; i<n; i++) {
        int k = toku_htonl(i);
	DBT key;
        toku_fill_dbt(&key, &k, sizeof k);
        toku_ft_delete(t, &key, null_txn);
        assert(r == 0);
    }

    /* lookups should all fail */
    for (i=0; i<n; i++) {
        int k = toku_htonl(i);
	DBT key;
        toku_fill_dbt(&key, &k, sizeof k);
	struct check_pair pair = {0, 0, 0, 0, 0};
        r = toku_ft_lookup(t, &key, lookup_checkf, &pair);
        assert(r == DB_NOTFOUND);
	assert(pair.call_count==0);
    }

    /* cursor should not find anything */
    FT_CURSOR cursor=0;

    r = toku_ft_cursor(t, &cursor, NULL, false, false);
    assert(r == 0);

    {
	struct check_pair pair = {0,0,0,0,0};
	r = toku_ft_cursor_get(cursor, NULL, lookup_checkf, &pair, DB_FIRST);
	assert(r != 0);
	assert(pair.call_count==0);
    }

    toku_ft_cursor_close(cursor);

    r = toku_close_ft_handle_nolsn(t, 0);        assert(r==0);
    toku_cachetable_close(&ct);
}

static void test_ft_delete_not_present(int n) {
    if (verbose) printf("test_ft_delete_not_present:%d\n", n);

    FT_HANDLE t;
    int r;
    CACHETABLE ct;
    int i;

    toku_cachetable_create(&ct, 0, ZERO_LSN, NULL_LOGGER);
    unlink(fname);
    r = toku_open_ft_handle(fname, 1, &t, 4096, 1024, TOKU_DEFAULT_COMPRESSION_METHOD, ct, null_txn, toku_builtin_compare_fun);
    assert(r==0);

    DBT key, val;
    int k, v;

    /* insert 0 .. n-1 */
    for (i=0; i<n; i++) {
        k = toku_htonl(i); v = i;
        toku_fill_dbt(&key, &k, sizeof k);
        toku_fill_dbt(&val, &v, sizeof v);
        toku_ft_insert(t, &key, &val, 0);
    }

    /* delete 0 .. n-1 */
    for (i=0; i<n; i++) {
        k = toku_htonl(i);
        toku_fill_dbt(&key, &k, sizeof k);
        toku_ft_delete(t, &key, null_txn);
        assert(r == 0);
    }

    /* try to delete key n+1 not in the tree */
    k = toku_htonl(n+1);
    toku_fill_dbt(&key, &k, sizeof k);
    toku_ft_delete(t, &key, null_txn);
    /* the delete may be buffered or may be executed on a leaf node, so the
       return value depends */
    if (verbose) printf("toku_ft_delete k=%d %d\n", k, r);

    r = toku_close_ft_handle_nolsn(t, 0);        assert(r==0);
    toku_cachetable_close(&ct);
}

static void test_ft_delete_cursor_first(int n) {
    if (verbose) printf("test_ft_delete_cursor_first:%d\n", n);

    FT_HANDLE t;
    int r;
    CACHETABLE ct;
    int i;

    toku_cachetable_create(&ct, 0, ZERO_LSN, NULL_LOGGER);
    unlink(fname);
    r = toku_open_ft_handle(fname, 1, &t, 4096, 1024, TOKU_DEFAULT_COMPRESSION_METHOD, ct, null_txn, toku_builtin_compare_fun);
    assert(r==0);

    /* insert 0 .. n-1 */
    for (i=0; i<n; i++) {
        int k = toku_htonl(i);
	int v = i;
	DBT key;
        toku_fill_dbt(&key, &k, sizeof k);
	DBT val;
        toku_fill_dbt(&val, &v, sizeof v);
        toku_ft_insert(t, &key, &val, 0);
    }

    /* lookups 0 .. n-1 should succeed */
    for (i=0; i<n; i++) {
        int k = toku_htonl(i);
	DBT key;
        toku_fill_dbt(&key, &k, sizeof k);
	int k2 = k;
	int v = i;
	struct check_pair pair = {sizeof k, &k2, sizeof v, &v, 0};
        r = toku_ft_lookup(t, &key, lookup_checkf, &pair);
        assert(r == 0);
	assert(pair.call_count==1);
    }

    /* delete 0 .. n-2 */
    for (i=0; i<n-1; i++) {
	{
	    int k = toku_htonl(i);
	    DBT key;
            toku_fill_dbt(&key, &k, sizeof k);
	    toku_ft_delete(t, &key, null_txn);
	}

	{
	    int k = toku_htonl(i);
	    DBT key;
            toku_fill_dbt(&key, &k, sizeof k);
	    struct check_pair pair = {0,0,0,0,0};
	    r = toku_ft_lookup(t, &key, lookup_checkf, &pair);
	    assert(r == DB_NOTFOUND);
	    assert(pair.call_count==0);
	}
    }

    /* lookup of 0 .. n-2 should all fail */
    for (i=0; i<n-1; i++) {
        int k = toku_htonl(i);
	DBT key;
        toku_fill_dbt(&key, &k, sizeof k);
	struct check_pair pair = {0,0,0,0,0};
        r = toku_ft_lookup(t, &key, lookup_checkf, &pair);
        assert(r == DB_NOTFOUND);
	assert(pair.call_count==0);
    }

    /* cursor should find the last key: n-1 */
    FT_CURSOR cursor=0;

    r = toku_ft_cursor(t, &cursor, NULL, false, false);
    assert(r == 0);

    {
	int kv = toku_htonl(n-1);
	int vv = n-1;
	struct check_pair pair = {sizeof kv, &kv, sizeof vv, &vv, 0};
	r = toku_ft_cursor_get(cursor, NULL, lookup_checkf, &pair, DB_FIRST);
	assert(r == 0);
	assert(pair.call_count==1);
    }

    toku_ft_cursor_close(cursor);

    r = toku_close_ft_handle_nolsn(t, 0);        assert(r==0);
    toku_cachetable_close(&ct);
}

/* test for bug: insert cmd in a nonleaf node, delete removes the
   insert cmd, but lookup finds the insert cmd

   build a 2 level tree, and expect the last insertion to be
   buffered. then delete and lookup. */

static void test_insert_delete_lookup(int n) {
    if (verbose) printf("test_insert_delete_lookup:%d\n", n);

    FT_HANDLE t;
    int r;
    CACHETABLE ct;
    int i;

    toku_cachetable_create(&ct, 0, ZERO_LSN, NULL_LOGGER);
    unlink(fname);
    r = toku_open_ft_handle(fname, 1, &t, 4096, 1024, TOKU_DEFAULT_COMPRESSION_METHOD, ct, null_txn, toku_builtin_compare_fun);
    assert(r==0);

    /* insert 0 .. n-1 */
    for (i=0; i<n; i++) {
        int k = toku_htonl(i);
	int v = i;
	DBT key;
        toku_fill_dbt(&key, &k, sizeof k);
	DBT val;
        toku_fill_dbt(&val, &v, sizeof v);
        toku_ft_insert(t, &key, &val, 0);
    }

    if (n > 0) {
	{
	    int k = toku_htonl(n-1);
	    DBT key;
            toku_fill_dbt(&key, &k, sizeof k);
	    toku_ft_delete(t, &key, null_txn);
	}
	{
	    int k = toku_htonl(n-1);
	    DBT key;
            toku_fill_dbt(&key, &k, sizeof k);
	    struct check_pair pair = {0,0,0,0,0};
	    r = toku_ft_lookup(t, &key, lookup_checkf, &pair);
	    assert(r == DB_NOTFOUND);
	    assert(pair.call_count==0);
	}
    }

    r = toku_close_ft_handle_nolsn(t, 0);        assert(r==0);
    toku_cachetable_close(&ct);
}

/* insert <0,0>, <0,1>, .. <0,n>
   delete_both <0,i> for all even i
   verify <0,i> exists for all odd i */


static void test_ft_delete(void) {
    test_ft_delete_empty(); 
    test_ft_delete_present(1); 
    test_ft_delete_present(100); 
    test_ft_delete_present(500); 
    test_ft_delete_not_present(1); 
    test_ft_delete_not_present(100); 
    test_ft_delete_not_present(500); 
    test_ft_delete_cursor_first(1); 
    test_ft_delete_cursor_first(100); 
    test_ft_delete_cursor_first(500); 
    test_ft_delete_cursor_first(10000); 
    test_insert_delete_lookup(2);   
    test_insert_delete_lookup(512); 
}

static void test_new_ft_cursor_create_close (void) {
    int r;
    FT_HANDLE brt=0;
    int n = 8;
    FT_CURSOR cursors[n];

    toku_ft_handle_create(&brt);

    int i;
    for (i=0; i<n; i++) {
        r = toku_ft_cursor(brt, &cursors[i], NULL, false, false); assert(r == 0);
    }

    for (i=0; i<n; i++) {
        toku_ft_cursor_close(cursors[i]);
    }

    r = toku_close_ft_handle_nolsn(brt, 0); assert(r == 0);
}

static void test_new_ft_cursor_first(int n) {
    if (verbose) printf("test_ft_cursor_first:%d\n", n);

    FT_HANDLE t=0;
    int r;
    CACHETABLE ct;
    int i;

    toku_cachetable_create(&ct, 0, ZERO_LSN, NULL_LOGGER);
    unlink(fname);
    toku_ft_handle_create(&t);
    toku_ft_handle_set_nodesize(t, 4096);
    r = toku_ft_handle_open(t, fname, 1, 1, ct, null_txn); assert(r==0);

    DBT key, val;
    int k, v;

    for (i=0; i<n; i++) {
        k = toku_htonl(i); v = toku_htonl(i);
        toku_ft_insert(t, toku_fill_dbt(&key, &k, sizeof k), toku_fill_dbt(&val, &v, sizeof v), 0); assert(r == 0);
    }

    FT_CURSOR cursor=0;

    r = toku_ft_cursor(t, &cursor, NULL, false, false); assert(r == 0);

    toku_init_dbt(&key); key.flags = DB_DBT_REALLOC;
    toku_init_dbt(&val); val.flags = DB_DBT_REALLOC;

    for (i=0; ; i++) {
	int kv = toku_htonl(i);
	int vv = toku_htonl(i);
	struct check_pair pair = {sizeof kv, &kv, sizeof vv, &vv, 0};
        r = toku_ft_cursor_get(cursor, NULL, lookup_checkf, &pair, DB_FIRST);
        if (r != 0) {
	    assert(pair.call_count==0);
	    break;
	}
	assert(pair.call_count==1);

        r = toku_ft_cursor_delete(cursor, 0, null_txn); assert(r == 0);
    }
    assert(i == n);

    if (key.data) toku_free(key.data);
    if (val.data) toku_free(val.data);

    toku_ft_cursor_close(cursor);
    r = toku_close_ft_handle_nolsn(t, 0); assert(r==0);
    toku_cachetable_close(&ct);
}

static void test_new_ft_cursor_last(int n) {
    if (verbose) printf("test_ft_cursor_last:%d\n", n);

    FT_HANDLE t=0;
    int r;
    CACHETABLE ct;
    int i;

    toku_cachetable_create(&ct, 0, ZERO_LSN, NULL_LOGGER);
    unlink(fname);
    toku_ft_handle_create(&t);
    toku_ft_handle_set_nodesize(t, 4096);
    r = toku_ft_handle_open(t, fname, 1, 1, ct, null_txn); assert(r==0);

    DBT key, val;
    int k, v;

    for (i=0; i<n; i++) {
        k = toku_htonl(i); v = toku_htonl(i);
        toku_ft_insert(t, toku_fill_dbt(&key, &k, sizeof k), toku_fill_dbt(&val, &v, sizeof v), 0); assert(r == 0);
    }

    FT_CURSOR cursor=0;

    r = toku_ft_cursor(t, &cursor, NULL, false, false); assert(r == 0);

    toku_init_dbt(&key); key.flags = DB_DBT_REALLOC;
    toku_init_dbt(&val); val.flags = DB_DBT_REALLOC;

    for (i=n-1; ; i--) {
	int kk = toku_htonl(i);
	int vv = toku_htonl(i);
	struct check_pair pair = {sizeof kk, &kk, sizeof vv, &vv, 0};
        r = toku_ft_cursor_get(cursor, NULL, lookup_checkf, &pair, DB_LAST);
        if (r != 0) {
	    assert(pair.call_count==0);
	    break;
	}
	assert(pair.call_count==1);

	//if (n==512 && i<=360) { printf("i=%d\n", i); toku_dump_ft(stdout, t); }
        r = toku_ft_cursor_delete(cursor, 0, null_txn); assert(r == 0);
    }
    assert(i == -1);

    if (key.data) toku_free(key.data);
    if (val.data) toku_free(val.data);

    toku_ft_cursor_close(cursor);
    r = toku_close_ft_handle_nolsn(t, 0); assert(r==0);
    toku_cachetable_close(&ct);
}

static void test_new_ft_cursor_next(int n) {
    if (verbose) printf("test_ft_cursor_next:%d\n", n);

    FT_HANDLE t=0;
    int r;
    CACHETABLE ct;
    int i;

    toku_cachetable_create(&ct, 0, ZERO_LSN, NULL_LOGGER);
    unlink(fname);
    toku_ft_handle_create(&t);
    toku_ft_handle_set_nodesize(t, 4096);
    r = toku_ft_handle_open(t, fname, 1, 1, ct, null_txn); assert(r==0);

    for (i=0; i<n; i++) {
	DBT key, val;
	int k = toku_htonl(i);
	int v = toku_htonl(i);
        toku_ft_insert(t, toku_fill_dbt(&key, &k, sizeof k), toku_fill_dbt(&val, &v, sizeof v), 0); assert(r == 0);
    }

    FT_CURSOR cursor=0;

    r = toku_ft_cursor(t, &cursor, NULL, false, false); assert(r == 0);

    for (i=0; ; i++) {
	int kk = toku_htonl(i);
	int vv = toku_htonl(i);
	struct check_pair pair = {sizeof kk, &kk, sizeof vv, &vv, 0};
        r = toku_ft_cursor_get(cursor, NULL, lookup_checkf, &pair, DB_NEXT);
        if (r != 0) {
	    assert(pair.call_count ==0);
	    break;
	}
	assert(pair.call_count==1);
    }
    assert(i == n);

    toku_ft_cursor_close(cursor);
    r = toku_close_ft_handle_nolsn(t, 0); assert(r==0);
    toku_cachetable_close(&ct);
}

static void test_new_ft_cursor_prev(int n) {
    if (verbose) printf("test_ft_cursor_prev:%d\n", n);

    FT_HANDLE t=0;
    int r;
    CACHETABLE ct;
    int i;

    toku_cachetable_create(&ct, 0, ZERO_LSN, NULL_LOGGER);
    unlink(fname);
    toku_ft_handle_create(&t);
    toku_ft_handle_set_nodesize(t, 4096);
    r = toku_ft_handle_open(t, fname, 1, 1, ct, null_txn); assert(r==0);

    for (i=0; i<n; i++) {
	DBT key, val;
        int k = toku_htonl(i);
	int v = toku_htonl(i);
        toku_ft_insert(t, toku_fill_dbt(&key, &k, sizeof k), toku_fill_dbt(&val, &v, sizeof v), 0); assert(r == 0);
    }

    FT_CURSOR cursor=0;

    r = toku_ft_cursor(t, &cursor, NULL, false, false); assert(r == 0);

    for (i=n-1; ; i--) {
	int kk = toku_htonl(i);
	int vv = toku_htonl(i);
	struct check_pair pair = {sizeof kk, &kk, sizeof vv, &vv, 0};
        r = toku_ft_cursor_get(cursor, NULL, lookup_checkf, &pair, DB_PREV);
        if (r != 0) {
	    assert(pair.call_count==0);
	    break;
	}
	assert(pair.call_count==1);
    }
    assert(i == -1);

    toku_ft_cursor_close(cursor);
    r = toku_close_ft_handle_nolsn(t, 0); assert(r==0);
    toku_cachetable_close(&ct);
}

static void test_new_ft_cursor_current(int n) {
    if (verbose) printf("test_ft_cursor_current:%d\n", n);

    FT_HANDLE t=0;
    int r;
    CACHETABLE ct;
    int i;

    toku_cachetable_create(&ct, 0, ZERO_LSN, NULL_LOGGER);
    unlink(fname);
    toku_ft_handle_create(&t);
    toku_ft_handle_set_nodesize(t, 4096);
    r = toku_ft_handle_open(t, fname, 1, 1, ct, null_txn); assert(r==0);

    for (i=0; i<n; i++) {
        int k = toku_htonl(i);
	int v = toku_htonl(i);
	DBT key, val;
        toku_ft_insert(t, toku_fill_dbt(&key, &k, sizeof k), toku_fill_dbt(&val, &v, sizeof v), 0); assert(r == 0);
    }

    FT_CURSOR cursor=0;

    r = toku_ft_cursor(t, &cursor, NULL, false, false); assert(r == 0);

    for (i=0; ; i++) {
	{
	    int kk = toku_htonl(i);
	    int vv = toku_htonl(i);
	    struct check_pair pair = {sizeof kk, &kk, sizeof vv, &vv, 0};
	    r = toku_ft_cursor_get(cursor, NULL, lookup_checkf, &pair, DB_FIRST);
	    if (r != 0) {
		assert(pair.call_count==0);
		break;
	    }
	    assert(pair.call_count==1);
	}
	{
	    int kk = toku_htonl(i);
	    int vv = toku_htonl(i);
	    struct check_pair pair = {sizeof kk, &kk, sizeof vv, &vv, 0};
	    r = toku_ft_cursor_get(cursor, NULL, lookup_checkf, &pair, DB_CURRENT);
	    assert(r == 0);
	    assert(pair.call_count==1);
	}

	{
	    int kk = toku_htonl(i);
	    int vv = toku_htonl(i);
	    struct check_pair pair = {sizeof kk, &kk, sizeof vv, &vv, 0};
	    r = toku_ft_cursor_get(cursor, NULL, lookup_checkf, &pair, DB_CURRENT_BINDING);
	    assert(r == 0);
	    assert(pair.call_count==1);
	}

        r = toku_ft_cursor_delete(cursor, 0, null_txn); assert(r == 0);

	{
	    static int count=0;
	    count++;
	    struct check_pair pair = {0,0,0,0,0};
	    r = toku_ft_cursor_get(cursor, NULL, lookup_checkf, &pair, DB_CURRENT);
	    CKERR2(r,DB_NOTFOUND); // previous DB_KEYEMPTY
	    assert(pair.call_count==0);
	}

	{
	    int kk = toku_htonl(i);
	    int vv = toku_htonl(i);
	    struct check_pair pair = {sizeof kk, &kk, sizeof vv, &vv, 0};
	    r = toku_ft_cursor_get(cursor, NULL, lookup_checkf, &pair, DB_CURRENT_BINDING);
	    assert(r == 0);
	    assert(pair.call_count==1);
	}
    }
    assert(i == n);

    toku_ft_cursor_close(cursor);
    r = toku_close_ft_handle_nolsn(t, 0); assert(r==0);
    toku_cachetable_close(&ct);
}

static void test_new_ft_cursor_set_range(int n) {
    if (verbose) printf("test_ft_cursor_set_range:%d\n", n);

    int r;
    CACHETABLE ct;
    FT_HANDLE brt=0;
    FT_CURSOR cursor=0;

    toku_cachetable_create(&ct, 0, ZERO_LSN, NULL_LOGGER);
    unlink(fname);
    toku_ft_handle_create(&brt);
    toku_ft_handle_set_nodesize(brt, 4096);
    r = toku_ft_handle_open(brt, fname, 1, 1, ct, null_txn); assert(r==0);

    int i;

    /* insert keys 0, 10, 20 .. 10*(n-1) */
    int max_key = 10*(n-1);
    for (i=0; i<n; i++) {
	DBT key, val;
        int k = toku_htonl(10*i);
        int v = 10*i;
        toku_ft_insert(brt, toku_fill_dbt(&key, &k, sizeof k), toku_fill_dbt(&val, &v, sizeof v), 0); assert(r == 0);
    }

    r = toku_ft_cursor(brt, &cursor, NULL, false, false); assert(r==0);

    /* pick random keys v in 0 <= v < 10*n, the cursor should point
       to the smallest key in the tree that is >= v */
    for (i=0; i<n; i++) {

        int v = random() % (10*n);
        int k = toku_htonl(v);
        DBT key;
        toku_fill_dbt(&key, &k, sizeof k);

	int vv = (((v+9)/10)*10); // This is the value we should actually find.

	struct check_pair pair = {sizeof k,  NULL,  // NULL data means don't check it
				  sizeof vv,  &vv,
				  0};
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

    r = toku_close_ft_handle_nolsn(brt, 0); assert(r==0);

    toku_cachetable_close(&ct);
}

static void test_new_ft_cursor_set(int n, int cursor_op, DB *db) {
    if (verbose) printf("test_ft_cursor_set:%d %d %p\n", n, cursor_op, db);

    int r;
    CACHETABLE ct;
    FT_HANDLE brt;
    FT_CURSOR cursor=0;

    unlink(fname);

    toku_cachetable_create(&ct, 0, ZERO_LSN, NULL_LOGGER);

    r = toku_open_ft_handle(fname, 1, &brt, 1<<12, 1<<9, TOKU_DEFAULT_COMPRESSION_METHOD, ct, null_txn, test_ft_cursor_keycompare); assert(r==0);

    int i;

    /* insert keys 0, 10, 20 .. 10*(n-1) */
    for (i=0; i<n; i++) {
	DBT key, val;
        int k = toku_htonl(10*i);
        int v = 10*i;
        toku_ft_insert(brt, toku_fill_dbt(&key, &k, sizeof k), toku_fill_dbt(&val, &v, sizeof v), 0); assert(r == 0);
    }

    r = toku_ft_cursor(brt, &cursor, NULL, false, false); assert(r==0);

    /* set cursor to random keys in set { 0, 10, 20, .. 10*(n-1) } */
    for (i=0; i<n; i++) {

        int v = 10*(random() % n);
        int k = toku_htonl(v);
        DBT key;
        toku_fill_dbt(&key, &k, sizeof k);
	struct check_pair pair = {sizeof k, &k, sizeof v, &v, 0};
        r = toku_ft_cursor_get(cursor, &key, lookup_checkf, &pair, cursor_op);
        assert(r == 0);
	assert(pair.call_count==1);
        if (cursor_op == DB_SET) assert(key.data == &k);
    }

    /* try to set cursor to keys not in the tree, all should fail */
    for (i=0; i<10*n; i++) {
        if (i % 10 == 0)
            continue;
        int k = toku_htonl(i);
        DBT key;
        toku_fill_dbt(&key, &k, sizeof k);
	struct check_pair pair = {0,0,0,0,0};
        r = toku_ft_cursor_get(cursor, &key, lookup_checkf, &pair, DB_SET);
        assert(r == DB_NOTFOUND);
	assert(pair.call_count==0);
        assert(key.data == &k);
    }

    toku_ft_cursor_close(cursor);

    r = toku_close_ft_handle_nolsn(brt, 0); assert(r==0);

    toku_cachetable_close(&ct);
}

static void test_new_ft_cursors(void) {
    test_new_ft_cursor_create_close();
    test_new_ft_cursor_first(8);
    test_new_ft_cursor_last(8);
    test_new_ft_cursor_last(512);
    test_new_ft_cursor_next(8);
    test_new_ft_cursor_prev(8);
    test_new_ft_cursor_current(8);
    test_new_ft_cursor_next(512);
    test_new_ft_cursor_set_range(512);
    test_new_ft_cursor_set(512, DB_SET, 0);      
}

static void ft_blackbox_test (void) {

    test_wrongendian_compare(0, 2);          
    test_wrongendian_compare(1, 2);          
    test_wrongendian_compare(1, 257);        
    test_wrongendian_compare(1, 1000);        
    test_new_ft_cursors();

    test_read_what_was_written();          if (verbose) printf("did read_what_was_written\n");
    test_cursor_next();                   
    test_cursor_last_empty();             
    test_multiple_ft_handles_one_db_one_file(); 
    test_dump_empty_db();                 
    
    
    if (verbose) printf("test_multiple_files\n");
    test_multiple_files();

    test_ft_limits();

    test_ft_delete();
}

int
test_main (int argc , const char *argv[]) {
    default_parse_args(argc, argv);

    ft_blackbox_test();
    if (verbose) printf("test ok\n");
    return 0;
}
