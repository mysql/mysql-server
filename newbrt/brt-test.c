#include "brt.h"

#include "memory.h"
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <stdlib.h>
#include <unistd.h>

extern long long n_items_malloced;

static void test0 (void) {
    BRT t;
    int r;
    CACHETABLE ct;
    char fname[]="testbrt.brt";
    printf("%s:%d test0\n", __FILE__, __LINE__);
    memory_check=1;
    memory_check_all_free();
    r = brt_create_cachetable(&ct, 0);
    assert(r==0);
    printf("%s:%d test0\n", __FILE__, __LINE__);
    unlink(fname);
    r = open_brt(fname, 0, 1, &t, 1024, ct);
    assert(r==0);
    printf("%s:%d test0\n", __FILE__, __LINE__);
    printf("%s:%d n_items_malloced=%lld\n", __FILE__, __LINE__, n_items_malloced);
    r = close_brt(t);     assert(r==0);
    printf("%s:%d n_items_malloced=%lld\n", __FILE__, __LINE__, n_items_malloced);    assert(r==0);
    r = cachetable_close(ct);
    assert(r==0);
    memory_check_all_free();
}

static void test1 (void) {
    BRT t;
    int r;
    CACHETABLE ct;
    char fname[]="testbrt.brt";
    memory_check=1;
    memory_check_all_free();
    r = brt_create_cachetable(&ct, 0);
    assert(r==0);
    unlink(fname);
    r = open_brt(fname, 0, 1, &t, 1024, ct);
    assert(r==0);
    brt_insert(t, "hello", 6, "there", 6);
    {
	bytevec val; ITEMLEN vallen;
	r = brt_lookup(t, "hello", 6, &val, &vallen);
	assert(r==0);
	assert(strcmp(val, "there")==0);
	assert(vallen==6);
    }
    r = close_brt(t);              assert(r==0);
    r = cachetable_close(ct);      assert(r==0);
    memory_check_all_free();
    printf("test1 ok\n");
}

static void test2 (int memcheck) {
    BRT t;
    int r;
    int i;
    CACHETABLE ct;
    char fname[]="testbrt.brt";
    memory_check=memcheck;
    printf("%s:%d checking\n", __FILE__, __LINE__);
    memory_check_all_free();
    r = brt_create_cachetable(&ct, 0); assert(r==0);
    unlink(fname);
    r = open_brt(fname, 0, 1, &t, 1024, ct);
    printf("%s:%d did setup\n", __FILE__, __LINE__);
    assert(r==0);
    for (i=0; i<2048; i++) {
	char key[100],val[100];
	snprintf(key,100,"hello%d",i);
	snprintf(val,100,"there%d",i);
	brt_insert(t, key, 1+strlen(key), val, 1+strlen(val));
	//printf("%s:%d did insert %d\n", __FILE__, __LINE__, i);
	if (0) {
	    brt_flush(t);
	    {
		int n = get_n_items_malloced(); 
		printf("%s:%d i=%d n_items_malloced=%d\n", __FILE__, __LINE__, i, n);
		if (n!=3) print_malloced_items();
		assert(n==3);
	    }
	}
    }
    printf("%s:%d inserted\n", __FILE__, __LINE__);
    r = close_brt(t);              assert(r==0);
    r = cachetable_close(ct);      assert(r==0);
    memory_check_all_free();
    printf("test2 ok\n");
}

static void test3 (int nodesize, int count, int memcheck) {
    BRT t;
    int r;
    struct timeval t0,t1;
    int i;
    CACHETABLE ct;
    char fname[]="testbrt.brt";
    memory_check=memcheck;
    memory_check_all_free();
    r = brt_create_cachetable(&ct, 0); assert(r==0);
    gettimeofday(&t0, 0);
    unlink(fname);
    r = open_brt(fname, 0, 1, &t, nodesize, ct);
    assert(r==0);
    for (i=0; i<count; i++) {
	char key[100],val[100];
	snprintf(key,100,"hello%d",i);
	snprintf(val,100,"there%d",i);
	brt_insert(t, key, 1+strlen(key), val, 1+strlen(val));
    }
    r = close_brt(t);              assert(r==0);
    r = cachetable_close(ct);      assert(r==0);
    memory_check_all_free();
    gettimeofday(&t1, 0);
    {
	double tdiff = (t1.tv_sec-t0.tv_sec)+1e-6*(t1.tv_usec-t0.tv_usec);
	printf("serial insertions: blocksize=%d %d insertions in %.3f seconds, %.2f insertions/second\n", nodesize, count, tdiff, count/tdiff);
    }
}

