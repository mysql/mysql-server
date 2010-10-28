/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include "includes.h"
#include "test.h"

static TOKUTXN const null_txn = 0;
static DB * const null_db = 0;

static char fname[] = __FILE__ ".brt";

static void test_dump_empty_db (void) {
    BRT t;
    CACHETABLE ct;
    int r;
    toku_memory_check=1;
    r = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER);
    assert(r==0);
    unlink(fname);
    r = toku_open_brt(fname, 1, &t, 1024, ct, null_txn, toku_builtin_compare_fun, null_db);
    assert(r==0);
    if (verbose) { r=toku_dump_brt(stdout, t); assert(r==0); }
    r = toku_close_brt(t, 0);          assert(r==0);
    r = toku_cachetable_close(&ct); assert(r==0);
    
}

/* Test running multiple trees in different files */
static void test_multiple_files_of_size (int size) {
    const char *n0 = __FILE__ "test0.brt";
    const char *n1 = __FILE__ "test1.brt";
    CACHETABLE ct;
    BRT t0,t1;
    int r,i;
    if (verbose) printf("test_multiple_files_of_size(%d)\n", size);
    unlink(n0);
    unlink(n1);
    
    r = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER);   assert(r==0);
    r = toku_open_brt(n0, 1, &t0, size, ct, null_txn, toku_builtin_compare_fun, null_db); assert(r==0);
    r = toku_open_brt(n1, 1, &t1, size, ct, null_txn, toku_builtin_compare_fun, null_db); assert(r==0);
    for (i=0; i<10000; i++) {
	char key[100],val[100];
	DBT k,v;
	snprintf(key, 100, "key%d", i);
	snprintf(val, 100, "val%d", i);
	r = toku_brt_insert(t0, toku_fill_dbt(&k, key, 1+strlen(key)), toku_fill_dbt(&v, val, 1+strlen(val)), null_txn); assert(r==0);
	snprintf(val, 100, "Val%d", i);
	r = toku_brt_insert(t1, toku_fill_dbt(&k, key, 1+strlen(key)), toku_fill_dbt(&v, val, 1+strlen(val)), null_txn); assert(r==0);
    }
    //toku_verify_brt(t0);
    //dump_brt(t0);
    //dump_brt(t1);
    r = toku_verify_brt(t0); assert(r==0);
    r = toku_verify_brt(t1); assert(r==0);

    r = toku_close_brt(t0, 0); assert(r==0);
    r = toku_close_brt(t1, 0); assert(r==0);
    r = toku_cachetable_close(&ct); assert(r==0);
    

    /* Now see if the data is all there. */
    r = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER);      assert(r==0);
    r = toku_open_brt(n0, 0, &t0, 1<<12, ct, null_txn, toku_builtin_compare_fun, null_db);
    if (verbose) printf("%s:%d r=%d\n", __FILE__, __LINE__,r);
    assert(r==0);
    r = toku_open_brt(n1, 0, &t1, 1<<12, ct, null_txn, toku_builtin_compare_fun, null_db); assert(r==0);

    for (i=0; i<10000; i++) {
	char key[100],val[100];
	snprintf(key, 100, "key%d", i);
	snprintf(val, 100, "val%d", i);
	brt_lookup_and_check_nodup(t0, key, val);
	snprintf(val, 100, "Val%d", i);
	brt_lookup_and_check_nodup(t1, key, val);
    }

    r = toku_close_brt(t0, 0); assert(r==0);
    r = toku_close_brt(t1, 0); assert(r==0);
    r = toku_cachetable_close(&ct); assert(r==0);
    
}

static void test_multiple_files (void) {
    test_multiple_files_of_size (1<<12);
    test_multiple_files_of_size (1<<20);
}

/* Test to see that a single db can be opened many times.  */
static void test_multiple_brts_one_db_one_file (void) {
    enum { MANYN = 2 };
    int i, r;
    CACHETABLE ct;
    BRT trees[MANYN];
    if (verbose) printf("test_multiple_brts_one_db_one_file:");
    
    unlink(fname);
    r = toku_brt_create_cachetable(&ct, 32, ZERO_LSN, NULL_LOGGER); assert(r==0);
    for (i=0; i<MANYN; i++) {
	r = toku_open_brt(fname, (i==0), &trees[i], 1<<12, ct, null_txn, toku_builtin_compare_fun, null_db);
	assert(r==0);
    }
    for (i=0; i<MANYN; i++) {
	char k[20], v[20];
	DBT kb, vb;
	snprintf(k, 20, "key%d", i);
	snprintf(v, 20, "val%d", i);
	r = toku_brt_insert(trees[i], toku_fill_dbt(&kb, k, strlen(k)+1), toku_fill_dbt(&vb, v, strlen(v)+1), null_txn);
	assert(r==0);
    }
    for (i=0; i<MANYN; i++) {
	char k[20],vexpect[20];
	snprintf(k, 20, "key%d", i);
	snprintf(vexpect, 20, "val%d", i);
	brt_lookup_and_check_nodup(trees[0], k, vexpect);
    }
    for (i=0; i<MANYN; i++) {
	r=toku_close_brt(trees[i], 0); assert(r==0);
    }
    r = toku_cachetable_close(&ct); assert(r==0);
    
    if (verbose) printf(" ok\n");
}


