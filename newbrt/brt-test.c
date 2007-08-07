#include "brt.h"
#include "key.h"

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
    r = open_brt(fname, 0, 1, &t, 1024, ct, default_compare_fun);
    assert(r==0);
    printf("%s:%d test0\n", __FILE__, __LINE__);
    printf("%s:%d n_items_malloced=%lld\n", __FILE__, __LINE__, n_items_malloced);
    r = close_brt(t);     assert(r==0);
    printf("%s:%d n_items_malloced=%lld\n", __FILE__, __LINE__, n_items_malloced);    assert(r==0);
    r = cachetable_close(&ct);
    assert(r==0);
    memory_check_all_free();
}

static void test1 (void) {
    BRT t;
    int r;
    CACHETABLE ct;
    char fname[]="testbrt.brt";
    DBT k,v;
    memory_check=1;
    memory_check_all_free();
    r = brt_create_cachetable(&ct, 0);
    assert(r==0);
    unlink(fname);
    r = open_brt(fname, 0, 1, &t, 1024, ct, default_compare_fun);
    assert(r==0);
    brt_insert(t, fill_dbt(&k, "hello", 6), fill_dbt(&v, "there", 6), 0);
    {
	r = brt_lookup(t, fill_dbt(&k, "hello", 6), init_dbt(&v), 0);
	assert(r==0);
	assert(strcmp(v.data, "there")==0);
	assert(v.size==6);
    }
    r = close_brt(t);              assert(r==0);
    r = cachetable_close(&ct);     assert(r==0);
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
    r = open_brt(fname, 0, 1, &t, 1024, ct, default_compare_fun);
    printf("%s:%d did setup\n", __FILE__, __LINE__);
    assert(r==0);
    for (i=0; i<2048; i++) {
	DBT k,v;
	char key[100],val[100];
	snprintf(key,100,"hello%d",i);
	snprintf(val,100,"there%d",i);
	brt_insert(t, fill_dbt(&k, key, 1+strlen(key)), fill_dbt(&v, val, 1+strlen(val)), 0);
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
    r = cachetable_close(&ct);     assert(r==0);
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
    r = open_brt(fname, 0, 1, &t, nodesize, ct, default_compare_fun);
    assert(r==0);
    for (i=0; i<count; i++) {
	char key[100],val[100];
	DBT k,v;
	snprintf(key,100,"hello%d",i);
	snprintf(val,100,"there%d",i);
	brt_insert(t, fill_dbt(&k, key, 1+strlen(key)), fill_dbt(&v, val, 1+strlen(val)), 0);
    }
    r = close_brt(t);              assert(r==0);
    r = cachetable_close(&ct);     assert(r==0);
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
    r = open_brt(fname, 0, 1, &t, nodesize, ct, default_compare_fun); assert(r==0);
    for (i=0; i<count; i++) {
	char key[100],val[100];
	int rv = random();
	DBT k,v;
	snprintf(key,100,"hello%d",rv);
	snprintf(val,100,"there%d",i);
	brt_insert(t, fill_dbt(&k, key, 1+strlen(key)), fill_dbt(&v, val, 1+strlen(val)), 0);
    }
    r = close_brt(t);              assert(r==0);
    r = cachetable_close(&ct);     assert(r==0);
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
    r = open_brt(fname, 0, 1, &t, 1<<12, ct, default_compare_fun);   assert(r==0);
    for (i=0; i<limit/2; i++) {
	char key[100],val[100];
	int rk = random()%limit;
	int rv = random();
	if (i%1000==0) printf("w"); fflush(stdout);
	values[rk] = rv;
	snprintf(key, 100, "key%d", rk);
	snprintf(val, 100, "val%d", rv);
	DBT k,v;
	brt_insert(t, fill_dbt(&k, key, 1+strlen(key)), fill_dbt(&v, val, 1+strlen(val)), 0);
    }
    printf("\n");
    for (i=0; i<limit/2; i++) {
	int rk = random()%limit;
	if (values[rk]>=0) {
	    char key[100], valexpected[100];
	    DBT k,v;
	    if (i%1000==0) printf("r"); fflush(stdout);
	    snprintf(key, 100, "key%d", rk);
	    snprintf(valexpected, 100, "val%d", values[rk]);
	    r = brt_lookup(t, fill_dbt(&k, key, 1+strlen(key)), init_dbt(&v), 0);
	    assert(r==0);
	    assert(v.size==(1+strlen(valexpected)));
	    assert(memcmp(v.data,valexpected,v.size)==0);
	}
    }
    printf("\n");
    toku_free(values);
    r = close_brt(t);          assert(r==0);
    r = cachetable_close(&ct); assert(r==0);
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
    r = open_brt(fname, 0, 1, &t, 1024, ct, default_compare_fun);
    assert(r==0);
    dump_brt(t);
    r = close_brt(t);          assert(r==0);
    r = cachetable_close(&ct); assert(r==0);
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
    r = open_brt(n0, 0, 1, &t0, size, ct, default_compare_fun); assert(r==0);
    r = open_brt(n1, 0, 1, &t1, size, ct, default_compare_fun); assert(r==0);
    for (i=0; i<10000; i++) {
	char key[100],val[100];
	DBT k,v;
	snprintf(key, 100, "key%d", i);
	snprintf(val, 100, "val%d", i);
	brt_insert(t0, fill_dbt(&k, key, 1+strlen(key)), fill_dbt(&v, val, 1+strlen(val)), 0);
	snprintf(val, 100, "Val%d", i);
	brt_insert(t1, fill_dbt(&k, key, 1+strlen(key)), fill_dbt(&v, val, 1+strlen(val)), 0);
    }
    //verify_brt(t0);
    //dump_brt(t0);
    //dump_brt(t1);
    verify_brt(t0);
    verify_brt(t1);

    r = close_brt(t0); assert(r==0);
    r = close_brt(t1); assert(r==0);
    r = cachetable_close(&ct); assert(r==0);
    memory_check_all_free();

    /* Now see if the data is all there. */
    r = brt_create_cachetable(&ct, 0);      assert(r==0);
    r = open_brt(n0, 0, 0, &t0, 1<<12, ct, default_compare_fun);
    printf("%s:%d r=%d\n", __FILE__, __LINE__,r);
    assert(r==0);
    r = open_brt(n1, 0, 0, &t1, 1<<12, ct, default_compare_fun); assert(r==0);
    
    for (i=0; i<10000; i++) {
	char key[100],val[100];
	DBT k,actual;
	snprintf(key, 100, "key%d", i);
	snprintf(val, 100, "val%d", i);
	r=brt_lookup(t0, fill_dbt(&k, key, 1+strlen(key)), init_dbt(&actual), 0);
	assert(r==0);
	assert(strcmp(val,actual.data)==0);
	assert(actual.size==1+strlen(val));
	snprintf(val, 100, "Val%d", i);
	r=brt_lookup(t1, fill_dbt(&k, key, 1+strlen(key)), init_dbt(&actual), 0);
	assert(r==0);
	assert(strcmp(val,actual.data)==0);
	assert(actual.size==1+strlen(val));
    }

    r = close_brt(t0); assert(r==0);
    r = close_brt(t1); assert(r==0);
    r = cachetable_close(&ct); assert(r==0);
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
    DBT k,v;

    printf("test_named_db\n");
    unlink(n0);
    unlink(n1);
    memory_check_all_free();
    r = brt_create_cachetable(&ct, 0);           assert(r==0);
    r = open_brt(n0, "db1", 1, &t0, 1<<12, ct, default_compare_fun); assert(r==0);


    brt_insert(t0, fill_dbt(&k, "good", 5), fill_dbt(&v, "day", 4), 0); assert(r==0);

    r = close_brt(t0); assert(r==0);
    r = cachetable_close(&ct); assert(r==0);
    memory_check_all_free();

    memory_check_all_free();
    r = brt_create_cachetable(&ct, 0);           assert(r==0);
    r = open_brt(n0, "db1", 0, &t0, 1<<12, ct, default_compare_fun); assert(r==0);

    {
	r = brt_lookup(t0, fill_dbt(&k, "good", 5), init_dbt(&v), 0);
	assert(r==0);
	assert(v.size==4);
	assert(strcmp(v.data,"day")==0);
    }
    
    r = close_brt(t0); assert(r==0);
    r = cachetable_close(&ct); assert(r==0);
    memory_check_all_free();
}

static void test_multiple_dbs (void) {
    const char *n0 = "test0.brt";
    const char *n1 = "test1.brt";
    CACHETABLE ct;
    BRT t0,t1;
    int r;
    DBT k,v;
    printf("test_multiple_dbs: ");
    unlink(n0);
    unlink(n1);
    memory_check_all_free();
    r = brt_create_cachetable(&ct, 0);           assert(r==0);
    r = open_brt(n0, "db1", 1, &t0, 1<<12, ct, default_compare_fun); assert(r==0);
    r = open_brt(n1, "db2", 1, &t1, 1<<12, ct, default_compare_fun); assert(r==0);

    brt_insert(t0, fill_dbt(&k, "good", 5), fill_dbt(&v, "grief", 6), 0); assert(r==0);
    brt_insert(t1, fill_dbt(&k, "bad",  4), fill_dbt(&v, "night", 6), 0); assert(r==0);

    r = close_brt(t0); assert(r==0);
    r = close_brt(t1); assert(r==0);
    r = cachetable_close(&ct); assert(r==0);

    memory_check_all_free();

    r = brt_create_cachetable(&ct, 0);           assert(r==0);
    r = open_brt(n0, "db1", 0, &t0, 1<<12, ct, default_compare_fun); assert(r==0);
    r = open_brt(n1, "db2", 0, &t1, 1<<12, ct, default_compare_fun); assert(r==0);

    {
	r = brt_lookup(t0, fill_dbt(&k, "good", 5), init_dbt(&v), 0);
	assert(r==0);
	assert(v.size==6);
	assert(strcmp(v.data,"grief")==0);

	r = brt_lookup(t1, fill_dbt(&k, "good", 5), init_dbt(&v), 0);
	assert(r!=0);

	r = brt_lookup(t0, fill_dbt(&k, "bad", 4), init_dbt(&v), 0);
	assert(r!=0);

	r = brt_lookup(t1, fill_dbt(&k, "bad", 4), init_dbt(&v), 0);
	assert(r==0);
	assert(v.size==6);
	assert(strcmp(v.data,"night")==0);
    }
    
    r = close_brt(t0); assert(r==0);
    r = close_brt(t1); assert(r==0);
    r = cachetable_close(&ct); assert(r==0);

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
	r = open_brt(name, dbname, 1, &trees[i], 1<<12, ct, default_compare_fun);
	assert(r==0);
    }
    for (i=0; i<MANYN; i++) {
	char k[20], v[20];
	DBT kdbt,vdbt;
	snprintf(k, 20, "key%d", i);
	snprintf(v, 20, "val%d", i);
	brt_insert(trees[i], fill_dbt(&kdbt, k, strlen(k)+1), fill_dbt(&vdbt, v, strlen(v)+1), 0);
    }
    for (i=0; i<MANYN; i++) {
	r = close_brt(trees[i]); assert(r==0);
    }
    r = cachetable_close(&ct);    assert(r==0);
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
	r = open_brt(name, 0, (i==0), &trees[i], 1<<12, ct, default_compare_fun);
	assert(r==0);
    }
    for (i=0; i<MANYN; i++) {
	char k[20], v[20];
	DBT kb, vb;
	snprintf(k, 20, "key%d", i);
	snprintf(v, 20, "val%d", i);
	brt_insert(trees[i], fill_dbt(&kb, k, strlen(k)+1), fill_dbt(&vb, v, strlen(v)+1), 0);
    }
    for (i=0; i<MANYN; i++) {
	char k[20],vexpect[20];
	DBT kb, vb;
	snprintf(k, 20, "key%d", i);
	snprintf(vexpect, 20, "val%d", i);
	r=brt_lookup(trees[0], fill_dbt(&kb, k, strlen(k)+1), init_dbt(&vb), 0);
	assert(r==0);
	assert(vb.size==1+strlen(vexpect));
	assert(strcmp(vb.data, vexpect)==0);
    }
    for (i=0; i<MANYN; i++) {
	r=close_brt(trees[i]); assert(r==0);
    }
    r = cachetable_close(&ct); assert(r==0);
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
    DBT k,v;

    printf("test_read_what_was_written(): "); fflush(stdout);

    unlink(n);
    memory_check_all_free();

    r = brt_create_cachetable(&ct, 0);       assert(r==0);
    r = open_brt(n, 0, 1, &brt, 1<<12, ct, default_compare_fun);  assert(r==0);
    r = close_brt(brt); assert(r==0);
    r = cachetable_close(&ct); assert(r==0);

    memory_check_all_free();

    /* Now see if we can read an empty tree in. */
    r = brt_create_cachetable(&ct, 0);       assert(r==0);
    r = open_brt(n, 0, 0, &brt, 1<<12, ct, default_compare_fun);  assert(r==0);

    /* See if we can put something in it. */
    brt_insert(brt, fill_dbt(&k, "hello", 6), fill_dbt(&v, "there", 6), 0);

    r = close_brt(brt); assert(r==0);
    r = cachetable_close(&ct); assert(r==0);
    
    memory_check_all_free();

    /* Now see if we can read it in and get the value. */
    r = brt_create_cachetable(&ct, 0);       assert(r==0);
    r = open_brt(n, 0, 0, &brt, 1<<12, ct, default_compare_fun); assert(r==0);

    {
	r = brt_lookup(brt, fill_dbt(&k, "hello", 6), init_dbt(&v), 0);
	assert(r==0);
	assert(v.size==6);
	assert(strcmp(v.data,"there")==0);
    }

    assert(verify_brt(brt)==0);

    /* Now put a bunch (VALS) of things in. */
    {
	int i;
	for (i=0; i<NVALS; i++) {
	    char key[100],val[100];
	    DBT k,v;
	    snprintf(key, 100, "key%d", i);
	    snprintf(val, 100, "val%d", i);
	    if (i<600) {
		int verify_result=verify_brt(brt);;
		assert(verify_result==0);
	    }
	    brt_insert(brt, fill_dbt(&k, key, strlen(key)+1), fill_dbt(&v, val, strlen(val)+1), 0);
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
			snprintf(key, 100, "key%d", j);
			snprintf(expectedval, 100, "val%d", j);
			r=brt_lookup(brt, fill_dbt(&k, key, strlen(key)+1), init_dbt(&v), 0);
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
	    snprintf(key, 100, "key%d", i);
	    snprintf(expectedval, 100, "val%d", i);
	    r=brt_lookup(brt, fill_dbt(&k, key, strlen(key)+1), init_dbt(&v), 0);
	    if (r!=0) printf("%s:%d r=%d on key=%s\n", __FILE__, __LINE__, r, key);
	    assert(r==0);
	    
	}
    }

    r = close_brt(brt); assert(r==0);
    printf("%s:%d About to close %p\n", __FILE__, __LINE__, ct);
    r = cachetable_close(&ct); assert(r==0);
    
    memory_check_all_free();
    
    r = brt_create_cachetable(&ct, 0);       assert(r==0);
    r = open_brt(n, 0, 0, &brt, 1<<12, ct, default_compare_fun); assert(r==0);

    {
	r = brt_lookup(brt, fill_dbt(&k, "hello", 6), init_dbt(&v), 0);
	assert(r==0);
	assert(v.size==6);
	assert(strcmp(v.data,"there")==0);
    }
    {
	int i;
	for (i=0; i<NVALS; i++) {
	    char key[100],expectedval[100];
	    snprintf(key, 100, "key%d", i);
	    snprintf(expectedval, 100, "val%d", i);
	    r=brt_lookup(brt, fill_dbt(&k, key, strlen(key)+1), init_dbt(&v), 0);
	    if (r!=0) printf("%s:%d r=%d on key=%s\n", __FILE__, __LINE__, r, key);
	    assert(r==0);
	    
	}
    }
    
    r = close_brt(brt); assert(r==0);
    r = cachetable_close(&ct); assert(r==0);
    
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
    r = open_brt(n, 0, 1, &brt, 1<<12, ct, default_compare_fun);  assert(r==0);
    //printf("%s:%d %d alloced\n", __FILE__, __LINE__, get_n_items_malloced()); print_malloced_items();
    r = brt_cursor(brt, &cursor);            assert(r==0);
    init_dbt(&kbt);
    //printf("%s:%d %d alloced\n", __FILE__, __LINE__, get_n_items_malloced()); print_malloced_items();
    init_dbt(&vbt);
    //printf("%s:%d %d alloced\n", __FILE__, __LINE__, get_n_items_malloced()); print_malloced_items();
    r = brt_c_get(cursor, &kbt, &vbt, DB_LAST);
    //printf("%s:%d %d alloced\n", __FILE__, __LINE__, get_n_items_malloced()); print_malloced_items();
    assert(r==DB_NOTFOUND);
    r = brt_c_get(cursor, &kbt, &vbt, DB_FIRST);
    //printf("%s:%d %d alloced\n", __FILE__, __LINE__, get_n_items_malloced()); print_malloced_items();
    assert(r==DB_NOTFOUND);
    r = close_brt(brt);
    //printf("%s:%d %d alloced\n", __FILE__, __LINE__, get_n_items_malloced()); print_malloced_items();
    r = cachetable_close(&ct); assert(r==0);
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
    r = open_brt(n, 0, 1, &brt, 1<<12, ct, default_compare_fun);  assert(r==0);
    //printf("%s:%d %d alloced\n", __FILE__, __LINE__, get_n_items_malloced()); print_malloced_items();
    r = brt_insert(brt, fill_dbt(&kbt, "hello", 6), fill_dbt(&vbt, "there", 6), 0);
    r = brt_insert(brt, fill_dbt(&kbt, "byebye", 7), fill_dbt(&vbt, "byenow", 7), 0);
    printf("%s:%d calling brt_cursor(...)\n", __FILE__, __LINE__);
    r = brt_cursor(brt, &cursor);            assert(r==0);
    init_dbt(&kbt);
    //printf("%s:%d %d alloced\n", __FILE__, __LINE__, get_n_items_malloced()); print_malloced_items();
    init_dbt(&vbt);
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
    r = cachetable_close(&ct); assert(r==0);
    //printf("%s:%d %d alloced\n", __FILE__, __LINE__, get_n_items_malloced()); print_malloced_items();
    memory_check_all_free();
    
}