static void test4 (int nodesize, int count, int memcheck) {
    BRT t;
    int r;
    struct timeval t0,t1;
    int i;
    CACHETABLE ct;
    char fname[]="testbrt.brt";
    gettimeofday(&t0, 0);
    unlink(fname);
    memory_check=memcheck;
    memory_check_all_free();
    r = brt_create_cachetable(&ct, 0);       assert(r==0);
    r = open_brt(fname, 0, 1, &t, nodesize,ct); assert(r==0);
    for (i=0; i<count; i++) {
	char key[100],val[100];
	int rv = random();
	snprintf(key,100,"hello%d",rv);
	snprintf(val,100,"there%d",i);
	brt_insert(t, key, 1+strlen(key), val, 1+strlen(val));
    }
    r = close_brt(t);              assert(r==0);
    r = cachetable_close(ct);      assert(r==0);
    memory_check_all_free();
    gettimeofday(&t1, 0);
    {
	double tdiff = (t1.tv_sec-t0.tv_sec)+1e-6*(t1.tv_usec-t0.tv_usec);
	printf("random insertions: blocksize=%d %d insertions in %.3f seconds, %.2f insertions/second\n", nodesize, count, tdiff, count/tdiff);
    }
}

static void test5 (void) {
    int r;
    BRT t;
    int limit=100000;
    int *values;
    int i;
    CACHETABLE ct;
    char fname[]="testbrt.brt";
    memory_check_all_free();
    MALLOC_N(limit,values);
    for (i=0; i<limit; i++) values[i]=-1;
    unlink(fname);
    r = brt_create_cachetable(&ct, 0);       assert(r==0);
    r = open_brt(fname, 0, 1, &t, 1<<12, ct);   assert(r==0);
    for (i=0; i<limit/2; i++) {
	char key[100],val[100];
	int rk = random()%limit;
	int rv = random();
	if (i%1000==0) printf("w"); fflush(stdout);
	values[rk] = rv;
	snprintf(key, 100, "key%d", rk);
	snprintf(val, 100, "val%d", rv);
	brt_insert(t, key, 1+strlen(key), val, 1+strlen(val));
    }
    printf("\n");
    for (i=0; i<limit/2; i++) {
	int rk = random()%limit;
	if (values[rk]>=0) {
	    char key[100], valexpected[100];
	    bytevec val;
	    ITEMLEN vallen;
	    if (i%1000==0) printf("r"); fflush(stdout);
	    snprintf(key, 100, "key%d", rk);
	    snprintf(valexpected, 100, "val%d", values[rk]);
	    r = brt_lookup(t, key, 1+strlen(key), &val, &vallen);
	    assert(r==0);
	    assert(vallen==(1+strlen(valexpected)));
	    assert(memcmp(val,valexpected,vallen)==0);
	}
    }
    printf("\n");
    toku_free(values);
    r = close_brt(t);        assert(r==0);
    r = cachetable_close(ct); assert(r==0);
    memory_check_all_free();
}

static void test_dump_empty_db (void) {
    BRT t;
    CACHETABLE ct;
    int r;
    char fname[]="testbrt.brt";
    memory_check=1;
    r = brt_create_cachetable(&ct, 0);
    assert(r==0);
    unlink(fname);
    r = open_brt(fname, 0, 1, &t, 1024, ct);
    assert(r==0);
    dump_brt(t);
    r = close_brt(t);        assert(r==0);
    r = cachetable_close(ct); assert(r==0);
    memory_check_all_free();
}