/* Check to see if data can be read that was written. */
static void  test_read_what_was_written (void) {
    CACHETABLE ct;
    BRT brt;
    int r;
    const int NVALS=10000;

    if (verbose) printf("test_read_what_was_written(): "); fflush(stdout);

    unlink(fname);
    

    r = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER);       assert(r==0);
    r = toku_open_brt(fname, 1, &brt, 1<<12, ct, null_txn, toku_builtin_compare_fun, null_db);  assert(r==0);
    r = toku_close_brt(brt, 0); assert(r==0);
    r = toku_cachetable_close(&ct); assert(r==0);

    

    /* Now see if we can read an empty tree in. */
    r = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER);     assert(r==0);
    r = toku_open_brt(fname, 0, &brt, 1<<12, ct, null_txn, toku_builtin_compare_fun, null_db);  assert(r==0);

    /* See if we can put something in it. */
    {
	DBT k,v;
	r = toku_brt_insert(brt, toku_fill_dbt(&k, "hello", 6), toku_fill_dbt(&v, "there", 6), null_txn);
	assert(r==0);
    }

    r = toku_close_brt(brt, 0); assert(r==0);
    r = toku_cachetable_close(&ct); assert(r==0);

    

    /* Now see if we can read it in and get the value. */
    r = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER);    assert(r==0);
    r = toku_open_brt(fname, 0, &brt, 1<<12, ct, null_txn, toku_builtin_compare_fun, null_db); assert(r==0);

    brt_lookup_and_check_nodup(brt, "hello", "there");

    assert(toku_verify_brt(brt)==0);

    /* Now put a bunch (NVALS) of things in. */
    {
	int i;
	for (i=0; i<NVALS; i++) {
	    char key[100],val[100];
	    DBT k,v;
	    snprintf(key, 100, "key%d", i);
	    snprintf(val, 100, "val%d", i);
	    if (i<600) {
		int verify_result=toku_verify_brt(brt);;
		assert(verify_result==0);
	    }
	    r = toku_brt_insert(brt, toku_fill_dbt(&k, key, strlen(key)+1), toku_fill_dbt(&v, val, strlen(val)+1), null_txn);
	    assert(r==0);
	    if (i<600) {
		int verify_result=toku_verify_brt(brt);
		if (verify_result) {
		    r = toku_dump_brt(stdout, brt);
		    assert(r==0);
		    assert(0);
		}
		{
		    int j;
		    for (j=0; j<=i; j++) {
			char expectedval[100];
			snprintf(key, 100, "key%d", j);
			snprintf(expectedval, 100, "val%d", j);
			brt_lookup_and_check_nodup(brt, key, expectedval);
		    }
		}
	    }
	}
    }
    if (verbose) printf("Now read them out\n");

    //show_brt_blocknumbers(brt);
    r = toku_verify_brt(brt);
    assert(r==0);
    //dump_brt(brt);

    /* See if we can read them all out again. */
    {
	int i;
	for (i=0; i<NVALS; i++) {
	    char key[100],expectedval[100];
	    snprintf(key, 100, "key%d", i);
	    snprintf(expectedval, 100, "val%d", i);
	    brt_lookup_and_check_nodup(brt, key, expectedval);
	}
    }

    r = toku_close_brt(brt, 0); assert(r==0);
    if (verbose) printf("%s:%d About to close %p\n", __FILE__, __LINE__, ct);
    r = toku_cachetable_close(&ct); assert(r==0);

    

    r = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER);    assert(r==0);
    r = toku_open_brt(fname, 0, &brt, 1<<12, ct, null_txn, toku_builtin_compare_fun, null_db); assert(r==0);

    brt_lookup_and_check_nodup(brt, "hello", "there");
    {
	int i;
	for (i=0; i<NVALS; i++) {
	    char key[100],expectedval[100];
	    snprintf(key, 100, "key%d", i);
	    snprintf(expectedval, 100, "val%d", i);
	    brt_lookup_and_check_nodup(brt, key, expectedval);
	}
    }

    r = toku_close_brt(brt, 0); assert(r==0);
    r = toku_cachetable_close(&ct); assert(r==0);

    


    if (verbose) printf(" ok\n");
}

/* Test c_get(DB_LAST) on an empty tree */
static void test_cursor_last_empty(void) {
    CACHETABLE ct;
    BRT brt;
    BRT_CURSOR cursor=0;
    int r;
    if (verbose) printf("%s", __FUNCTION__);
    unlink(fname);
    
    //printf("%s:%d %d alloced\n", __FILE__, __LINE__, toku_get_n_items_malloced()); toku_print_malloced_items();
    r = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER);       assert(r==0);
    //printf("%s:%d %d alloced\n", __FILE__, __LINE__, toku_get_n_items_malloced()); toku_print_malloced_items();
    r = toku_open_brt(fname, 1, &brt, 1<<12, ct, null_txn, toku_builtin_compare_fun, null_db);  assert(r==0);
    //printf("%s:%d %d alloced\n", __FILE__, __LINE__, toku_get_n_items_malloced()); toku_print_malloced_items();
    r = toku_brt_cursor(brt, &cursor, NULL, FALSE);            assert(r==0);
    {
	struct check_pair pair = {0,0,0,0,0};
	r = toku_brt_cursor_get(cursor, NULL, lookup_checkf, &pair, DB_LAST);
	assert(pair.call_count==0);
	assert(r==DB_NOTFOUND);
    }
    {
	struct check_pair pair = {0,0,0,0,0};
	r = toku_brt_cursor_get(cursor, NULL, lookup_checkf, &pair, DB_FIRST);
	assert(pair.call_count==0);
	assert(r==DB_NOTFOUND);
    }
    r = toku_close_brt(brt, 0);
    //printf("%s:%d %d alloced\n", __FILE__, __LINE__, toku_get_n_items_malloced()); toku_print_malloced_items();
    r = toku_cachetable_close(&ct); assert(r==0);
    //printf("%s:%d %d alloced\n", __FILE__, __LINE__, toku_get_n_items_malloced()); toku_print_malloced_items();
    
}