static int nonce;
DB nonce_db;

DBT *fill_b(DBT *x, unsigned char *key, unsigned int keylen) {
    fill_dbt(x, key, keylen);
    x->app_private = &nonce;
    return x;
}

int wrong_compare_fun(DB *db, const DBT *a, const DBT *b) {
    unsigned int i;
    unsigned char *ad=a->data;
    unsigned char *bd=b->data;
    unsigned int siz=a->size;
    assert(a->size==b->size);
    assert(a->app_private == &nonce); // a must have the nonce in it, but I don't care if b does.
    assert(db==&nonce_db); // make sure the db was passed  down correctly
    for (i=0; i<siz; i++) {
	if (ad[siz-1-i]<bd[siz-1-i]) return -1;
	if (ad[siz-1-i]>bd[siz-1-i]) return +1;
    }
    return 0;

}

static void test_wrongendian_compare (int wrong_p, unsigned int N) {
    const char *n="testbrt.brt";
    CACHETABLE ct;
    BRT brt;
    BRT_CURSOR cursor;
    int r;
    DBT kbt, vbt;
    unsigned int i;

    unlink(n);
    memory_check_all_free();

    {
	char a[4]={0,1,0,0};
	char b[4]={1,0,0,0};
	DBT at, bt;
	assert(wrong_compare_fun(&nonce_db, fill_dbt_ap(&at, a, 4, &nonce), fill_dbt(&bt, b, 4))>0);
	assert(wrong_compare_fun(&nonce_db, fill_dbt_ap(&at, b, 4, &nonce), fill_dbt(&bt, a, 4))<0);
    }

    r = brt_create_cachetable(&ct, 0);       assert(r==0);
    printf("%s:%d WRONG=%d\n", __FILE__, __LINE__, wrong_p);

    if (0) {
    r = open_brt(n, 0, 1, &brt, 1<<20, ct, wrong_p ? wrong_compare_fun : default_compare_fun);  assert(r==0);
    for (i=1; i<257; i+=255) {
	unsigned char a[4],b[4];
	b[3] = a[0] = i&255;
	b[2] = a[1] = (i>>8)&255;
	b[1] = a[2] = (i>>16)&255;
	b[0] = a[3] = (i>>24)&255;
	fill_b(&kbt, a, sizeof(a));
	fill_dbt(&vbt, b, sizeof(b));
	printf("%s:%d insert: %02x%02x%02x%02x -> %02x%02x%02x%02x\n", __FILE__, __LINE__,
	       ((char*)kbt.data)[0], ((char*)kbt.data)[1], ((char*)kbt.data)[2], ((char*)kbt.data)[3],
	       ((char*)vbt.data)[0], ((char*)vbt.data)[1], ((char*)vbt.data)[2], ((char*)vbt.data)[3]);
	r = brt_insert(brt, &kbt, &vbt, &nonce_db);
	assert(r==0);
    }
    r = brt_cursor(brt, &cursor);            assert(r==0);

    for (i=0; i<2; i++) {
	init_dbt(&kbt); init_dbt(&vbt);
	r = brt_c_get(cursor, &kbt, &vbt, DB_NEXT);
	assert(r==0);
	assert(kbt.size==4 && vbt.size==4);
	printf("%s:%d %02x%02x%02x%02x -> %02x%02x%02x%02x\n", __FILE__, __LINE__,
	       ((char*)kbt.data)[0], ((char*)kbt.data)[1], ((char*)kbt.data)[2], ((char*)kbt.data)[3],
	       ((char*)vbt.data)[0], ((char*)vbt.data)[1], ((char*)vbt.data)[2], ((char*)vbt.data)[3]);
    }

    
    r = close_brt(brt);
    }

    if (1) {
    r = open_brt(n, 0, 1, &brt, 1<<20, ct, wrong_p ? wrong_compare_fun : default_compare_fun);  assert(r==0);
    
    for (i=0; i<N; i++) {
	unsigned char a[4],b[4];
	b[3] = a[0] = i&255;
	b[2] = a[1] = (i>>8)&255;
	b[1] = a[2] = (i>>16)&255;
	b[0] = a[3] = (i>>24)&255;
	fill_b(&kbt, a, sizeof(a));
	fill_dbt(&vbt, b, sizeof(b));
	if (0) printf("%s:%d insert: %02x%02x%02x%02x -> %02x%02x%02x%02x\n", __FILE__, __LINE__,
	       ((unsigned char*)kbt.data)[0], ((unsigned char*)kbt.data)[1], ((unsigned char*)kbt.data)[2], ((unsigned char*)kbt.data)[3],
	       ((unsigned char*)vbt.data)[0], ((unsigned char*)vbt.data)[1], ((unsigned char*)vbt.data)[2], ((unsigned char*)vbt.data)[3]);
	r = brt_insert(brt, &kbt, &vbt, &nonce_db);
	assert(r==0);
    }
    r = brt_cursor(brt, &cursor);            assert(r==0);

    int prev=-1;
    for (i=0; i<N; i++) {
	int this;
	init_dbt(&kbt); init_dbt(&vbt);
	r = brt_c_get(cursor, &kbt, &vbt, DB_NEXT);
	assert(r==0);
	assert(kbt.size==4 && vbt.size==4);
	if (0) printf("%s:%d %02x%02x%02x%02x -> %02x%02x%02x%02x\n", __FILE__, __LINE__,
		      ((unsigned char*)kbt.data)[0], ((unsigned char*)kbt.data)[1], ((unsigned char*)kbt.data)[2], ((unsigned char*)kbt.data)[3],
		      ((unsigned char*)vbt.data)[0], ((unsigned char*)vbt.data)[1], ((unsigned char*)vbt.data)[2], ((unsigned char*)vbt.data)[3]);
	this= ( (((unsigned char*)kbt.data)[3] << 24) +
		(((unsigned char*)kbt.data)[2] << 16) +
		(((unsigned char*)kbt.data)[1] <<  8) +
		(((unsigned char*)kbt.data)[0] <<  0));
	assert(prev<this);
	prev=this;
	assert(this==(int)i);
	    
    }

  
    r = close_brt(brt);
    }
    r = cachetable_close(&ct); assert(r==0);
    memory_check_all_free();
}

static void brt_blackbox_test (void) {
    test_wrongendian_compare(0, 2);          memory_check_all_free();
    test_wrongendian_compare(1, 2);          memory_check_all_free();
    test_wrongendian_compare(1, 257);        memory_check_all_free();
    test_wrongendian_compare(1, 1000);        memory_check_all_free();
    test_read_what_was_written();         memory_check_all_free(); printf("did read_what_was_written\n");
    test_cursor_next();                   memory_check_all_free();
    test_multiple_dbs_many();             memory_check_all_free();
    test_cursor_last_empty();             memory_check_all_free();
    test_multiple_brts_one_db_one_file(); memory_check_all_free();
    test_dump_empty_db();                 memory_check_all_free();
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

    // Once upon a time srandom(8) caused this test to fail.
    srandom(8); test4(2048, 1<<15, 1);



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
    malloc_cleanup();
    printf("ok\n");
    return 0;
}