/* Test running multiple trees in different files */
static void test_multiple_files_of_size (int size) {
    const char *n0 = "test0.brt";
    const char *n1 = "test1.brt";
    CACHETABLE ct;
    BRT t0,t1;
    int r,i;
    printf("test_multiple_files_of_size(%d)\n", size);
    unlink(n0);
    unlink(n1);
    memory_check_all_free();
    r = brt_create_cachetable(&ct, 0);      assert(r==0);
    r = open_brt(n0, 0, 1, &t0, size, ct); assert(r==0);
    r = open_brt(n1, 0, 1, &t1, size, ct); assert(r==0);
    for (i=0; i<10000; i++) {
	char key[100],val[100];
	snprintf(key, 100, "key%d", i);
	snprintf(val, 100, "val%d", i);
	brt_insert(t0, key, 1+strlen(key), val, 1+strlen(val));
	snprintf(val, 100, "Val%d", i);
	brt_insert(t1, key, 1+strlen(key), val, 1+strlen(val));
    }
    //verify_brt(t0);
    //dump_brt(t0);
    //dump_brt(t1);
    verify_brt(t0);
    verify_brt(t1);

    r = close_brt(t0); assert(r==0);
    r = close_brt(t1); assert(r==0);
    r = cachetable_close(ct); assert(r==0);
    memory_check_all_free();

    /* Now see if the data is all there. */
    r = brt_create_cachetable(&ct, 0);      assert(r==0);
    r = open_brt(n0, 0, 0, &t0, 1<<12, ct);
    printf("%s:%d r=%d\n", __FILE__, __LINE__,r);
    assert(r==0);
    r = open_brt(n1, 0, 0, &t1, 1<<12, ct); assert(r==0);
    
    for (i=0; i<10000; i++) {
	char key[100],val[100];
	bytevec actualval;
	ITEMLEN actuallen;
	snprintf(key, 100, "key%d", i);
	snprintf(val, 100, "val%d", i);
	r=brt_lookup(t0, key, 1+strlen(key), &actualval, &actuallen);
	assert(r==0);
	assert(strcmp(val,actualval)==0);
	assert(actuallen==1+strlen(val));
	snprintf(val, 100, "Val%d", i);
	r=brt_lookup(t1, key, 1+strlen(key), &actualval, &actuallen);
	assert(r==0);
	assert(strcmp(val,actualval)==0);
	assert(actuallen==1+strlen(val));
    }

    r = close_brt(t0); assert(r==0);
    r = close_brt(t1); assert(r==0);
    r = cachetable_close(ct); assert(r==0);
    memory_check_all_free();
}

static void test_multiple_files (void) {
    test_multiple_files_of_size (1<<12);
    test_multiple_files_of_size (1<<20);
}

static void test_named_db (void) {
    const char *n0 = "test0.brt";
    const char *n1 = "test1.brt";
    CACHETABLE ct;
    BRT t0;
    int r;
    printf("test_named_db\n");
    unlink(n0);
    unlink(n1);
    memory_check_all_free();
    r = brt_create_cachetable(&ct, 0);           assert(r==0);
    r = open_brt(n0, "db1", 1, &t0, 1<<12, ct); assert(r==0);

    brt_insert(t0, "good", 5, "day", 4); assert(r==0);

    r = close_brt(t0); assert(r==0);
    r = cachetable_close(ct); assert(r==0);
    memory_check_all_free();

    memory_check_all_free();
    r = brt_create_cachetable(&ct, 0);           assert(r==0);
    r = open_brt(n0, "db1", 0, &t0, 1<<12, ct); assert(r==0);

    {
	bytevec val;
	ITEMLEN vallen;
	r = brt_lookup(t0, "good", 5, &val, &vallen);
	assert(r==0);
	assert(vallen==4);
	assert(strcmp(val,"day")==0);
    }
    
    r = close_brt(t0); assert(r==0);
    r = cachetable_close(ct); assert(r==0);
    memory_check_all_free();
}