static void test_cursor_next (void) {
    CACHETABLE ct;
    BRT brt;
    BRT_CURSOR cursor=0;
    int r;
    DBT kbt, vbt;

    unlink(fname);
    
    r = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER);       assert(r==0);
    //printf("%s:%d %d alloced\n", __FILE__, __LINE__, toku_get_n_items_malloced()); toku_print_malloced_items();
    r = toku_open_brt(fname, 1, &brt, 1<<12, ct, null_txn, toku_builtin_compare_fun, null_db);  assert(r==0);
    //printf("%s:%d %d alloced\n", __FILE__, __LINE__, toku_get_n_items_malloced()); toku_print_malloced_items();
    r = toku_brt_insert(brt, toku_fill_dbt(&kbt, "hello", 6), toku_fill_dbt(&vbt, "there", 6), null_txn);
    r = toku_brt_insert(brt, toku_fill_dbt(&kbt, "byebye", 7), toku_fill_dbt(&vbt, "byenow", 7), null_txn);
    if (verbose) printf("%s:%d calling toku_brt_cursor(...)\n", __FILE__, __LINE__);
    r = toku_brt_cursor(brt, &cursor, NULL, FALSE);            assert(r==0);
    toku_init_dbt(&kbt);
    //printf("%s:%d %d alloced\n", __FILE__, __LINE__, toku_get_n_items_malloced()); toku_print_malloced_items();
    toku_init_dbt(&vbt);
    //printf("%s:%d %d alloced\n", __FILE__, __LINE__, toku_get_n_items_malloced()); toku_print_malloced_items();

    if (verbose) printf("%s:%d calling toku_brt_cursor_get(...)\n", __FILE__, __LINE__);
    {
	struct check_pair pair = {7, "byebye", 7, "byenow", 0};
	r = toku_brt_cursor_get(cursor, NULL, lookup_checkf, &pair, DB_NEXT);
	if (verbose) printf("%s:%d called toku_brt_cursor_get(...)\n", __FILE__, __LINE__);
	assert(r==0);
	assert(pair.call_count==1);
    }

    {
	struct check_pair pair = {6, "hello", 6, "there", 0};
	r = toku_brt_cursor_get(cursor, NULL, lookup_checkf, &pair, DB_NEXT);
	assert(r==0);
	assert(pair.call_count==1);
    }
    {
	struct check_pair pair = {0, 0, 0, 0, 0};
	r = toku_brt_cursor_get(cursor, NULL, lookup_checkf, &pair, DB_NEXT);
	assert(r==DB_NOTFOUND);
	assert(pair.call_count==0);
    }

    r = toku_close_brt(brt, 0);
    //printf("%s:%d %d alloced\n", __FILE__, __LINE__, toku_get_n_items_malloced()); toku_print_malloced_items();
    r = toku_cachetable_close(&ct); assert(r==0);
    //printf("%s:%d %d alloced\n", __FILE__, __LINE__, toku_get_n_items_malloced()); toku_print_malloced_items();
    

}

static DB nonce_db;

static int wrong_compare_fun(DB *db, const DBT *a, const DBT *b) {
    unsigned int i;
    unsigned char *ad=a->data;
    unsigned char *bd=b->data;
    unsigned int siz=a->size;
    assert(a->size==b->size);
    assert(db==&nonce_db); // make sure the db was passed  down correctly
    for (i=0; i<siz; i++) {
	if (ad[siz-1-i]<bd[siz-1-i]) return -1;
	if (ad[siz-1-i]>bd[siz-1-i]) return +1;
    }
    return 0;

}

static void test_wrongendian_compare (int wrong_p, unsigned int N) {
    CACHETABLE ct;
    BRT brt;
    int r;
    unsigned int i;

    unlink(fname);
    

    {
	char a[4]={0,1,0,0};
	char b[4]={1,0,0,0};
	DBT at, bt;
	assert(wrong_compare_fun(&nonce_db, toku_fill_dbt(&at, a, 4), toku_fill_dbt(&bt, b, 4))>0);
	assert(wrong_compare_fun(&nonce_db, toku_fill_dbt(&at, b, 4), toku_fill_dbt(&bt, a, 4))<0);
    }

    r = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER);       assert(r==0);
    //printf("%s:%d WRONG=%d\n", __FILE__, __LINE__, wrong_p);

    if (0) { // ???? Why is this commented out?
    r = toku_open_brt(fname, 1, &brt, 1<<20, ct, null_txn, wrong_p ? wrong_compare_fun : toku_builtin_compare_fun, &nonce_db);  assert(r==0);
    for (i=1; i<257; i+=255) {
	unsigned char a[4],b[4];
	b[3] = a[0] = (unsigned char)(i&255);
	b[2] = a[1] = (unsigned char)((i>>8)&255);
	b[1] = a[2] = (unsigned char)((i>>16)&255);
	b[0] = a[3] = (unsigned char)((i>>24)&255);
	DBT kbt = {.size=sizeof(a), .data=a};
	DBT vbt = {.size=sizeof(b), .data=b};
	if (verbose)
	    printf("%s:%d insert: %02x%02x%02x%02x -> %02x%02x%02x%02x\n", __FILE__, __LINE__,
		   ((char*)kbt.data)[0], ((char*)kbt.data)[1], ((char*)kbt.data)[2], ((char*)kbt.data)[3],
		   ((char*)vbt.data)[0], ((char*)vbt.data)[1], ((char*)vbt.data)[2], ((char*)vbt.data)[3]);
	r = toku_brt_insert(brt, &kbt, &vbt, null_txn);
	assert(r==0);
    }
    {
	BRT_CURSOR cursor=0;
	r = toku_brt_cursor(brt, &cursor, NULL, FALSE);            assert(r==0);

	for (i=0; i<2; i++) {
	    unsigned char a[4],b[4];
	    struct check_pair pair = {4, &a, 4, &b, 0};
	    b[3] = a[0] = (unsigned char)(i&255);
	    b[2] = a[1] = (unsigned char)((i>>8)&255);
	    b[1] = a[2] = (unsigned char)((i>>16)&255);
	    b[0] = a[3] = (unsigned char)((i>>24)&255);
	    r = toku_brt_cursor_get(cursor, NULL, lookup_checkf, &pair, DB_NEXT);
	    assert(r==0);
	    assert(pair.call_count==1);
	}


        r = toku_close_brt(brt, 0);
    }
    }

    {
	toku_cachetable_verify(ct);
	r = toku_open_brt(fname, 1, &brt, 1<<20, ct, null_txn, wrong_p ? wrong_compare_fun : toku_builtin_compare_fun, &nonce_db);  assert(r==0);
	toku_cachetable_verify(ct);

	for (i=0; i<N; i++) {
	    unsigned char a[4],b[4];
	    b[3] = a[0] = (unsigned char)(i&255);
	    b[2] = a[1] = (unsigned char)((i>>8)&255);
	    b[1] = a[2] = (unsigned char)((i>>16)&255);
	    b[0] = a[3] = (unsigned char)((i>>24)&255);
	    DBT kbt = {.size=sizeof(a), .data=a};
	    DBT vbt = {.size=sizeof(b), .data=b};
	    if (0) printf("%s:%d insert: %02x%02x%02x%02x -> %02x%02x%02x%02x\n", __FILE__, __LINE__,
			  ((unsigned char*)kbt.data)[0], ((unsigned char*)kbt.data)[1], ((unsigned char*)kbt.data)[2], ((unsigned char*)kbt.data)[3],
			  ((unsigned char*)vbt.data)[0], ((unsigned char*)vbt.data)[1], ((unsigned char*)vbt.data)[2], ((unsigned char*)vbt.data)[3]);
	    r = toku_brt_insert(brt, &kbt, &vbt, null_txn);
	    assert(r==0);
	    toku_cachetable_verify(ct);
	}
	BRT_CURSOR cursor=0;
	r = toku_brt_cursor(brt, &cursor, NULL, FALSE);            assert(r==0);
	
	for (i=0; i<N; i++) {
	    unsigned char a[4],b[4];
	    struct check_pair pair = {4, &a, 4, &b, 0};
	    b[3] = a[0] = (unsigned char)(i&255);
	    b[2] = a[1] = (unsigned char)((i>>8)&255);
	    b[1] = a[2] = (unsigned char)((i>>16)&255);
	    b[0] = a[3] = (unsigned char)((i>>24)&255);
	    r = toku_brt_cursor_get(cursor, NULL, lookup_checkf, &pair, DB_NEXT);
	    assert(r==0);
	    assert(pair.call_count==1);
	    toku_cachetable_verify(ct);
	}

	r = toku_close_brt(brt, 0);
	assert(r==0);
    }
    r = toku_cachetable_close(&ct); assert(r==0);
    
}

static int test_brt_cursor_keycompare(DB *db __attribute__((unused)), const DBT *a, const DBT *b) {
    return toku_keycompare(a->data, a->size, b->data, b->size);
}

static void test_large_kv(int bsize, int ksize, int vsize) {
    BRT t;
    int r;
    CACHETABLE ct;

    if (verbose) printf("test_large_kv: %d %d %d\n", bsize, ksize, vsize);

    r = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER);
    assert(r==0);
    unlink(fname);
    r = toku_open_brt(fname, 1, &t, bsize, ct, null_txn, toku_builtin_compare_fun, null_db);
    assert(r==0);

    DBT key, val;
    char *k, *v;
    k = toku_malloc(ksize); assert(k); memset(k, 0, ksize);
    v = toku_malloc(vsize); assert(v); memset(v, 0, vsize);
    toku_fill_dbt(&key, k, ksize);
    toku_fill_dbt(&val, v, vsize);

    r = toku_brt_insert(t, &key, &val, 0);
    assert(r == 0);

    toku_free(k);
    toku_free(v);

    r = toku_close_brt(t, 0);        assert(r==0);
    r = toku_cachetable_close(&ct);     assert(r==0);
}

/*
 * test the key and value limits
 * the current implementation crashes when kvsize == bsize/2 rather than fails
 */
static void test_brt_limits(void) {
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
static void test_brt_delete_empty(void) {
    if (verbose) printf("test_brt_delete_empty\n");

    BRT t;
    int r;
    CACHETABLE ct;

    r = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER);
    assert(r==0);
    unlink(fname);
    r = toku_open_brt(fname, 1, &t, 4096, ct, null_txn, toku_builtin_compare_fun, null_db);
    assert(r==0);

    DBT key;
    int k = toku_htonl(1);
    toku_fill_dbt(&key, &k, sizeof k);
    r = toku_brt_delete(t, &key, null_txn);
    assert(r == 0);

    r = toku_close_brt(t, 0);        assert(r==0);
    r = toku_cachetable_close(&ct);     assert(r==0);
}

/*
 * insert n keys, delete all n keys, verify that lookups for all the keys fail,
 * verify that a cursor walk of the tree finds nothing
 */
static void test_brt_delete_present(int n) {
    if (verbose) printf("test_brt_delete_present:%d\n", n);

    BRT t;
    int r;
    CACHETABLE ct;
    int i;

    r = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER);
    assert(r==0);
    unlink(fname);
    r = toku_open_brt(fname, 1, &t, 4096, ct, null_txn, toku_builtin_compare_fun, null_db);
    assert(r==0);

    /* insert 0 .. n-1 */
    for (i=0; i<n; i++) {
        int k = toku_htonl(i);
	int v = i;
	DBT key = {.size=sizeof k, .data=&k};
	DBT val = {.size=sizeof v, .data=&v};
        r = toku_brt_insert(t, &key, &val, 0);
        assert(r == 0);
    }

    /* delete 0 .. n-1 */
    for (i=0; i<n; i++) {
        int k = toku_htonl(i);
	DBT key = {.size=sizeof k, .data=&k};
        r = toku_brt_delete(t, &key, null_txn);
        assert(r == 0);
    }

    /* lookups should all fail */
    for (i=0; i<n; i++) {
        int k = toku_htonl(i);
	DBT key = {.size=sizeof k, .data=&k};
	struct check_pair pair = {0, 0, 0, 0, 0};
        r = toku_brt_lookup(t, &key, lookup_checkf, &pair);
        assert(r == DB_NOTFOUND);
	assert(pair.call_count==0);
    }

    /* cursor should not find anything */
    BRT_CURSOR cursor=0;

    r = toku_brt_cursor(t, &cursor, NULL, FALSE);
    assert(r == 0);

    {
	struct check_pair pair = {0,0,0,0,0};
	r = toku_brt_cursor_get(cursor, NULL, lookup_checkf, &pair, DB_FIRST);
	assert(r != 0);
	assert(pair.call_count==0);
    }

    r = toku_brt_cursor_close(cursor);
    assert(r == 0);

    r = toku_close_brt(t, 0);        assert(r==0);
    r = toku_cachetable_close(&ct);     assert(r==0);
}