static void test_multiple_dbs (void) {
    const char *n0 = "test0.brt";
    const char *n1 = "test1.brt";
    CACHETABLE ct;
    BRT t0,t1;
    int r;
    printf("test_multiple_dbs: ");
    unlink(n0);
    unlink(n1);
    memory_check_all_free();
    r = brt_create_cachetable(&ct, 0);           assert(r==0);
    r = open_brt(n0, "db1", 1, &t0, 1<<12, ct); assert(r==0);
    r = open_brt(n1, "db2", 1, &t1, 1<<12, ct); assert(r==0);

    brt_insert(t0, "good", 5, "grief", 6); assert(r==0);
    brt_insert(t1, "bad", 4, "night", 6); assert(r==0);

    r = close_brt(t0); assert(r==0);
    r = close_brt(t1); assert(r==0);
    r = cachetable_close(ct); assert(r==0);

    memory_check_all_free();

    r = brt_create_cachetable(&ct, 0);           assert(r==0);
    r = open_brt(n0, "db1", 0, &t0, 1<<12, ct); assert(r==0);
    r = open_brt(n1, "db2", 0, &t1, 1<<12, ct); assert(r==0);

    {
	bytevec val;
	ITEMLEN vallen;
	r = brt_lookup(t0, "good", 5, &val, &vallen);
	assert(r==0);
	assert(vallen==6);
	assert(strcmp(val,"grief")==0);

	r = brt_lookup(t1, "good", 5, &val, &vallen);
	assert(r!=0);

	r = brt_lookup(t0, "bad", 4, &val, &vallen);
	assert(r!=0);

	r = brt_lookup(t1, "bad", 4, &val, &vallen);
	assert(r==0);
	assert(vallen==6);
	assert(strcmp(val,"night")==0);
    }
    
    r = close_brt(t0); assert(r==0);
    r = close_brt(t1); assert(r==0);
    r = cachetable_close(ct); assert(r==0);

    memory_check_all_free();
    printf("ok\n");
}

/* Test to see a single file can contain many databases. */
static void test_multiple_dbs_many (void) {
    enum { MANYN = 16 };
    int i, r;
    const char *name = "test.brt";
    CACHETABLE ct;
    BRT trees[MANYN];
    printf("test_multiple_dbs_many:\n");
    memory_check_all_free();
    unlink(name);
    r = brt_create_cachetable(&ct, MANYN+4);     assert(r==0);
    for (i=0; i<MANYN; i++) {
	char dbname[20];
	snprintf(dbname, 20, "db%d", i);
	r = open_brt(name, dbname, 1, &trees[i], 1<<12, ct);
	assert(r==0);
    }
    for (i=0; i<MANYN; i++) {
	char k[20], v[20];
	snprintf(k, 20, "key%d", i);
	snprintf(v, 20, "val%d", i);
	brt_insert(trees[i], k, strlen(k)+1, v, strlen(v)+1);
    }
    for (i=0; i<MANYN; i++) {
	r = close_brt(trees[i]); assert(r==0);
    }
    r = cachetable_close(ct);    assert(r==0);
    memory_check_all_free();
}

/* Test to see that a single db can be opened many times.  */
static void test_multiple_brts_one_db_one_file (void) {
    enum { MANYN = 2 };
    int i, r;
    const char *name = "test.brt";
    CACHETABLE ct;
    BRT trees[MANYN];
    printf("test_multiple_brts_one_db_one_file:");
    memory_check_all_free();
    unlink(name);
    r = brt_create_cachetable(&ct, 32); assert(r==0);
    for (i=0; i<MANYN; i++) {
	r = open_brt(name, 0, (i==0), &trees[i], 1<<12, ct);
	assert(r==0);
    }
    for (i=0; i<MANYN; i++) {
	char k[20], v[20];
	snprintf(k, 20, "key%d", i);
	snprintf(v, 20, "val%d", i);
	brt_insert(trees[i], k, strlen(k)+1, v, strlen(v)+1);
    }
    for (i=0; i<MANYN; i++) {
	char k[20],vexpect[20];
	bytevec v;
	ITEMLEN vlen;
	snprintf(k, 20, "key%d", i);
	snprintf(vexpect, 20, "val%d", i);
	r=brt_lookup(trees[0], k, strlen(k)+1, &v, &vlen);
	assert(r==0);
	assert(vlen==1+strlen(vexpect));
	assert(strcmp(v, vexpect)==0);
    }
    for (i=0; i<MANYN; i++) {
	r=close_brt(trees[i]); assert(r==0);
    }
    r = cachetable_close(ct); assert(r==0);
    memory_check_all_free();
    printf(" ok\n");
}