static void test_brt_delete_not_present(int n) {
    if (verbose) printf("test_brt_delete_not_present:%d\n", n);

    BRT t;
    int r;
    CACHETABLE ct;
    int i;

    r = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER);
    assert(r==0);
    unlink(fname);
    r = toku_open_brt(fname, 1, &t, 4096, ct, null_txn, toku_builtin_compare_fun, null_db);
    assert(r==0);

    DBT key, val;
    int k, v;

    /* insert 0 .. n-1 */
    for (i=0; i<n; i++) {
        k = toku_htonl(i); v = i;
        toku_fill_dbt(&key, &k, sizeof k);
        toku_fill_dbt(&val, &v, sizeof v);
        r = toku_brt_insert(t, &key, &val, 0);
        assert(r == 0);
    }

    /* delete 0 .. n-1 */
    for (i=0; i<n; i++) {
        k = toku_htonl(i);
        toku_fill_dbt(&key, &k, sizeof k);
        r = toku_brt_delete(t, &key, null_txn);
        assert(r == 0);
    }

    /* try to delete key n+1 not in the tree */
    k = toku_htonl(n+1);
    toku_fill_dbt(&key, &k, sizeof k);
    r = toku_brt_delete(t, &key, null_txn);
    /* the delete may be buffered or may be executed on a leaf node, so the
       return value depends */
    if (verbose) printf("toku_brt_delete k=%d %d\n", k, r);

    r = toku_close_brt(t, 0);        assert(r==0);
    r = toku_cachetable_close(&ct);     assert(r==0);
}

static void test_brt_delete_cursor_first(int n) {
    if (verbose) printf("test_brt_delete_cursor_first:%d\n", n);

    BRT t;
    int r;
    CACHETABLE ct;
    int i;

    r = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER);
    assert(r==0);
    unlink(fname);
    r = toku_open_brt(fname, 1, &t, 4096, ct, null_txn, toku_builtin_compare_fun, null_db);
    assert(r==0);

    /* insert 0 .. n-1 */
    for (i=0; i<n; i++) {
        int k = toku_htonl(i);
	int v = i;
	DBT key = {.size=sizeof k, .data=&k};
	DBT val = {.size=sizeof v, .data=&v};
        r = toku_brt_insert(t, &key, &val, 0);
        assert(r == 0);
    }

    /* lookups 0 .. n-1 should succeed */
    for (i=0; i<n; i++) {
        int k = toku_htonl(i);
	DBT key = {.size=sizeof k, .data=&k};
	int k2 = k;
	int v = i;
	struct check_pair pair = {sizeof k, &k2, sizeof v, &v, 0};
        r = toku_brt_lookup(t, &key, lookup_checkf, &pair);
        assert(r == 0);
	assert(pair.call_count==1);
    }

    /* delete 0 .. n-2 */
    for (i=0; i<n-1; i++) {
	{
	    int k = toku_htonl(i);
	    DBT key = {.size=sizeof k, .data=&k};
	    r = toku_brt_delete(t, &key, null_txn);
	    assert(r == 0);
	}

	{
	    int k = toku_htonl(i);
	    DBT key = {.size=sizeof k, .data=&k};
	    struct check_pair pair = {0,0,0,0,0};
	    r = toku_brt_lookup(t, &key, lookup_checkf, &pair);
	    assert(r == DB_NOTFOUND);
	    assert(pair.call_count==0);
	}
    }

    /* lookup of 0 .. n-2 should all fail */
    for (i=0; i<n-1; i++) {
        int k = toku_htonl(i);
	DBT key = {.size=sizeof k, .data=&k};
	struct check_pair pair = {0,0,0,0,0};
        r = toku_brt_lookup(t, &key, lookup_checkf, &pair);
        assert(r == DB_NOTFOUND);
	assert(pair.call_count==0);
    }

    /* cursor should find the last key: n-1 */
    BRT_CURSOR cursor=0;

    r = toku_brt_cursor(t, &cursor, NULL, FALSE);
    assert(r == 0);

    {
	int kv = toku_htonl(n-1);
	int vv = n-1;
	struct check_pair pair = {sizeof kv, &kv, sizeof vv, &vv, 0};
	r = toku_brt_cursor_get(cursor, NULL, lookup_checkf, &pair, DB_FIRST);
	assert(r == 0);
	assert(pair.call_count==1);
    }

    r = toku_brt_cursor_close(cursor);
    assert(r == 0);

    r = toku_close_brt(t, 0);        assert(r==0);
    r = toku_cachetable_close(&ct);     assert(r==0);
}

/* test for bug: insert cmd in a nonleaf node, delete removes the
   insert cmd, but lookup finds the insert cmd

   build a 2 level tree, and expect the last insertion to be
   buffered. then delete and lookup. */

static void test_insert_delete_lookup(int n) {
    if (verbose) printf("test_insert_delete_lookup:%d\n", n);

    BRT t;
    int r;
    CACHETABLE ct;
    int i;

    r = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER);
    assert(r==0);
    unlink(fname);
    r = toku_open_brt(fname, 1, &t, 4096, ct, null_txn, toku_builtin_compare_fun, null_db);
    assert(r==0);

    /* insert 0 .. n-1 */
    for (i=0; i<n; i++) {
        int k = toku_htonl(i);
	int v = i;
	DBT key = {.size=sizeof k, .data=&k};
	DBT val = {.size=sizeof v, .data=&v};
        r = toku_brt_insert(t, &key, &val, 0);
        assert(r == 0);
    }

    if (n > 0) {
	{
	    int k = toku_htonl(n-1);
	    DBT key = {.size=sizeof k, .data=&k};
	    r = toku_brt_delete(t, &key, null_txn);
	    assert(r == 0);
	}
	{
	    int k = toku_htonl(n-1);
	    DBT key = {.size=sizeof k, .data=&k};
	    struct check_pair pair = {0,0,0,0,0};
	    r = toku_brt_lookup(t, &key, lookup_checkf, &pair);
	    assert(r == DB_NOTFOUND);
	    assert(pair.call_count==0);
	}
    }

    r = toku_close_brt(t, 0);        assert(r==0);
    r = toku_cachetable_close(&ct);     assert(r==0);
}

/* insert <0,0>, <0,1>, .. <0,n>
   delete_both <0,i> for all even i
   verify <0,i> exists for all odd i */


static void test_brt_delete(void) {
    test_brt_delete_empty(); 
    test_brt_delete_present(1); 
    test_brt_delete_present(100); 
    test_brt_delete_present(500); 
    test_brt_delete_not_present(1); 
    test_brt_delete_not_present(100); 
    test_brt_delete_not_present(500); 
    test_brt_delete_cursor_first(1); 
    test_brt_delete_cursor_first(100); 
    test_brt_delete_cursor_first(500); 
    test_brt_delete_cursor_first(10000); 
    test_insert_delete_lookup(2);   
    test_insert_delete_lookup(512); 
}

static void test_new_brt_cursor_create_close (void) {
    int r;
    BRT brt=0;
    int n = 8;
    BRT_CURSOR cursors[n];

    r = toku_brt_create(&brt); assert(r == 0);

    int i;
    for (i=0; i<n; i++) {
        r = toku_brt_cursor(brt, &cursors[i], NULL, FALSE); assert(r == 0);
    }

    for (i=0; i<n; i++) {
        r = toku_brt_cursor_close(cursors[i]); assert(r == 0);
    }

    r = toku_close_brt(brt, 0); assert(r == 0);
}

static void test_new_brt_cursor_first(int n) {
    if (verbose) printf("test_brt_cursor_first:%d\n", n);

    BRT t=0;
    int r;
    CACHETABLE ct;
    int i;

    r = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER); assert(r==0);
    unlink(fname);
    r = toku_brt_create(&t); assert(r == 0);
    r = toku_brt_set_nodesize(t, 4096); assert(r == 0);
    r = toku_brt_open(t, fname, 1, 1, ct, null_txn, 0); assert(r==0);

    DBT key, val;
    int k, v;

    for (i=0; i<n; i++) {
        k = toku_htonl(i); v = toku_htonl(i);
        r = toku_brt_insert(t, toku_fill_dbt(&key, &k, sizeof k), toku_fill_dbt(&val, &v, sizeof v), 0); assert(r == 0);
    }

    BRT_CURSOR cursor=0;

    r = toku_brt_cursor(t, &cursor, NULL, FALSE); assert(r == 0);

    toku_init_dbt(&key); key.flags = DB_DBT_REALLOC;
    toku_init_dbt(&val); val.flags = DB_DBT_REALLOC;

    for (i=0; ; i++) {
	int kv = toku_htonl(i);
	int vv = toku_htonl(i);
	struct check_pair pair = {sizeof kv, &kv, sizeof vv, &vv, 0};
        r = toku_brt_cursor_get(cursor, NULL, lookup_checkf, &pair, DB_FIRST);
        if (r != 0) {
	    assert(pair.call_count==0);
	    break;
	}
	assert(pair.call_count==1);

        r = toku_brt_cursor_delete(cursor, 0, null_txn); assert(r == 0);
    }
    assert(i == n);

    if (key.data) toku_free(key.data);
    if (val.data) toku_free(val.data);

    r = toku_brt_cursor_close(cursor); assert(r == 0);
    r = toku_close_brt(t, 0); assert(r==0);
    r = toku_cachetable_close(&ct);assert(r==0);
}

static void test_new_brt_cursor_last(int n) {
    if (verbose) printf("test_brt_cursor_last:%d\n", n);

    BRT t=0;
    int r;
    CACHETABLE ct;
    int i;

    r = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER); assert(r==0);
    unlink(fname);
    r = toku_brt_create(&t); assert(r == 0);
    r = toku_brt_set_nodesize(t, 4096); assert(r == 0);
    r = toku_brt_open(t, fname, 1, 1, ct, null_txn, 0); assert(r==0);

    DBT key, val;
    int k, v;

    for (i=0; i<n; i++) {
        k = toku_htonl(i); v = toku_htonl(i);
        r = toku_brt_insert(t, toku_fill_dbt(&key, &k, sizeof k), toku_fill_dbt(&val, &v, sizeof v), 0); assert(r == 0);
    }

    BRT_CURSOR cursor=0;

    r = toku_brt_cursor(t, &cursor, NULL, FALSE); assert(r == 0);

    toku_init_dbt(&key); key.flags = DB_DBT_REALLOC;
    toku_init_dbt(&val); val.flags = DB_DBT_REALLOC;

    for (i=n-1; ; i--) {
	int kk = toku_htonl(i);
	int vv = toku_htonl(i);
	struct check_pair pair = {sizeof kk, &kk, sizeof vv, &vv, 0};
        r = toku_brt_cursor_get(cursor, NULL, lookup_checkf, &pair, DB_LAST);
        if (r != 0) {
	    assert(pair.call_count==0);
	    break;
	}
	assert(pair.call_count==1);

	//if (n==512 && i<=360) { printf("i=%d\n", i); toku_dump_brt(stdout, t); }
        r = toku_brt_cursor_delete(cursor, 0, null_txn); assert(r == 0);
    }
    assert(i == -1);

    if (key.data) toku_free(key.data);
    if (val.data) toku_free(val.data);

    r = toku_brt_cursor_close(cursor); assert(r == 0);
    r = toku_close_brt(t, 0); assert(r==0);
    r = toku_cachetable_close(&ct);assert(r==0);
}