/* Check to see if data can be read that was written. */
static void  test_read_what_was_written (void) {
    const char *n="testbrt.brt";
    CACHETABLE ct;
    BRT brt;
    int r;
    const int NVALS=10000;

    printf("test_read_what_was_written(): "); fflush(stdout);

    unlink(n);
    memory_check_all_free();

    r = brt_create_cachetable(&ct, 0);       assert(r==0);
    r = open_brt(n, 0, 1, &brt, 1<<12, ct);  assert(r==0);
    r = close_brt(brt); assert(r==0);
    r = cachetable_close(ct); assert(r==0);

    memory_check_all_free();

    /* Now see if we can read an empty tree in. */
    r = brt_create_cachetable(&ct, 0);       assert(r==0);
    r = open_brt(n, 0, 0, &brt, 1<<12, ct);  assert(r==0);

    /* See if we can put something in it. */
    brt_insert(brt, "hello", 6, "there", 6);

    r = close_brt(brt); assert(r==0);
    r = cachetable_close(ct); assert(r==0);
    
    memory_check_all_free();

    /* Now see if we can read it in and get the value. */
    r = brt_create_cachetable(&ct, 0);       assert(r==0);
    r = open_brt(n, 0, 0, &brt, 1<<12, ct); assert(r==0);

    {
	bytevec val;
	ITEMLEN vallen;
	r = brt_lookup(brt, "hello", 6, &val, &vallen);
	assert(r==0);
	assert(vallen==6);
	assert(strcmp(val,"there")==0);
    }

    assert(verify_brt(brt)==0);

    /* Now put a bunch (VALS) of things in. */
    {
	int i;
	for (i=0; i<NVALS; i++) {
	    char key[100],val[100];
	    snprintf(key, 100, "key%d", i);
	    snprintf(val, 100, "val%d", i);
	    if (i<600) {
		int verify_result=verify_brt(brt);;
		assert(verify_result==0);
	    }
	    brt_insert(brt, key, strlen(key)+1, val, strlen(val)+1);
	    if (i<600) {
		int verify_result=verify_brt(brt);
		if (verify_result) {
		    dump_brt(brt);
		    assert(0);
		}
		{
		    int j;
		    for (j=0; j<=i; j++) {
			char expectedval[100];
			bytevec val;
			ITEMLEN vallen;
			snprintf(key, 100, "key%d", j);
			snprintf(expectedval, 100, "val%d", j);
			r=brt_lookup(brt, key, strlen(key)+1, &val, &vallen);
			if (r!=0) {
			    printf("%s:%d r=%d on lookup(key=%s) after i=%d\n", __FILE__, __LINE__, r, key, i);
			    dump_brt(brt);
			}
			assert(r==0);
		    }
		}
	    }
	}
    }
    printf("Now read them out\n");

    //show_brt_blocknumbers(brt);
    verify_brt(brt);
    //dump_brt(brt);

    /* See if we can read them all out again. */
    {
	int i;
	for (i=0; i<NVALS; i++) {
	    char key[100],expectedval[100];
	    bytevec val;
	    ITEMLEN vallen;
	    snprintf(key, 100, "key%d", i);
	    snprintf(expectedval, 100, "val%d", i);
	    r=brt_lookup(brt, key, strlen(key)+1, &val, &vallen);
	    if (r!=0) printf("%s:%d r=%d on key=%s\n", __FILE__, __LINE__, r, key);
	    assert(r==0);
	    
	}
    }

    r = close_brt(brt); assert(r==0);
    r = cachetable_close(ct); assert(r==0);
    
    memory_check_all_free();
    
    r = brt_create_cachetable(&ct, 0);       assert(r==0);
    r = open_brt(n, 0, 0, &brt, 1<<12, ct); assert(r==0);

    {
	bytevec val;
	ITEMLEN vallen;
	r = brt_lookup(brt, "hello", 6, &val, &vallen);
	assert(r==0);
	assert(vallen==6);
	assert(strcmp(val,"there")==0);
    }
    {
	int i;
	for (i=0; i<NVALS; i++) {
	    char key[100],expectedval[100];
	    bytevec val;
	    ITEMLEN vallen;
	    snprintf(key, 100, "key%d", i);
	    snprintf(expectedval, 100, "val%d", i);
	    r=brt_lookup(brt, key, strlen(key)+1, &val, &vallen);
	    if (r!=0) printf("%s:%d r=%d on key=%s\n", __FILE__, __LINE__, r, key);
	    assert(r==0);
	    
	}
    }
    
    r = close_brt(brt); assert(r==0);
    r = cachetable_close(ct); assert(r==0);
    
    memory_check_all_free();


    printf(" ok\n");
}