static void test_new_brt_cursor_next(int n) {
    if (verbose) printf("test_brt_cursor_next:%d\n", n);

    BRT t=0;
    int r;
    CACHETABLE ct;
    int i;

    r = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER); assert(r==0);
    unlink(fname);
    r = toku_brt_create(&t); assert(r == 0);
    r = toku_brt_set_nodesize(t, 4096); assert(r == 0);
    r = toku_brt_open(t, fname, 1, 1, ct, null_txn, 0); assert(r==0);

    for (i=0; i<n; i++) {
	DBT key, val;
	int k = toku_htonl(i);
	int v = toku_htonl(i);
        r = toku_brt_insert(t, toku_fill_dbt(&key, &k, sizeof k), toku_fill_dbt(&val, &v, sizeof v), 0); assert(r == 0);
    }

    BRT_CURSOR cursor=0;

    r = toku_brt_cursor(t, &cursor, NULL, FALSE); assert(r == 0);

    for (i=0; ; i++) {
	int kk = toku_htonl(i);
	int vv = toku_htonl(i);
	struct check_pair pair = {sizeof kk, &kk, sizeof vv, &vv, 0};
        r = toku_brt_cursor_get(cursor, NULL, lookup_checkf, &pair, DB_NEXT);
        if (r != 0) {
	    assert(pair.call_count ==0);
	    break;
	}
	assert(pair.call_count==1);
    }
    assert(i == n);

    r = toku_brt_cursor_close(cursor); assert(r == 0);
    r = toku_close_brt(t, 0); assert(r==0);
    r = toku_cachetable_close(&ct);assert(r==0);
}

static void test_new_brt_cursor_prev(int n) {
    if (verbose) printf("test_brt_cursor_prev:%d\n", n);

    BRT t=0;
    int r;
    CACHETABLE ct;
    int i;

    r = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER); assert(r==0);
    unlink(fname);
    r = toku_brt_create(&t); assert(r == 0);
    r = toku_brt_set_nodesize(t, 4096); assert(r == 0);
    r = toku_brt_open(t, fname, 1, 1, ct, null_txn, 0); assert(r==0);

    for (i=0; i<n; i++) {
	DBT key, val;
        int k = toku_htonl(i);
	int v = toku_htonl(i);
        r = toku_brt_insert(t, toku_fill_dbt(&key, &k, sizeof k), toku_fill_dbt(&val, &v, sizeof v), 0); assert(r == 0);
    }

    BRT_CURSOR cursor=0;

    r = toku_brt_cursor(t, &cursor, NULL, FALSE); assert(r == 0);

    for (i=n-1; ; i--) {
	int kk = toku_htonl(i);
	int vv = toku_htonl(i);
	struct check_pair pair = {sizeof kk, &kk, sizeof vv, &vv, 0};
        r = toku_brt_cursor_get(cursor, NULL, lookup_checkf, &pair, DB_PREV);
        if (r != 0) {
	    assert(pair.call_count==0);
	    break;
	}
	assert(pair.call_count==1);
    }
    assert(i == -1);

    r = toku_brt_cursor_close(cursor); assert(r == 0);
    r = toku_close_brt(t, 0); assert(r==0);
    r = toku_cachetable_close(&ct);assert(r==0);
}

static void test_new_brt_cursor_current(int n) {
    if (verbose) printf("test_brt_cursor_current:%d\n", n);

    BRT t=0;
    int r;
    CACHETABLE ct;
    int i;

    r = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER); assert(r==0);
    unlink(fname);
    r = toku_brt_create(&t); assert(r == 0);
    r = toku_brt_set_nodesize(t, 4096); assert(r == 0);
    r = toku_brt_open(t, fname, 1, 1, ct, null_txn, 0); assert(r==0);

    for (i=0; i<n; i++) {
        int k = toku_htonl(i);
	int v = toku_htonl(i);
	DBT key, val;
        r = toku_brt_insert(t, toku_fill_dbt(&key, &k, sizeof k), toku_fill_dbt(&val, &v, sizeof v), 0); assert(r == 0);
    }

    BRT_CURSOR cursor=0;

    r = toku_brt_cursor(t, &cursor, NULL, FALSE); assert(r == 0);

    for (i=0; ; i++) {
	{
	    int kk = toku_htonl(i);
	    int vv = toku_htonl(i);
	    struct check_pair pair = {sizeof kk, &kk, sizeof vv, &vv, 0};
	    r = toku_brt_cursor_get(cursor, NULL, lookup_checkf, &pair, DB_FIRST);
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
	    r = toku_brt_cursor_get(cursor, NULL, lookup_checkf, &pair, DB_CURRENT);
	    assert(r == 0);
	    assert(pair.call_count==1);
	}

	{
	    int kk = toku_htonl(i);
	    int vv = toku_htonl(i);
	    struct check_pair pair = {sizeof kk, &kk, sizeof vv, &vv, 0};
	    r = toku_brt_cursor_get(cursor, NULL, lookup_checkf, &pair, DB_CURRENT_BINDING);
	    assert(r == 0);
	    assert(pair.call_count==1);
	}

        r = toku_brt_cursor_delete(cursor, 0, null_txn); assert(r == 0);

	{
	    static int count=0;
	    count++;
	    struct check_pair pair = {0,0,0,0,0};
	    r = toku_brt_cursor_get(cursor, NULL, lookup_checkf, &pair, DB_CURRENT);
	    CKERR2(r,DB_NOTFOUND); // previous DB_KEYEMPTY
	    assert(pair.call_count==0);
	}

	{
	    int kk = toku_htonl(i);
	    int vv = toku_htonl(i);
	    struct check_pair pair = {sizeof kk, &kk, sizeof vv, &vv, 0};
	    r = toku_brt_cursor_get(cursor, NULL, lookup_checkf, &pair, DB_CURRENT_BINDING);
	    assert(r == 0);
	    assert(pair.call_count==1);
	}
    }
    assert(i == n);

    r = toku_brt_cursor_close(cursor); assert(r == 0);
    r = toku_close_brt(t, 0); assert(r==0);
    r = toku_cachetable_close(&ct);assert(r==0);
}