extern void pma_show_stats (void);

/* Test c_get(DB_LAST) on an empty tree */
void test_cursor_last_empty(void) {
    const char *n="testbrt.brt";
    CACHETABLE ct;
    BRT brt;
    BRT_CURSOR cursor;
    int r;
    DBT kbt, vbt;
    printf("%s", __FUNCTION__);
    unlink(n);
    memory_check_all_free();
    //printf("%s:%d %d alloced\n", __FILE__, __LINE__, get_n_items_malloced()); print_malloced_items();
    r = brt_create_cachetable(&ct, 0);       assert(r==0);
    //printf("%s:%d %d alloced\n", __FILE__, __LINE__, get_n_items_malloced()); print_malloced_items();
    r = open_brt(n, 0, 1, &brt, 1<<12, ct);  assert(r==0);
    //printf("%s:%d %d alloced\n", __FILE__, __LINE__, get_n_items_malloced()); print_malloced_items();
    r = brt_cursor(brt, &cursor);            assert(r==0);
    r = ybt_init(&kbt);                      assert(r==0);
    //printf("%s:%d %d alloced\n", __FILE__, __LINE__, get_n_items_malloced()); print_malloced_items();
    r = ybt_init(&vbt);                      assert(r==0);
    //printf("%s:%d %d alloced\n", __FILE__, __LINE__, get_n_items_malloced()); print_malloced_items();
    r = brt_c_get(cursor, &kbt, &vbt, DB_LAST);
    //printf("%s:%d %d alloced\n", __FILE__, __LINE__, get_n_items_malloced()); print_malloced_items();
    assert(r==DB_NOTFOUND);
    r = brt_c_get(cursor, &kbt, &vbt, DB_FIRST);
    //printf("%s:%d %d alloced\n", __FILE__, __LINE__, get_n_items_malloced()); print_malloced_items();
    assert(r==DB_NOTFOUND);
    r = close_brt(brt);
    //printf("%s:%d %d alloced\n", __FILE__, __LINE__, get_n_items_malloced()); print_malloced_items();
    r = cachetable_close(ct); assert(r==0);
    //printf("%s:%d %d alloced\n", __FILE__, __LINE__, get_n_items_malloced()); print_malloced_items();
    memory_check_all_free();
}

void test_cursor_next (void) {
    const char *n="testbrt.brt";
    CACHETABLE ct;
    BRT brt;
    BRT_CURSOR cursor;
    int r;
    DBT kbt, vbt;

    unlink(n);
    memory_check_all_free();
    r = brt_create_cachetable(&ct, 0);       assert(r==0);
    //printf("%s:%d %d alloced\n", __FILE__, __LINE__, get_n_items_malloced()); print_malloced_items();
    r = open_brt(n, 0, 1, &brt, 1<<12, ct);  assert(r==0);
    //printf("%s:%d %d alloced\n", __FILE__, __LINE__, get_n_items_malloced()); print_malloced_items();
    r = brt_insert(brt, "hello", 6, "there", 6);
    r = brt_insert(brt, "byebye", 7, "byenow", 7);
    printf("%s:%d calling brt_cursor(...)\n", __FILE__, __LINE__);
    r = brt_cursor(brt, &cursor);            assert(r==0);
    r = ybt_init(&kbt);                      assert(r==0);
    //printf("%s:%d %d alloced\n", __FILE__, __LINE__, get_n_items_malloced()); print_malloced_items();
    r = ybt_init(&vbt);                      assert(r==0);
    //printf("%s:%d %d alloced\n", __FILE__, __LINE__, get_n_items_malloced()); print_malloced_items();

    printf("%s:%d calling brt_c_get(...)\n", __FILE__, __LINE__);
    r = brt_c_get(cursor, &kbt, &vbt, DB_NEXT);
    printf("%s:%d called brt_c_get(...)\n", __FILE__, __LINE__);
    //printf("%s:%d %d alloced\n", __FILE__, __LINE__, get_n_items_malloced()); print_malloced_items();
    assert(r==0);
    assert(kbt.size==7);
    assert(memcmp(kbt.data, "byebye", 7)==0);
    assert(vbt.size==7);
    assert(memcmp(vbt.data, "byenow", 7)==0);

    r = brt_c_get(cursor, &kbt, &vbt, DB_NEXT);
    assert(r==0);
    assert(kbt.size==6);
    assert(memcmp(kbt.data, "hello", 6)==0);
    assert(vbt.size==6);
    assert(memcmp(vbt.data, "there", 6)==0);

    r = brt_c_get(cursor, &kbt, &vbt, DB_NEXT);
    assert(r==DB_NOTFOUND);

    r = close_brt(brt);
    //printf("%s:%d %d alloced\n", __FILE__, __LINE__, get_n_items_malloced()); print_malloced_items();
    r = cachetable_close(ct); assert(r==0);
    //printf("%s:%d %d alloced\n", __FILE__, __LINE__, get_n_items_malloced()); print_malloced_items();
    memory_check_all_free();
    
}

static void brt_blackbox_test (void) {
    test_cursor_next();                   memory_check_all_free();
    test_multiple_dbs_many();             memory_check_all_free();
    test_cursor_last_empty();             memory_check_all_free();
    test_multiple_brts_one_db_one_file(); memory_check_all_free();
    test_dump_empty_db();                 memory_check_all_free();
    test_read_what_was_written();
    test_named_db();
    memory_check_all_free();
    test_multiple_dbs();
    memory_check_all_free();
    printf("test0 A\n");
    test0();
    printf("test0 B\n");
    test0(); /* Make sure it works twice. */
    printf("test1\n");
    test1();
    printf("test2 checking memory\n");
    test2(1);
    printf("test2 faster\n");
    test2(0);
    printf("test5\n");
    test5();
    printf("test_multiple_files\n");
    test_multiple_files();
    printf("test3 slow\n");
    memory_check=0;
    test3(2048, 1<<15, 1);
    printf("test4 slow\n");
    test4(2048, 1<<15, 1);
    printf("test3 fast\n");

    pma_show_stats();

    test3(1<<15, 1024, 1);
    test4(1<<15, 1024, 1);
    printf("test3 fast\n");

    test3(1<<18, 1<<20, 0);
    test4(1<<18, 1<<20, 0);

//    test3(1<<19, 1<<20, 0);
//    test4(1<<19, 1<<20, 0);

//    test3(1<<20, 1<<20, 0);
//    test4(1<<20, 1<<20, 0);

//    test3(1<<20, 1<<21, 0);
//    test4(1<<20, 1<<21, 0);

//    test3(1<<20, 1<<22, 0);
//    test4(1<<20, 1<<22, 0);

}

int main (int argc __attribute__((__unused__)), char *argv[] __attribute__((__unused__))) {
    brt_blackbox_test();
    printf("ok\n");
    return 0;
}