static void test_new_brt_cursor_set_range(int n) {
    if (verbose) printf("test_brt_cursor_set_range:%d\n", n);

    int r;
    CACHETABLE ct;
    BRT brt=0;
    BRT_CURSOR cursor=0;

    r = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER); assert(r==0);
    unlink(fname);
    r = toku_brt_create(&brt); assert(r == 0);
    r = toku_brt_set_nodesize(brt, 4096); assert(r == 0);
    r = toku_brt_open(brt, fname, 1, 1, ct, null_txn, 0); assert(r==0);

    int i;

    /* insert keys 0, 10, 20 .. 10*(n-1) */
    int max_key = 10*(n-1);
    for (i=0; i<n; i++) {
	DBT key, val;
        int k = toku_htonl(10*i);
        int v = 10*i;
        r = toku_brt_insert(brt, toku_fill_dbt(&key, &k, sizeof k), toku_fill_dbt(&val, &v, sizeof v), 0); assert(r == 0);
    }

    r = toku_brt_cursor(brt, &cursor, NULL, FALSE); assert(r==0);

    /* pick random keys v in 0 <= v < 10*n, the cursor should point
       to the smallest key in the tree that is >= v */
    for (i=0; i<n; i++) {

        int v = random() % (10*n);
        int k = toku_htonl(v);
        DBT key = {.size=sizeof k, .data=&k};

	int vv = (((v+9)/10)*10); // This is the value we should actually find.

	struct check_pair pair = {sizeof k,  NULL,  // NULL data means don't check it
				  sizeof vv,  &vv,
				  0};
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

    r = toku_brt_cursor_close(cursor); assert(r==0);

    r = toku_close_brt(brt, 0); assert(r==0);

    r = toku_cachetable_close(&ct); assert(r==0);
}

static void test_new_brt_cursor_set(int n, int cursor_op, DB *db) {
    if (verbose) printf("test_brt_cursor_set:%d %d %p\n", n, cursor_op, db);

    int r;
    CACHETABLE ct;
    BRT brt;
    BRT_CURSOR cursor=0;

    unlink(fname);

    r = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER); assert(r==0);

    r = toku_open_brt(fname, 1, &brt, 1<<12, ct, null_txn, test_brt_cursor_keycompare, db); assert(r==0);

    int i;

    /* insert keys 0, 10, 20 .. 10*(n-1) */
    for (i=0; i<n; i++) {
	DBT key, val;
        int k = toku_htonl(10*i);
        int v = 10*i;
        r = toku_brt_insert(brt, toku_fill_dbt(&key, &k, sizeof k), toku_fill_dbt(&val, &v, sizeof v), 0); assert(r == 0);
    }

    r = toku_brt_cursor(brt, &cursor, NULL, FALSE); assert(r==0);

    /* set cursor to random keys in set { 0, 10, 20, .. 10*(n-1) } */
    for (i=0; i<n; i++) {

        int v = 10*(random() % n);
        int k = toku_htonl(v);
        DBT key = {.size=sizeof k, .data=&k};
	struct check_pair pair = {sizeof k, &k, sizeof v, &v, 0};
        r = toku_brt_cursor_get(cursor, &key, lookup_checkf, &pair, cursor_op);
        assert(r == 0);
	assert(pair.call_count==1);
        if (cursor_op == DB_SET) assert(key.data == &k);
    }

    /* try to set cursor to keys not in the tree, all should fail */
    for (i=0; i<10*n; i++) {
        if (i % 10 == 0)
            continue;
        int k = toku_htonl(i);
        DBT key = {.size=sizeof k, .data=&k};
	struct check_pair pair = {0,0,0,0,0};
        r = toku_brt_cursor_get(cursor, &key, lookup_checkf, &pair, DB_SET);
        assert(r == DB_NOTFOUND);
	assert(pair.call_count==0);
        assert(key.data == &k);
    }

    r = toku_brt_cursor_close(cursor); assert(r==0);

    r = toku_close_brt(brt, 0); assert(r==0);

    r = toku_cachetable_close(&ct); assert(r==0);
}

static void test_new_brt_cursors(void) {
    test_new_brt_cursor_create_close();
    test_new_brt_cursor_first(8);
    test_new_brt_cursor_last(8);
    test_new_brt_cursor_last(512);
    test_new_brt_cursor_next(8);
    test_new_brt_cursor_prev(8);
    test_new_brt_cursor_current(8);
    test_new_brt_cursor_next(512);
    test_new_brt_cursor_set_range(512);
    test_new_brt_cursor_set(512, DB_SET, 0);      
}

static void brt_blackbox_test (void) {
    toku_memory_check = 1;
    test_wrongendian_compare(0, 2);          
    test_wrongendian_compare(1, 2);          
    test_wrongendian_compare(1, 257);        
    test_wrongendian_compare(1, 1000);        
    test_new_brt_cursors();

    test_read_what_was_written();          if (verbose) printf("did read_what_was_written\n");
    test_cursor_next();                   
    test_cursor_last_empty();             
    test_multiple_brts_one_db_one_file(); 
    test_dump_empty_db();                 
    
    
    if (verbose) printf("test_multiple_files\n");
    test_multiple_files();

    toku_memory_check = 1;

    test_brt_limits();

    test_brt_delete();

// This test doesn't make much sense any more.  We'll have to do revised tests for this functionality.
#if 0
    int old_brt_do_push_cmd = toku_brt_do_push_cmd;
    toku_brt_do_push_cmd = 0;

    test_brt_delete();

    toku_brt_do_push_cmd = old_brt_do_push_cmd;
#endif

}

int
test_main (int argc , const char *argv[]) {
    default_parse_args(argc, argv);

    brt_blackbox_test();
    if (verbose) printf("test ok\n");
    return 0;
}
