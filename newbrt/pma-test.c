/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include "brt-internal.h"
#include "key.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <arpa/inet.h>
#include "list.h"
#include "pma-internal.h"

TOKUTXN const null_txn = 0;
DB * const null_db = 0;
const DISKOFF null_diskoff = -1;
const FILENUM null_filenum = {0};

#define NULL_ARGS null_txn, null_diskoff

static void test_make_space_at (void) {
    PMA pma;
    char *key;
    int r;
    struct kv_pair *key_A, *key_B;

    key = "A";
    key_A = kv_pair_malloc(key, strlen(key)+1, 0, 0);
    key = "B";
    key_B = kv_pair_malloc(key, strlen(key)+1, 0, 0);

    r=toku_pma_create(&pma, toku_default_compare_fun, null_db, null_filenum, 0);
    assert(r==0);
    assert(toku_pma_n_entries(pma)==0);
    r=toku_pmainternal_make_space_at(pma, 2);
    assert(toku_pma_index_limit(pma)==4);
    assert((unsigned long)pma->pairs[toku_pma_index_limit(pma)]==0xdeadbeefL);
    toku_print_pma(pma);

    pma->pairs[2] = key_A;
    pma->n_pairs_present++;
    r=toku_pmainternal_make_space_at(pma,2);
    printf("Requested space at 2, got space at %d\n", r);
    toku_print_pma(pma);    
    assert(pma->pairs[r]==0);
    assert((unsigned long)pma->pairs[toku_pma_index_limit(pma)]==0xdeadbeefL);

    assert(toku_pma_index_limit(pma)==4);
    pma->pairs[0] = key_A;
    pma->pairs[1] = key_B;
    pma->pairs[2] = 0;
    pma->pairs[3] = 0;
    pma->n_pairs_present=2;
    toku_print_pma(pma);    
    r=toku_pmainternal_make_space_at(pma,0);
    printf("Requested space at 0, got space at %d\n", r);
    toku_print_pma(pma);
    assert((unsigned long)pma->pairs[toku_pma_index_limit(pma)]==0xdeadbeefL); // make sure it doesn't go off the end.

    assert(toku_pma_index_limit(pma)==8);
    pma->pairs[0] = key_A; 
    pma->pairs[1] = 0;
    pma->pairs[2] = 0;
    pma->pairs[3] = 0;
    pma->pairs[4] = key_B;
    pma->pairs[5] = 0;
    pma->pairs[6] = 0;
    pma->pairs[7] = 0;
    pma->n_pairs_present=2;
    toku_print_pma(pma);
    r=toku_pmainternal_make_space_at(pma,5);
    toku_print_pma(pma);
    printf("r=%d\n", r);
    {
	int i;
	for (i=0; i<toku_pma_index_limit(pma); i++) {
	    if (pma->pairs[i]) {
		assert(i<r);
            pma->pairs[i] = 0;
	    }
	}
    }
    pma->n_pairs_present = 0;
    r=toku_pma_free(&pma); assert(r==0);
    assert(pma==0);
    kv_pair_free(key_A);
    kv_pair_free(key_B);
}

static void test_pma_find (void) {
    PMA pma;
    int i;
    int r;
    const int N = 16;
    DBT k;
    MALLOC(pma);
    MALLOC_N(N,pma->pairs);
    // All that is needed to test pma_find is N and pairs.
    pma->N = N;
    for (i=0; i<N; i++) pma->pairs[i]=0;
    assert(toku_pma_index_limit(pma)==N);
    pma->compare_fun = toku_default_compare_fun;
    r=toku_pmainternal_find(pma, toku_fill_dbt(&k, "hello", 5));
    assert(r==0);

    pma->pairs[5] = kv_pair_malloc("hello", 5, 0, 0);
    assert(toku_pma_index_limit(pma)==N);
    r=toku_pmainternal_find(pma, toku_fill_dbt(&k, "hello", 5));
    assert(toku_pma_index_limit(pma)==N);
    assert(r==5);
    r=toku_pmainternal_find(pma, toku_fill_dbt(&k, "there", 5));
    assert(r==6);
    r=toku_pmainternal_find(pma, toku_fill_dbt(&k, "aaa", 3));
    assert(r==0);

    pma->pairs[N-1] = kv_pair_malloc("there", 5, 0, 0);
    r=toku_pmainternal_find(pma, toku_fill_dbt(&k, "hello", 5));
    assert(r==5);
    r=toku_pmainternal_find(pma, toku_fill_dbt(&k, "there", 5));
    assert(r==N-1);
    r=toku_pmainternal_find(pma, toku_fill_dbt(&k, "aaa", 3));
    assert(r==0);
    r=toku_pmainternal_find(pma, toku_fill_dbt(&k, "hellob", 6));
    assert(r==6);
    r=toku_pmainternal_find(pma, toku_fill_dbt(&k, "zzz", 3));
    assert(r==N);

    for (i=0; i<N; i++)
        if (pma->pairs[i])
            kv_pair_free(pma->pairs[i]);
    toku_free(pma->pairs);
    toku_free(pma);
}

void test_smooth_region_N (int N) {
    struct kv_pair *pairs[N];
    struct kv_pair *strings[N];
    char string[N];
    int i;
    int len;
    if (N<10) len=1;
    else if (N<100) len=2;
    else len=8;

    for (i=0; i<N; i++) {
	snprintf(string, 10, "%0*d", len, i);
	strings[i] = kv_pair_malloc(string, len+1, 0, 0);
    }

    assert(N<30);
    for (i=0; i<(1<<N)-1; i++) {
	int insertat;
	for (insertat=0; insertat<=N; insertat++) {
	    int j;
	    int r;
	    for (j=0; j<N; j++) {
		if ((1<<j)&i) {
		    pairs[j] = strings[j];
		} else {
		    pairs[j] = 0;
		}
	    }
	    toku_pmainternal_printpairs(pairs, N); printf(" at %d becomes f", insertat);
	    r = toku_pmainternal_smooth_region(pairs, N, insertat, 0, 0);
	    toku_pmainternal_printpairs(pairs, N); printf(" at %d\n", r);
	    assert(0<=r); assert(r<N);
	    assert(pairs[r]==0);
	    /* Now verify that things are in the right place:
	     *  everything before r should be smaller than keys[insertat].
	     *  everything after is bigger.
	     *  Also, make sure everything appeared. */
	    {
		int cleari = i;
		for (j=0; j<N; j++) {
		    if (pairs[j]) {
			int whichkey = atoi(pairs[j]->key);
			assert(cleari&(1<<whichkey));
			cleari &= ~(1<<whichkey);
			if (whichkey<insertat) assert(j<r);
			else assert(j>r);
		    }
		}
		assert(cleari==0);
	    }
	}
    }
    for (i=0; i<N; i++) {
	kv_pair_free(strings[i]);
    }
}

void test_smooth_region6 (void) {
    enum {N=7};
    struct kv_pair *pairs[N];
    char *key;
    int i;

    for (i=0; i<N; i++)
        pairs[i] = 0;
    key = "A";
    pairs[0] = kv_pair_malloc(key, strlen(key)+1, 0, 0);
    key = "B";
    pairs[1] = kv_pair_malloc(key, strlen(key)+1, 0, 0);

    int r = toku_pmainternal_smooth_region(pairs, N, 2, 0, 0);
    printf("{ ");
    for (i=0; i<N; i++)
        printf("%s ", pairs[i] ? pairs[i]->key : "?");
    printf("} %d\n", r);

    for (i=0; i<7; i++)
        if (pairs[i])
            kv_pair_free(pairs[i]);
}
    

static void test_smooth_region (void) {
    test_smooth_region_N(4);
    test_smooth_region_N(5);
    test_smooth_region6();
}

static void test_calculate_parameters (void) {
    struct pma pma;
    pma.N=4; toku_pmainternal_calculate_parameters(&pma); assert(pma.uplgN==2); assert(pma.udt_step==0.5);
    pma.N=8; toku_pmainternal_calculate_parameters(&pma); assert(pma.uplgN==4); assert(pma.udt_step==0.5);
    
}

static void test_count_region (void) {
    const int N = 4;
    struct kv_pair *pairs[N];
    int i;
    char *key;
    
    for (i=0; i<N; i++)
        pairs[i] = 0;
    assert(toku_pmainternal_count_region(pairs,0,4)==0);
    assert(toku_pmainternal_count_region(pairs,2,4)==0);
    assert(toku_pmainternal_count_region(pairs,0,2)==0);
    key = "A";
    pairs[2] = kv_pair_malloc(key, strlen(key)+1, 0, 0);
    assert(toku_pmainternal_count_region(pairs,0,4)==1);
    assert(toku_pmainternal_count_region(pairs,2,4)==1);
    assert(toku_pmainternal_count_region(pairs,0,2)==0);
    assert(toku_pmainternal_count_region(pairs,2,2)==0);
    assert(toku_pmainternal_count_region(pairs,2,3)==1);
    key = "B";
    pairs[3] = kv_pair_malloc(key, strlen(key)+1, 0, 0);
    key = "a";
    pairs[0] = kv_pair_malloc(key, strlen(key)+1, 0, 0);
    assert(toku_pmainternal_count_region(pairs,0,4)==3);
    for (i=0; i<N; i++)
        if (pairs[i])
            kv_pair_free(pairs[i]);
}

// Add a kvpair into a expected sum and check to see if it matches the actual sum.
void add_fingerprint_and_check(u_int32_t rand4fingerprint, u_int32_t actual_fingerprint, u_int32_t *expect_fingerprint, const void *key, int klen, const void *data, int dlen) {
    *expect_fingerprint += rand4fingerprint*toku_calccrc32_kvpair(key, klen, data, dlen);
    assert(*expect_fingerprint==actual_fingerprint);
}


static void do_insert (PMA pma, const void *key, int keylen, const void *data, int datalen, u_int32_t rand4fingerprint, u_int32_t *sum, u_int32_t *expect_fingerprint) {
    DBT k,v;
    assert(*sum==*expect_fingerprint);
    int r = toku_pma_insert(pma, toku_fill_dbt(&k, key, keylen), toku_fill_dbt(&v, data, datalen), NULL_ARGS, rand4fingerprint, sum);
    assert(r==BRT_OK);
    add_fingerprint_and_check(rand4fingerprint, *sum, expect_fingerprint, key, keylen, data, datalen);
    toku_pma_verify_fingerprint(pma, rand4fingerprint, *sum);
}

static void do_delete (PMA pma, const void *key, int keylen, const void *data, int datalen, u_int32_t rand4fingerprint, u_int32_t *sum, u_int32_t *expect_fingerprint) {
    DBT k;
    assert(*sum==*expect_fingerprint);
    int r = toku_pma_delete(pma, toku_fill_dbt(&k, key, keylen), rand4fingerprint, sum, 0);
    assert(r==BRT_OK);
    add_fingerprint_and_check(-rand4fingerprint, *sum, expect_fingerprint, key, keylen, data, datalen); // negative rand4 means subtract.
    toku_pma_verify_fingerprint(pma, rand4fingerprint, *sum);
}

static void test_pma_random_pick (void) {
    PMA pma;
    int r = toku_pma_create(&pma, toku_default_compare_fun, null_db, null_filenum, 0);
    bytevec key,val;
    ITEMLEN keylen,vallen;
    DBT k;
    u_int32_t rand4fingerprint = random();
    u_int32_t sum = 0;
    u_int32_t expect_fingerprint = 0;

    assert(r==0);
    r = toku_pma_random_pick(pma, &key, &keylen, &val, &vallen);
    assert(r==DB_NOTFOUND);
    do_insert(pma, "hello", 6, "there", 6, rand4fingerprint, &sum, &expect_fingerprint);
    toku_pma_verify_fingerprint(pma, rand4fingerprint, sum);

    r = toku_pma_random_pick(pma, &key, &keylen, &val, &vallen);
    assert(r==0);
    assert(keylen==6); assert(vallen==6);
    assert(strcmp(key,"hello")==0);
    assert(strcmp(val,"there")==0);
    r = toku_pma_delete(pma, toku_fill_dbt(&k, "nothello", 9), rand4fingerprint, &sum, 0);
    assert(r==DB_NOTFOUND);
    assert(sum==expect_fingerprint); // didn't change because nothing was deleted.

    do_delete(pma, "hello", 6, "there", 6, rand4fingerprint, &sum, &expect_fingerprint);

    r = toku_pma_random_pick(pma, &key, &keylen, &val, &vallen);
    assert(r==DB_NOTFOUND);
    
    do_insert(pma, "hello", 6, "there", 6, rand4fingerprint, &sum, &expect_fingerprint);

    r = toku_pma_random_pick(pma, &key, &keylen, &val, &vallen);
    assert(r==0);
    assert(keylen==6); assert(vallen==6);
    assert(strcmp(key,"hello")==0);
    assert(strcmp(val,"there")==0);

    do_insert(pma, "aaa", 4, "athere", 7, rand4fingerprint, &sum, &expect_fingerprint);
    do_insert(pma, "aab", 4, "bthere", 7, rand4fingerprint, &sum, &expect_fingerprint);
    do_insert(pma, "aac", 4, "cthere", 7, rand4fingerprint, &sum, &expect_fingerprint);
    do_insert(pma, "aad", 4, "dthere", 7, rand4fingerprint, &sum, &expect_fingerprint);
    do_insert(pma, "aae", 4, "ethere", 7, rand4fingerprint, &sum, &expect_fingerprint);
    do_insert(pma, "aaf", 4, "fthere", 7, rand4fingerprint, &sum, &expect_fingerprint);
    do_insert(pma, "aag", 4, "gthere", 7, rand4fingerprint, &sum, &expect_fingerprint);
    toku_pma_verify_fingerprint(pma, rand4fingerprint, sum);
    do_delete(pma, "aaa", 4, "athere", 7, rand4fingerprint, &sum, &expect_fingerprint);
    do_delete(pma, "aab", 4, "bthere", 7, rand4fingerprint, &sum, &expect_fingerprint);
    do_delete(pma, "aac", 4, "cthere", 7, rand4fingerprint, &sum, &expect_fingerprint);
    do_delete(pma, "aad", 4, "dthere", 7, rand4fingerprint, &sum, &expect_fingerprint);
    do_delete(pma, "aae", 4, "ethere", 7, rand4fingerprint, &sum, &expect_fingerprint);
    /* don't delete aaf */
    do_delete(pma, "aag", 4, "gthere", 7, rand4fingerprint, &sum, &expect_fingerprint);
    do_delete(pma, "hello", 6, "there", 6, rand4fingerprint, &sum, &expect_fingerprint);
   
    r = toku_pma_random_pick(pma, &key, &keylen, &val, &vallen);
    assert(r==0);
    assert(keylen==4); assert(vallen==7);
    assert(strcmp(key,"aaf")==0);
    assert(strcmp(val,"fthere")==0);
    toku_pma_verify_fingerprint(pma, rand4fingerprint, sum);
    r=toku_pma_free(&pma); assert(r==0);
    assert(pma==0);
}

static void test_find_insert (void) {
    PMA pma;
    int r;
    DBT k,v;

    u_int32_t rand4fingerprint = random();
    u_int32_t sum = 0;
    u_int32_t expect_fingerprint = 0;


    toku_pma_create(&pma, toku_default_compare_fun, null_db, null_filenum, 0);
    r=toku_pma_lookup(pma, toku_fill_dbt(&k, "aaa", 3), &v);
    assert(r==DB_NOTFOUND);

    do_insert(pma, "aaa", 3, "aaadata", 7, rand4fingerprint, &sum, &expect_fingerprint);

    toku_init_dbt(&v);
    r=toku_pma_lookup(pma, toku_fill_dbt(&k, "aaa", 3), &v);
    assert(r==BRT_OK);
    assert(v.size==7);
    assert(toku_keycompare(v.data,v.size,"aaadata", 7)==0);
    //toku_free(v.data); v.data=0;

    do_insert(pma, "bbb", 4, "bbbdata", 8, rand4fingerprint, &sum, &expect_fingerprint);

    toku_init_dbt(&v);
    r=toku_pma_lookup(pma, toku_fill_dbt(&k, "aaa", 3), &v);
    assert(r==BRT_OK);
    assert(toku_keycompare(v.data,v.size,"aaadata", 7)==0);

    toku_init_dbt(&v);
    r=toku_pma_lookup(pma, toku_fill_dbt(&k, "bbb", 4), &v);
    assert(r==BRT_OK);
    assert(toku_keycompare(v.data,v.size,"bbbdata", 8)==0);

    assert((unsigned long)pma->pairs[toku_pma_index_limit(pma)]==0xdeadbeefL);
    
    do_insert(pma, "00000", 6, "d0", 3, rand4fingerprint, &sum, &expect_fingerprint);

    assert((unsigned long)pma->pairs[toku_pma_index_limit(pma)]==0xdeadbeefL);

    r=toku_pma_free(&pma); assert(r==0); assert(pma==0);
    toku_pma_create(&pma, toku_default_compare_fun, null_db, null_filenum, 0); assert(pma!=0);

    rand4fingerprint = random();
    sum = expect_fingerprint = 0;

    {
	int i;
	for (i=0; i<100; i++) {
	    char string[10];
	    char dstring[10];
	    snprintf(string,10,"%05d",i);
	    snprintf(dstring,10,"d%d", i);
	    //printf("Inserting %d: string=%s dstring=%s (before sum=%08x) \n", i, string, dstring, sum);
	    do_insert(pma, string, strlen(string)+1, dstring, strlen(dstring)+1,  rand4fingerprint, &sum, &expect_fingerprint);
	}
    }
    r=toku_pma_free(&pma); assert(r==0); assert(pma==0);
}

static int tpi_k,tpi_v;
static void do_sum_em (bytevec key, ITEMLEN keylen, bytevec val, ITEMLEN vallen, void *v) {
    assert((unsigned long)v==0xdeadbeefL);
    assert(strlen(key)+1==keylen);
    assert(strlen(val)+1==vallen);
    tpi_k += atoi(key);
    tpi_v += atoi(val);
}

static void test_pma_iterate_internal (PMA pma, int expected_k, int expected_v) {
    tpi_k=tpi_v=0;
    toku_pma_iterate(pma, do_sum_em, (void*)0xdeadbeefL);
    assert(tpi_k==expected_k);
    assert(tpi_v==expected_v);
}

static void test_pma_iterate (void) {
    PMA pma;
    int r;

    u_int32_t rand4fingerprint = random();
    u_int32_t sum = 0;
    u_int32_t expect_fingerprint = 0;

    toku_pma_create(&pma, toku_default_compare_fun, null_db, null_filenum, 0);
    do_insert(pma, "42", 3, "-19", 4, rand4fingerprint, &sum, &expect_fingerprint);
    test_pma_iterate_internal(pma, 42, -19);

    do_insert(pma, "12", 3, "-100", 5, rand4fingerprint, &sum, &expect_fingerprint);
    test_pma_iterate_internal(pma, 42+12, -19-100);
    r=toku_pma_free(&pma); assert(r==0); assert(pma==0);
}

static void test_pma_iterate2 (void) {
    PMA pma0,pma1;
    int r;
    int sum=0;
    int n_items=0;

    u_int32_t rand4fingerprint0 = random();
    u_int32_t sum0 = 0;
    u_int32_t expect_fingerprint0 = 0;

    u_int32_t rand4fingerprint1 = random();
    u_int32_t sum1 = 0;
    u_int32_t expect_fingerprint1 = 0;

    r=toku_pma_create(&pma0, toku_default_compare_fun, null_db, null_filenum, 0); assert(r==0);
    r=toku_pma_create(&pma1, toku_default_compare_fun, null_db, null_filenum, 0); assert(r==0);
    do_insert(pma0, "a", 2, "aval", 5, rand4fingerprint0, &sum0, &expect_fingerprint0);
    do_insert(pma0, "b", 2, "bval", 5, rand4fingerprint0, &sum0, &expect_fingerprint0);
    do_insert(pma1, "x", 2, "xval", 5, rand4fingerprint1, &sum1, &expect_fingerprint1);
    PMA_ITERATE(pma0,kv __attribute__((__unused__)),kl,dv __attribute__((__unused__)),dl, (n_items++,sum+=kl+dl));
    PMA_ITERATE(pma1,kv __attribute__((__unused__)),kl,dv __attribute__((__unused__)), dl, (n_items++,sum+=kl+dl));
    assert(sum==21);
    assert(n_items==3);
    r=toku_pma_free(&pma0); assert(r==0); assert(pma0==0);
    r=toku_pma_free(&pma1); assert(r==0); assert(pma1==0);
}

/* Check to see if we can create and kill a cursor. */
void test_pma_cursor_0 (void) {
    PMA pma;
    PMA_CURSOR c=0;
    int r;
    r=toku_pma_create(&pma, toku_default_compare_fun, null_db, null_filenum, 0); assert(r==0);
    r=toku_pma_cursor(pma, &c); assert(r==0); assert(c!=0);
    printf("%s:%d\n", __FILE__, __LINE__);
    r=toku_pma_free(&pma);      assert(r!=0); /* didn't deallocate the cursor. */
    printf("%s:%d\n", __FILE__, __LINE__);
    r=toku_pma_cursor_free(&c); assert(r==0);
    printf("%s:%d\n", __FILE__, __LINE__);
    r=toku_pma_free(&pma); assert(r==0); /* did deallocate the cursor. */    
}

/* Make sure we can free the cursors in any order.  There is a doubly linked list of cursors
 * and if we free them in a different order, then different unlinking code is invoked. */
void test_pma_cursor_1 (void) {
    PMA pma;
    PMA_CURSOR c0=0,c1=0,c2=0;
    int r;
    int order;
    for (order=0; order<6; order++) {
	r=toku_pma_create(&pma, toku_default_compare_fun, null_db, null_filenum, 0); assert(r==0);
	r=toku_pma_cursor(pma, &c0); assert(r==0); assert(c0!=0);
	r=toku_pma_cursor(pma, &c1); assert(r==0); assert(c1!=0);
	r=toku_pma_cursor(pma, &c2); assert(r==0); assert(c2!=0);

	r=toku_pma_free(&pma); assert(r!=0);

	if (order<2)      { r=toku_pma_cursor_free(&c0); assert(r==0);  c0=c1; c1=c2; }
	else if (order<4) { r=toku_pma_cursor_free(&c1); assert(r==0);  c1=c2; }
	else 	          { r=toku_pma_cursor_free(&c2); assert(r==0); }

	r=toku_pma_free(&pma); assert(r!=0);

	if (order%2==0) { r=toku_pma_cursor_free(&c0); assert(r==0);  c0=c1; }
	else            { r=toku_pma_cursor_free(&c1); assert(r==0); }
	
	r=toku_pma_free(&pma); assert(r!=0);

	r = toku_pma_cursor_free(&c0); assert(r==0);
	
	r=toku_pma_free(&pma); assert(r==0);
    }
}

void test_pma_cursor_2 (void) {
    PMA pma;
    PMA_CURSOR c=0;
    int r;
    DBT key,val;
    toku_init_dbt(&key); key.flags=DB_DBT_REALLOC;
    toku_init_dbt(&val); val.flags=DB_DBT_REALLOC;
    r=toku_pma_create(&pma, toku_default_compare_fun, null_db, null_filenum, 0); assert(r==0);
    r=toku_pma_cursor(pma, &c); assert(r==0); assert(c!=0);
    r=toku_pma_cursor_set_position_last(c); assert(r==DB_NOTFOUND);
    r=toku_pma_cursor_free(&c); assert(r==0);
    r=toku_pma_free(&pma); assert(r==0);
}

void test_pma_cursor_3 (void) {
    PMA pma;
    PMA_CURSOR c=0;
    int r;
    DBT key,val;

    u_int32_t rand4fingerprint = random();
    u_int32_t sum = 0;
    u_int32_t expect_fingerprint = 0;

    r=toku_pma_create(&pma, toku_default_compare_fun, null_db, null_filenum, 0); assert(r==0);
    do_insert(pma, "x",  2, "xx", 3, rand4fingerprint, &sum, &expect_fingerprint);
    do_insert(pma, "m",  2, "mm", 3, rand4fingerprint, &sum, &expect_fingerprint);
    do_insert(pma, "aa", 3, "a",  2, rand4fingerprint, &sum, &expect_fingerprint);
    toku_init_dbt(&key); key.flags=DB_DBT_REALLOC;
    toku_init_dbt(&val); val.flags=DB_DBT_REALLOC;
    r=toku_pma_cursor(pma, &c); assert(r==0); assert(c!=0);

    r=toku_pma_cursor_set_position_first(c); assert(r==0);
    r=toku_pma_cursor_get_current(c, &key, &val); assert(r==0);
    assert(key.size=3); assert(memcmp(key.data,"aa",3)==0);
    assert(val.size=2); assert(memcmp(val.data,"a",2)==0);

    r=toku_pma_cursor_set_position_next(c); assert(r==0);
    r=toku_pma_cursor_get_current(c, &key, &val); assert(r==0);
    assert(key.size=2); assert(memcmp(key.data,"m",2)==0);
    assert(val.size=3); assert(memcmp(val.data,"mm",3)==0);
    
    r=toku_pma_cursor_set_position_next(c); assert(r==0);
    r=toku_pma_cursor_get_current(c, &key, &val); assert(r==0);
    assert(key.size=2); assert(memcmp(key.data,"x",2)==0);
    assert(val.size=3); assert(memcmp(val.data,"xx",3)==0);
    
    r=toku_pma_cursor_set_position_next(c); assert(r==DB_NOTFOUND);

    /* After an error, the cursor should still point at the same thing. */
    r=toku_pma_cursor_get_current(c, &key, &val); assert(r==0);
    assert(key.size=2); assert(memcmp(key.data,"x",2)==0);
    assert(val.size=3); assert(memcmp(val.data,"xx",3)==0);


    r=toku_pma_cursor_set_position_next(c); assert(r==DB_NOTFOUND);

    toku_free(key.data);
    toku_free(val.data);

    r=toku_pma_cursor_free(&c); assert(r==0);
    r=toku_pma_free(&pma); assert(r==0);

}

void assert_cursor_val(PMA_CURSOR cursor, int v) {
    DBT key, val;
    int error;

    toku_init_dbt(&key); key.flags = DB_DBT_MALLOC;
    toku_init_dbt(&val); val.flags = DB_DBT_MALLOC;
    error = toku_pma_cursor_get_current(cursor, &key, &val);
    assert(error == 0);
    assert( v == *(int *)val.data);
    toku_free(key.data);
    toku_free(val.data);
}

/* make sure cursors are adjusted when the pma grows */
void test_pma_cursor_4 (void) {
    int error;
    PMA pma;
    PMA_CURSOR cursora, cursorb, cursorc;
    int i;

    u_int32_t rand4fingerprint = random();
    u_int32_t sum = 0;
    u_int32_t expect_fingerprint = 0;

    printf("test_pma_cursor_4\n");
    error = toku_pma_create(&pma, toku_default_compare_fun, null_db, null_filenum, 0);
    assert(error == 0);

    for (i=1; i<=4; i += 1) {
        char k[5]; int v;

        sprintf(k, "%4.4d", i);
        v = i;
	do_insert(pma, k, strlen(k)+1, &v, sizeof v, rand4fingerprint, &sum, &expect_fingerprint);
    }
    assert(toku_pma_n_entries(pma) == 4);
    printf("a:"); toku_print_pma(pma);

    error = toku_pma_cursor(pma, &cursora);
    assert(error == 0);
    error = toku_pma_cursor_set_position_first(cursora);
    assert(error == 0);
    assert_cursor_val(cursora, 1);

    error = toku_pma_cursor(pma, &cursorb);
    assert(error == 0);
    error = toku_pma_cursor_set_position_first(cursorb);
    assert(error == 0);
    assert_cursor_val(cursorb, 1);
    error = toku_pma_cursor_set_position_next(cursorb);
    assert(error == 0);
    assert_cursor_val(cursorb, 2);

    error = toku_pma_cursor(pma, &cursorc);
    assert(error == 0);
    error = toku_pma_cursor_set_position_last(cursorc);
    assert(error == 0);
    assert_cursor_val(cursorc, 4);

    for (i=5; i<=8; i += 1) {
        char k[5]; int v;

        sprintf(k, "%4.4d", i);
        v = i;
	do_insert(pma, k, strlen(k)+1, &v, sizeof v, rand4fingerprint, &sum, &expect_fingerprint);
    }
    assert(toku_pma_n_entries(pma) == 8);
    printf("a:"); toku_print_pma(pma);

    assert_cursor_val(cursora, 1);
    assert_cursor_val(cursorb, 2);
    assert_cursor_val(cursorc, 4);

    error = toku_pma_cursor_free(&cursora);
    assert(error == 0);
    error = toku_pma_cursor_free(&cursorb);
    assert(error == 0);
    error = toku_pma_cursor_free(&cursorc);
    assert(error == 0);

    error = toku_pma_free(&pma);
    assert(error == 0);
}

void test_pma_cursor_delete(int n) {
    printf("test_pma_cursor_delete:%d\n", n);

    PMA pma;
    int error;

    u_int32_t rand4fingerprint = random();
    u_int32_t sum = 0;
    u_int32_t expect_fingerprint = 0;

    error = toku_pma_create(&pma, toku_default_compare_fun, null_db, null_filenum, 0);
    assert(error == 0);

    /* insert 1 -> 42 */
    int k, v;
    int i;
    for (i=0; i<n; i++) {
        k = i; v = -i;
	do_insert(pma, &k, sizeof k, &v, sizeof v, rand4fingerprint, &sum, &expect_fingerprint);
    }

    /* point the cursor to the first kv */
    PMA_CURSOR cursor;
    error = toku_pma_cursor(pma, &cursor);
    assert(error == 0);

    DBT cursorkey, cursorval;
    toku_init_dbt(&cursorkey); cursorkey.flags = DB_DBT_MALLOC;
    toku_init_dbt(&cursorval); cursorval.flags = DB_DBT_MALLOC;
    error = toku_pma_cursor_get_current(cursor, &cursorkey, &cursorval);
    assert(error != 0);

    error = toku_pma_cursor_set_position_first(cursor);
    assert(error == 0);

    int kk;
    toku_init_dbt(&cursorkey); cursorkey.flags = DB_DBT_MALLOC;
    toku_init_dbt(&cursorval); cursorval.flags = DB_DBT_MALLOC;
    error = toku_pma_cursor_get_current(cursor, &cursorkey, &cursorval);
    assert(error == 0);
    assert(cursorkey.size == sizeof kk);
    kk = 0;
    assert(0 == memcmp(cursorkey.data, &kk, sizeof kk));
    toku_free(cursorkey.data);
    toku_free(cursorval.data);

    /* delete the first key, which is (int)(0) with value (0) */
    k = 0; 
    do_delete(pma, &k, sizeof k, &k, sizeof k, rand4fingerprint, &sum, &expect_fingerprint);
  
    /* cursor get should fail */
    toku_init_dbt(&cursorkey); cursorkey.flags = DB_DBT_MALLOC;
    toku_init_dbt(&cursorval); cursorval.flags = DB_DBT_MALLOC;
    error = toku_pma_cursor_get_current(cursor, &cursorkey, &cursorval);
    assert(error != 0);

    error = toku_pma_cursor_set_position_next(cursor);
    if (n <= 1)
        assert(error != 0);
    else {
        assert(error == 0);
        toku_init_dbt(&cursorkey); cursorkey.flags = DB_DBT_MALLOC;
        toku_init_dbt(&cursorval); cursorval.flags = DB_DBT_MALLOC;
        error = toku_pma_cursor_get_current(cursor, &cursorkey, &cursorval);
        assert(error == 0);
        assert(cursorkey.size == sizeof kk);
        kk = 1;
        assert(0 == memcmp(cursorkey.data, &kk, sizeof kk));
        toku_free(cursorkey.data);
        toku_free(cursorval.data);
    }

    error = toku_pma_cursor_free(&cursor);
    assert(error == 0);

    error = toku_pma_free(&pma);
    assert(error == 0);
}

void test_pma_cursor (void) {
    test_pma_cursor_0();
    test_pma_cursor_1();
    test_pma_cursor_2();
    test_pma_cursor_3();
    test_pma_cursor_4();
    test_pma_cursor_delete(1);
    test_pma_cursor_delete(2);
}

int wrong_endian_compare_fun (DB *ignore __attribute__((__unused__)),
			      const DBT *a, const DBT *b) {
    unsigned int i;
    unsigned char *ad=a->data;
    unsigned char *bd=b->data;
    int siz = a->size;
    assert(a->size==b->size); // This function requires that the keys be the same size.
    
    for (i=0; i<a->size; i++) {
	if (ad[siz-1-i]<bd[siz-1-i]) return -1;
	if (ad[siz-1-i]>bd[siz-1-i]) return +1;
    }
    return 0;
}

void test_pma_compare_fun (int wrong_endian_p) {
    PMA pma;
    PMA_CURSOR c = 0;
    DBT key,val;
    int r;
    char *wrong_endian_expected_keys[] = {"00", "10", "01", "11"}; /* Sorry for being judgemental.  But it's wrong. */ 
    char *right_endian_expected_keys[] = {"00", "01", "10", "11"};
    char **expected_keys = wrong_endian_p ? wrong_endian_expected_keys : right_endian_expected_keys;
    int i;

    u_int32_t rand4fingerprint = random();
    u_int32_t sum = 0;
    u_int32_t expect_fingerprint = 0;

    r = toku_pma_create(&pma, wrong_endian_p ? wrong_endian_compare_fun : toku_default_compare_fun,
			null_db, null_filenum,
			0); assert(r==0);
    do_insert(pma, "10", 3, "10v", 4, rand4fingerprint, &sum, &expect_fingerprint);
    do_insert(pma, "00", 3, "00v", 4, rand4fingerprint, &sum, &expect_fingerprint);
    do_insert(pma, "01", 3, "01v", 4, rand4fingerprint, &sum, &expect_fingerprint);
    do_insert(pma, "11", 3, "11v", 4, rand4fingerprint, &sum, &expect_fingerprint);
    toku_init_dbt(&key); key.flags=DB_DBT_REALLOC;
    toku_init_dbt(&val); val.flags=DB_DBT_REALLOC;
    r=toku_pma_cursor(pma, &c); assert(r==0); assert(c!=0);
    
    for (i=0; i<4; i++) {
	if (i==0) {
	    r=toku_pma_cursor_set_position_first(c); assert(r==0);
	} else {
	    r=toku_pma_cursor_set_position_next(c); assert(r==0);
	}
	r=toku_pma_cursor_get_current(c, &key, &val); assert(r==0);
	//printf("Got %s, expect %s\n", (char*)key.data, expected_keys[i]);
	assert(key.size=3); assert(memcmp(key.data,expected_keys[i],3)==0);
	assert(val.size=4); assert(memcmp(val.data,expected_keys[i],2)==0);
	assert(memcmp(2+(char*)val.data,"v",2)==0);
    }

    r=toku_pma_cursor_set_position_next(c); assert(r==DB_NOTFOUND);
    
    toku_free(key.data);
    toku_free(val.data);

    r=toku_pma_cursor_free(&c); assert(r==0);
    r=toku_pma_free(&pma); assert(r==0);
}

void test_pma_split_n(int n) {
    PMA pmaa, pmab, pmac;
    int error;
    int i;
    int na, nb, nc;

    u_int32_t rand4fingerprint = random();
    u_int32_t sum = 0;
    u_int32_t expect_fingerprint = 0;

    u_int32_t brand = random();
    u_int32_t bsum = 0;
    u_int32_t crand = random();
    u_int32_t csum = 0;

    printf("test_pma_split_n:%d\n", n);

    error = toku_pma_create(&pmaa, toku_default_compare_fun, null_db, null_filenum, 0);
    assert(error == 0);
    error = toku_pma_create(&pmab, toku_default_compare_fun, null_db, null_filenum, 0);
    assert(error == 0);
    error = toku_pma_create(&pmac, toku_default_compare_fun, null_db, null_filenum, 0);
    assert(error == 0);

    /* insert some kv pairs */
    for (i=0; i<n; i++) {
        char k[5]; int v;

        sprintf(k, "%4.4d", i);
        v = i;
	do_insert(pmaa, k, strlen(k)+1, &v, sizeof v, rand4fingerprint, &sum, &expect_fingerprint);

        toku_pma_verify(pmaa);
    }

    printf("a:"); toku_print_pma(pmaa);

    error = toku_pma_split(pmaa, 0, 0, pmab, 0, brand, &bsum, pmac, 0, crand, &csum);
    assert(error == 0);
    toku_pma_verify(pmaa);
    toku_pma_verify(pmab);
    toku_pma_verify(pmac);
    toku_pma_verify_fingerprint(pmab, brand, bsum);
    toku_pma_verify_fingerprint(pmac, crand, csum);

    printf("a:"); toku_print_pma(pmaa);
    na = toku_pma_n_entries(pmaa);
    printf("b:"); toku_print_pma(pmab);
    nb = toku_pma_n_entries(pmab);
    printf("c:"); toku_print_pma(pmac);
    nc = toku_pma_n_entries(pmac);

    assert(na == 0);
    assert(nb + nc == n);

    error = toku_pma_free(&pmaa);
    assert(error == 0);
    error = toku_pma_free(&pmab);
    assert(error == 0);
    error = toku_pma_free(&pmac);
    assert(error == 0);
}

void test_pma_dup_split_n(int n, int dup_mode) {
    PMA pmaa, pmab, pmac;
    int error;
    int i;
    int na, nb, nc;

    u_int32_t rand4sum = random();
    u_int32_t sum = 0;
    u_int32_t expect_sum = 0;

    u_int32_t brand = random();
    u_int32_t bsum = 0;
    u_int32_t crand = random();
    u_int32_t csum = 0;

    printf("test_pma_dup_split_n:%d %d\n", n, dup_mode);

    error = toku_pma_create(&pmaa, toku_default_compare_fun, null_db, null_filenum, 0);
    assert(error == 0);
    toku_pma_set_dup_mode(pmaa, dup_mode);
    error = toku_pma_create(&pmab, toku_default_compare_fun, null_db, null_filenum, 0);
    assert(error == 0);
    toku_pma_set_dup_mode(pmab, dup_mode);
    error = toku_pma_create(&pmac, toku_default_compare_fun, null_db, null_filenum, 0);
    assert(error == 0);
    toku_pma_set_dup_mode(pmac, dup_mode);

    /* insert some kv pairs */
    int dupkey = random();
    for (i=0; i<n; i++) {
        int v = i;
    	do_insert(pmaa, &dupkey, sizeof dupkey, &v, sizeof v, rand4sum, &sum, &expect_sum);

        toku_pma_verify(pmaa);
    }

    printf("a:"); toku_print_pma(pmaa);

    DBT splitk;

    error = toku_pma_split(pmaa, 0, &splitk, pmab, 0, brand, &bsum, pmac, 0, crand, &csum);
    assert(error == 0);
    toku_pma_verify(pmaa);
    toku_pma_verify(pmab);
    toku_pma_verify(pmac);
    toku_pma_verify_fingerprint(pmab, brand, bsum);
    toku_pma_verify_fingerprint(pmac, crand, csum);

    if (0) { printf("a:"); toku_print_pma(pmaa); }
    na = toku_pma_n_entries(pmaa);
    if (0) { printf("b:"); toku_print_pma(pmab); }
    nb = toku_pma_n_entries(pmab);
    if (0) { printf("c:"); toku_print_pma(pmac); }
    nc = toku_pma_n_entries(pmac);

    if (na > 0) {
        int kk;
        assert(splitk.size == sizeof kk);
        memcpy(&kk, splitk.data, splitk.size);
        assert(kk == dupkey);
        if (nb > 0) assert(splitk.flags & BRT_PIVOT_PRESENT_L);
        if (nc > 0) assert(splitk.flags & BRT_PIVOT_PRESENT_R);
    }

    if (splitk.data) toku_free(splitk.data);

    assert(na == 0);
    assert(nb + nc == n);

    error = toku_pma_free(&pmaa);
    assert(error == 0);
    error = toku_pma_free(&pmab);
    assert(error == 0);
    error = toku_pma_free(&pmac);
    assert(error == 0);
}

void test_pma_split_varkey(void) {
    char *keys[] = {
        "this", "is", "a", "key", "this is a really really big key", "zz", 0 };
    PMA pmaa, pmab, pmac;
    int error;
    int i;
    int n, na, nb, nc;

    u_int32_t rand4fingerprint = random();
    u_int32_t sum = 0;
    u_int32_t expect_fingerprint = 0;

    u_int32_t brand = random();
    u_int32_t bsum = 0;
    u_int32_t crand = random();
    u_int32_t csum = 0;

    printf("test_pma_split_varkey\n");

    error = toku_pma_create(&pmaa, toku_default_compare_fun, null_db, null_filenum, 0);
    assert(error == 0);
    error = toku_pma_create(&pmab, toku_default_compare_fun, null_db, null_filenum, 0);
    assert(error == 0);
    error = toku_pma_create(&pmac, toku_default_compare_fun, null_db, null_filenum, 0);
    assert(error == 0);

    /* insert some kv pairs */
    for (i=0; keys[i]; i++) {
        char v = i;
	do_insert(pmaa, keys[i], strlen(keys[i])+1, &v, sizeof v, rand4fingerprint, &sum, &expect_fingerprint);
    }
    n = i;

    printf("a:"); toku_print_pma(pmaa);

    error = toku_pma_split(pmaa, 0, 0, pmab, 0, brand, &bsum, pmac, 0, crand, &csum);
    assert(error == 0);
    toku_pma_verify(pmaa);
    toku_pma_verify(pmab);
    toku_pma_verify(pmac);
    toku_pma_verify_fingerprint(pmab, brand, bsum);
    toku_pma_verify_fingerprint(pmac, crand, csum);

    printf("a:"); toku_print_pma(pmaa);
    na = toku_pma_n_entries(pmaa);
    printf("b:"); toku_print_pma(pmab);
    nb = toku_pma_n_entries(pmab);
    printf("c:"); toku_print_pma(pmac);
    nc = toku_pma_n_entries(pmac);

    assert(na == 0);
    assert(nb + nc == n);

    error = toku_pma_free(&pmaa);
    assert(error == 0);
    error = toku_pma_free(&pmab);
    assert(error == 0);
    error = toku_pma_free(&pmac);
    assert(error == 0);
}

void print_cursor(const char *str, PMA_CURSOR cursor) {
    DBT key, val;
    int error;

    printf("cursor %s: ", str);
    toku_init_dbt(&key); key.flags = DB_DBT_MALLOC;
    toku_init_dbt(&val); val.flags = DB_DBT_MALLOC;
    error = toku_pma_cursor_get_current(cursor, &key, &val);
    assert(error == 0);
    printf("%s ", (char*)key.data);
    toku_free(key.data);
    toku_free(val.data);
    printf("\n");
}

void walk_cursor(const char *str, PMA_CURSOR cursor) {
    DBT key, val;
    int error;

    printf("walk %s: ", str);
    for (;;) {
        toku_init_dbt(&key); key.flags = DB_DBT_MALLOC;
        toku_init_dbt(&val); val.flags = DB_DBT_MALLOC;
        error = toku_pma_cursor_get_current(cursor, &key, &val);
        assert(error == 0);
        printf("%s ", (char*)key.data);
        toku_free(key.data);
        toku_free(val.data);

        error = toku_pma_cursor_set_position_next(cursor);
        if (error != 0)
            break;
    }
    printf("\n");
}

void walk_cursor_reverse(const char *str, PMA_CURSOR cursor) {
    DBT key, val;
    int error;

    printf("walk %s: ", str);
    for (;;) {
        toku_init_dbt(&key); key.flags = DB_DBT_MALLOC;
        toku_init_dbt(&val); val.flags = DB_DBT_MALLOC;
        error = toku_pma_cursor_get_current(cursor, &key, &val);
        assert(error == 0);
        printf("%s ", (char*)key.data);
        toku_free(key.data);
        toku_free(val.data);

        error = toku_pma_cursor_set_position_prev(cursor);
        if (error != 0)
            break;
    }
    printf("\n");
}

void test_pma_split_cursor(void) {
    PMA pmaa, pmab, pmac;
    PMA_CURSOR cursora, cursorb, cursorc;
    int error;
    int i;
    int na, nb, nc;

    u_int32_t rand4fingerprint = random();
    u_int32_t sum = 0;
    u_int32_t expect_fingerprint = 0;

    u_int32_t brand = random();
    u_int32_t bsum = 0;
    u_int32_t crand = random();
    u_int32_t csum = 0;


    printf("test_pma_split_cursor\n");

    error = toku_pma_create(&pmaa, toku_default_compare_fun, null_db, null_filenum, 0);
    assert(error == 0);
    error = toku_pma_create(&pmab, toku_default_compare_fun, null_db, null_filenum, 0);
    assert(error == 0);
    error = toku_pma_create(&pmac, toku_default_compare_fun, null_db, null_filenum, 0);
    assert(error == 0);

    /* insert some kv pairs */
    for (i=1; i<=16; i += 1) {
        char k[11]; int v;

        snprintf(k, sizeof k, "%.10d", i);
        v = i;

	do_insert(pmaa, k, sizeof k, &v, sizeof v, rand4fingerprint, &sum, &expect_fingerprint);
    }
    assert(toku_pma_n_entries(pmaa) == 16);
    printf("a:"); toku_print_pma(pmaa);

    error = toku_pma_cursor(pmaa, &cursora);
    assert(error == 0);
    error = toku_pma_cursor_set_position_first(cursora);
    assert(error == 0);
    // print_cursor("cursora", cursora);
    assert_cursor_val(cursora, 1);

    error = toku_pma_cursor(pmaa, &cursorb);
    assert(error == 0);
    error = toku_pma_cursor_set_position_first(cursorb);
    assert(error == 0);
    error = toku_pma_cursor_set_position_next(cursorb);
    assert(error == 0);
    // print_cursor("cursorb", cursorb);
    assert_cursor_val(cursorb, 2);

    error = toku_pma_cursor(pmaa, &cursorc);
    assert(error == 0);
    error = toku_pma_cursor_set_position_last(cursorc);
    assert(error == 0);
    // print_cursor("cursorc", cursorc);
    assert_cursor_val(cursorc, 16);

    error = toku_pma_split(pmaa, 0, 0, pmab, 0, brand, &bsum, pmac, 0, crand, &csum);
    assert(error == 0);

    toku_pma_verify_fingerprint(pmab, brand, bsum);
    toku_pma_verify_fingerprint(pmac, crand, csum);

    printf("a:"); toku_print_pma(pmaa);
    na = toku_pma_n_entries(pmaa);
    assert(na == 0);
    printf("b:"); toku_print_pma(pmab);
    nb = toku_pma_n_entries(pmab);
    printf("c:"); toku_print_pma(pmac);
    nc = toku_pma_n_entries(pmac);
    assert(nb + nc == 16);

    /* cursors open, should fail */
    error = toku_pma_free(&pmab);
    assert(error != 0);

    /* walk cursora */
    assert_cursor_val(cursora, 1);
    walk_cursor("cursora", cursora);

    /* walk cursorb */
    assert_cursor_val(cursorb, 2);
    walk_cursor("cursorb", cursorb);

    /* walk cursorc */
    assert_cursor_val(cursorc, 16);
    walk_cursor("cursorc", cursorc);
    walk_cursor_reverse("cursorc reverse", cursorc);

    error = toku_pma_cursor_free(&cursora);
    assert(error == 0);
    error = toku_pma_cursor_free(&cursorb);
    assert(error == 0);
    error = toku_pma_cursor_free(&cursorc);
    assert(error == 0);

    error = toku_pma_free(&pmaa);
    assert(error == 0);
    error = toku_pma_free(&pmab);
    assert(error == 0);
    error = toku_pma_free(&pmac);
    assert(error == 0);
}

void test_pma_split(void) {
    test_pma_split_n(0); memory_check_all_free();
    test_pma_split_n(1); memory_check_all_free();
    test_pma_split_n(2); memory_check_all_free();
    test_pma_split_n(4); memory_check_all_free();
    test_pma_split_n(8); memory_check_all_free();
    test_pma_split_n(9);  memory_check_all_free();
    test_pma_dup_split_n(0, TOKU_DB_DUP);  memory_check_all_free();
    test_pma_dup_split_n(1, TOKU_DB_DUP);  memory_check_all_free();
    test_pma_dup_split_n(9, TOKU_DB_DUP);  memory_check_all_free();
    test_pma_split_varkey(); memory_check_all_free();
    test_pma_split_cursor(); memory_check_all_free();
}

/*
 * test the toku_pma_bulk_insert function by creating n kv pairs and bulk 
 * inserting them into an empty pma.  verify that the pma contains all
 * of the kv pairs.
 */
void test_pma_bulk_insert_n(int n) {
    PMA pma;
    int error;
    int i;
    DBT *keys, *vals;

    u_int32_t rand4fingerprint = random();
    u_int32_t sum = 0;
    u_int32_t expect_fingerprint = 0;

    printf("test_pma_bulk_insert_n: %d\n", n);

    error = toku_pma_create(&pma, toku_default_compare_fun, null_db, null_filenum, 0);
    assert(error == 0);

    /* init n kv pairs */
    keys = toku_malloc(n * sizeof (DBT));
    assert(keys);
    vals = toku_malloc(n * sizeof (DBT));
    assert(vals);

    /* init n kv pairs */
    for (i=0; i<n; i++) {
        char kstring[11];
        char *k; int klen;
        int *v; int vlen;

        snprintf(kstring, sizeof kstring, "%.10d", i);
        klen = strlen(kstring) + 1;
        k = toku_malloc(klen);
        assert(k);
        strcpy(k, kstring);
        toku_fill_dbt(&keys[i], k, klen);

        vlen = sizeof (int);
        v = toku_malloc(vlen);
        assert(v);
        *v = i;
        toku_fill_dbt(&vals[i], v, vlen);

	expect_fingerprint += rand4fingerprint*toku_calccrc32_kvpair (k, klen, v, vlen);
    }

    /* bulk insert n kv pairs */
    error = toku_pma_bulk_insert(pma, keys, vals, n, rand4fingerprint, &sum);
    assert(error == 0);
    assert(sum==expect_fingerprint);
    toku_pma_verify(pma);
    toku_pma_verify_fingerprint(pma, rand4fingerprint, sum);

    /* verify */
    if (0) toku_print_pma(pma);
    assert(n == toku_pma_n_entries(pma));
    for (i=0; i<n; i++) {
        DBT val;
        toku_init_dbt(&val); val.flags = DB_DBT_MALLOC;
        error = toku_pma_lookup(pma, &keys[i], &val);
        assert(error == 0);
        assert(vals[i].size == val.size);
        assert(memcmp(vals[i].data, val.data, val.size) == 0);
        toku_free(val.data);
    }

    /* cleanup */
    for (i=0; i<n; i++) {
        toku_free(keys[i].data);
        toku_free(vals[i].data);
    }

    error = toku_pma_free(&pma);
    assert(error == 0);
    
    toku_free(keys);
    toku_free(vals);
}

void test_pma_bulk_insert(void) {
    test_pma_bulk_insert_n(0); memory_check_all_free();
    test_pma_bulk_insert_n(1); memory_check_all_free();
    test_pma_bulk_insert_n(2); memory_check_all_free();
    test_pma_bulk_insert_n(3); memory_check_all_free();
    test_pma_bulk_insert_n(4); memory_check_all_free();
    test_pma_bulk_insert_n(5); memory_check_all_free();
    test_pma_bulk_insert_n(8); memory_check_all_free();
    test_pma_bulk_insert_n(32); memory_check_all_free();
}

void test_pma_insert_or_replace(void) {
    PMA pma;
    int r;
    DBT dbtk, dbtv;
    int n_diff=-2;
    u_int32_t rand4fingerprint = random();
    u_int32_t sum = 0;
    u_int32_t expect_fingerprint = 0;
    r = toku_pma_create(&pma, toku_default_compare_fun, null_db, null_filenum, 0);
    assert(r==0);
    r = toku_pma_insert_or_replace(pma, toku_fill_dbt(&dbtk, "aaa", 4), toku_fill_dbt(&dbtv, "zzz", 4), &n_diff, NULL_ARGS, rand4fingerprint, &sum);
    assert(r==0); assert(n_diff==-1);
    add_fingerprint_and_check(rand4fingerprint, sum, &expect_fingerprint, "aaa", 4, "zzz", 4);

    r = toku_pma_lookup(pma, toku_fill_dbt(&dbtk, "aaa", 4), toku_init_dbt(&dbtv));
    assert(r==0); assert(dbtv.size==4); assert(memcmp(dbtv.data, "zzz", 4)==0);

    r = toku_pma_insert_or_replace(pma, toku_fill_dbt(&dbtk, "bbbb", 5), toku_fill_dbt(&dbtv, "ww", 3), &n_diff, NULL_ARGS, rand4fingerprint, &sum);
    assert(r==0); assert(n_diff==-1);
    add_fingerprint_and_check(rand4fingerprint, sum, &expect_fingerprint, "bbbb", 5, "ww", 3);

    r = toku_pma_lookup(pma, toku_fill_dbt(&dbtk, "aaa", 4), toku_init_dbt(&dbtv));
    assert(r==0); assert(dbtv.size==4); assert(memcmp(dbtv.data, "zzz", 4)==0);

    r = toku_pma_lookup(pma, toku_fill_dbt(&dbtk, "bbbb", 5), toku_init_dbt(&dbtv));
    assert(r==0); assert(dbtv.size==3); assert(memcmp(dbtv.data, "ww", 3)==0);

    // replae bbbb
    r = toku_pma_insert_or_replace(pma, toku_fill_dbt(&dbtk, "bbbb", 5), toku_fill_dbt(&dbtv, "xxxx", 5), &n_diff, NULL_ARGS, rand4fingerprint, &sum);
    assert(r==0); assert(n_diff==3);
    expect_fingerprint -= rand4fingerprint*toku_calccrc32_kvpair("bbbb", 5, "ww", 3);
    add_fingerprint_and_check(rand4fingerprint, sum, &expect_fingerprint, "bbbb", 5, "xxxx", 5);
    
    r = toku_pma_lookup(pma, toku_fill_dbt(&dbtk, "aaa", 4), toku_init_dbt(&dbtv));
    assert(r==0); assert(dbtv.size==4); assert(memcmp(dbtv.data, "zzz", 4)==0);

    r = toku_pma_lookup(pma, toku_fill_dbt(&dbtk, "bbbb", 5), toku_init_dbt(&dbtv));
    assert(r==0); assert(dbtv.size==5); assert(memcmp(dbtv.data, "xxxx", 3)==0);

    r=toku_pma_free(&pma);
    assert(r==0);
}

/*
 * test that the pma shrinks back to its minimum size.
 */
void test_pma_delete_shrink(int n) {
    PMA pma;
    int r;
    int i;

    u_int32_t rand4fingerprint = random();
    u_int32_t sum = 0;
    u_int32_t expect_fingerprint = 0;

    printf("test_pma_delete_shrink:%d\n", n);

    r = toku_pma_create(&pma, toku_default_compare_fun, null_db, null_filenum, n*(8 + 11 + sizeof (int)));
    assert(r == 0);

    /* insert */
    for (i=0; i<n; i++) {
        char k[11];
        int v;

        snprintf(k, sizeof k, "%.10d", i);
        v = i;

	do_insert(pma, k, sizeof k, &v, sizeof v, rand4fingerprint, &sum, &expect_fingerprint);
    }

    /* delete */
    for (i=0; i<n; i++) {
        char k[11]; 
	int v=i;

        snprintf(k, sizeof k, "%.10d", i);
	do_delete(pma, k, strlen(k)+1, &v, sizeof v, rand4fingerprint, &sum, &expect_fingerprint);
    }
    assert(pma->N == PMA_MIN_ARRAY_SIZE);

    r = toku_pma_free(&pma);
    assert(r == 0);
}

/*
 * test that the pma shrinks to its minimum size after inserting
 * random keys and then deleting them.
 */
void test_pma_delete_random(int n) {
    PMA pma;
    int r;
    int i;
    int keys[n];

    u_int32_t rand4fingerprint = random();
    u_int32_t sum = 0;
    u_int32_t expect_fingerprint = 0;

    printf("test_pma_delete_random:%d\n", n);

    r = toku_pma_create(&pma, toku_default_compare_fun, null_db, null_filenum, n * (8 + 11 + sizeof (int)));
    assert(r == 0);

    for (i=0; i<n; i++) {
        keys[i] = random();
    }

    /* insert */
    for (i=0; i<n; i++) {
        char k[11];
        int v;

        snprintf(k, sizeof k, "%.10d", keys[i]);
        v = keys[i];

	do_insert(pma, k, strlen(k)+1, &v, sizeof v, rand4fingerprint, &sum, &expect_fingerprint);
    }

    /* delete */
    for (i=0; i<n; i++) {
        char k[11]; 
	int v = keys[i];

        snprintf(k, sizeof k, "%.10d", keys[i]);
	do_delete(pma, k, strlen(k)+1, &v, sizeof v, rand4fingerprint, &sum, &expect_fingerprint);
    }
    assert(pma->N == PMA_MIN_ARRAY_SIZE);

    r = toku_pma_free(&pma);
    assert(r == 0);
}

void assert_cursor_equal(PMA_CURSOR pmacursor, int v) {
    DBT key, val;
    toku_init_dbt(&key); key.flags = DB_DBT_MALLOC;
    toku_init_dbt(&val); val.flags = DB_DBT_MALLOC;
    int r;
    r = toku_pma_cursor_get_current(pmacursor, &key, &val);
    assert(r == 0);
    if (0) printf("key %s\n", (char*) key.data);
    int thev;
    assert(val.size == sizeof thev);
    memcpy(&thev, val.data, val.size);
    assert(thev == v);
    toku_free(key.data);
    toku_free(val.data);
}

void assert_cursor_nokey(PMA_CURSOR pmacursor) {
   DBT key, val;
    toku_init_dbt(&key); key.flags = DB_DBT_MALLOC;
    toku_init_dbt(&val); val.flags = DB_DBT_MALLOC;
    int r;
    r = toku_pma_cursor_get_current(pmacursor, &key, &val);
    assert(r != 0);
}

/*
 * test that pma delete ops update pma cursors
 * - insert n keys
 * - point the cursor at the last key in the pma
 * - delete keys sequentially.  the cursor should be stuck at the
 * last key until the last key is deleted.
 */
void test_pma_delete_cursor(int n) {
    printf("test_delete_cursor:%d\n", n);

    PMA pma;
    int r;

    u_int32_t rand4fingerprint = random();
    u_int32_t sum = 0;
    u_int32_t expect_fingerprint = 0;

    r = toku_pma_create(&pma, toku_default_compare_fun, null_db, null_filenum, 0);
    assert(r == 0);

    int i;
    for (i=0; i<n; i++) {
        char k[11];
        int v;

        snprintf(k, sizeof k, "%.10d", i);
        v = i;
	do_insert(pma, k, strlen(k)+1, &v, sizeof v, rand4fingerprint, &sum, &expect_fingerprint);
    }

    PMA_CURSOR pmacursor;

    r = toku_pma_cursor(pma, &pmacursor);
    assert(r == 0);

    r = toku_pma_cursor_set_position_last(pmacursor);
    assert(r == 0);

    assert_cursor_equal(pmacursor, n-1);

    for (i=0; i<n; i++) {
        char k[11]; 
	int v=i;

        snprintf(k, sizeof k, "%.10d", i);
	do_delete(pma, k, strlen(k)+1, &v, sizeof v, rand4fingerprint, &sum, &expect_fingerprint);
        if (i == n-1)
            assert_cursor_nokey(pmacursor);
        else
            assert_cursor_equal(pmacursor, n-1);
    }
    assert(pma->N == PMA_MIN_ARRAY_SIZE);

    r = toku_pma_cursor_free(&pmacursor);
    assert(r == 0);

    r = toku_pma_free(&pma);
    assert(r == 0);
}

/*
 * insert k,1
 * place cursor at k
 * delete k
 * cursor get current
 * lookup k
 * insert k,2
 * lookup k
 * cursor get current 
 */
void test_pma_delete_insert() {
    printf("test_pma_delete_insert\n");

    PMA pma;
    int error;

    u_int32_t rand4fingerprint = random();
    u_int32_t sum = 0;
    u_int32_t expect_fingerprint = 0;

    error = toku_pma_create(&pma, toku_default_compare_fun, null_db, null_filenum, 0);
    assert(error == 0);

    PMA_CURSOR pmacursor;

    error = toku_pma_cursor(pma, &pmacursor);
    assert(error == 0);

    DBT key, val;
    int k, v;

    k = 1; v = 1;
    do_insert(pma, &k, sizeof k, &v, sizeof v, rand4fingerprint, &sum, &expect_fingerprint);

    error = toku_pma_cursor_set_position_first(pmacursor);
    assert(error == 0);
    assert_cursor_equal(pmacursor, 1);

    k = 1; v = 1;
    do_delete(pma, &k, sizeof k, &v, sizeof v, rand4fingerprint, &sum, &expect_fingerprint);
    assert_cursor_nokey(pmacursor);

    k = 1;
    toku_fill_dbt(&key, &k, sizeof k);
    toku_init_dbt(&val); val.flags = DB_DBT_MALLOC;
    error = toku_pma_lookup(pma, &key, &val);
    assert(error != 0);

    k = 1; v = 2;
    do_insert(pma, &k, sizeof k, &v, sizeof v, rand4fingerprint, &sum, &expect_fingerprint);
    assert_cursor_equal(pmacursor, 2);

    error = toku_pma_cursor_free(&pmacursor);
    assert(error == 0);

    error = toku_pma_free(&pma);
    assert(error == 0);
}

void test_pma_double_delete() {
    printf("test_pma_double_delete\n");

    PMA pma;
    int error;

    u_int32_t rand4fingerprint = random();
    u_int32_t sum = 0;
    u_int32_t expect_fingerprint = 0;

    error = toku_pma_create(&pma, toku_default_compare_fun, null_db, null_filenum, 0);
    assert(error == 0);

    PMA_CURSOR pmacursor;

    error = toku_pma_cursor(pma, &pmacursor);
    assert(error == 0);

    DBT key;
    int k, v;

    k = 1; v = 1;
    do_insert(pma, &k, sizeof k, &v, sizeof v, rand4fingerprint, &sum, &expect_fingerprint);

    error = toku_pma_cursor_set_position_first(pmacursor);
    assert(error == 0);
    assert_cursor_equal(pmacursor, 1);

    k = 1; v = 1;
    do_delete(pma, &k, sizeof k, &v, sizeof v, rand4fingerprint, &sum, &expect_fingerprint);
    assert_cursor_nokey(pmacursor);

    k = 1;
    toku_fill_dbt(&key, &k, sizeof k);
    error = toku_pma_delete(pma, &key, rand4fingerprint, &sum, 0);
    assert(error == DB_NOTFOUND);
    assert(sum == expect_fingerprint);

    error = toku_pma_cursor_free(&pmacursor);
    assert(error == 0);

    error = toku_pma_free(&pma);
    assert(error == 0);
}

void test_pma_cursor_first_delete_last() {
    printf("test_pma_cursor_first_delete_last\n");

    int error;
    PMA pma;

    u_int32_t rand4fingerprint = random();
    u_int32_t sum = 0;
    u_int32_t expect_fingerprint = 0;

    error = toku_pma_create(&pma, toku_default_compare_fun, null_db, null_filenum, 0);
    assert(error == 0);

    int k, v;

    int i;
    for (i=1; i<=2; i++) {
        k = htonl(i);
        v = i;
	do_insert(pma, &k, sizeof k, &v, sizeof v, rand4fingerprint, &sum, &expect_fingerprint);
    }
    assert(toku_pma_n_entries(pma) == 2);

    PMA_CURSOR pmacursor;

    error = toku_pma_cursor(pma, &pmacursor);
    assert(error == 0);

    error = toku_pma_cursor_set_position_first(pmacursor);
    assert(error == 0);

    k = htonl(1);
    v = 1;
    do_delete(pma, &k, sizeof k, &v, sizeof v, rand4fingerprint, &sum, &expect_fingerprint);
    assert(toku_pma_n_entries(pma) == 2);

    error = toku_pma_cursor_set_position_last(pmacursor);
    assert(error == 0);
    assert(toku_pma_n_entries(pma) == 1);

    error = toku_pma_cursor_free(&pmacursor);
    assert(error == 0);

    error = toku_pma_free(&pma);
    assert(error == 0);
}

void test_pma_cursor_last_delete_first() {
    printf("test_pma_cursor_last_delete_first\n");

    int error;
    PMA pma;

    u_int32_t rand4fingerprint = random();
    u_int32_t sum = 0;
    u_int32_t expect_fingerprint = 0;

    error = toku_pma_create(&pma, toku_default_compare_fun, null_db, null_filenum, 0);
    assert(error == 0);

    int k, v;

    int i;
    for (i=1; i<=2; i++) {
        k = htonl(i);
        v = i;
	do_insert(pma, &k, sizeof k, &v, sizeof v, rand4fingerprint, &sum, &expect_fingerprint);
    }
    assert(toku_pma_n_entries(pma) == 2);

    PMA_CURSOR pmacursor;

    error = toku_pma_cursor(pma, &pmacursor);
    assert(error == 0);

    error = toku_pma_cursor_set_position_last(pmacursor);
    assert(error == 0);

    k = htonl(2);
    v = 2;
    do_delete(pma, &k, sizeof k, &v, sizeof v, rand4fingerprint, &sum, &expect_fingerprint);
    assert(toku_pma_n_entries(pma) == 2);

    error = toku_pma_cursor_set_position_first(pmacursor);
    assert(error == 0);
    assert(toku_pma_n_entries(pma) == 1);

    error = toku_pma_cursor_free(&pmacursor);
    assert(error == 0);

    error = toku_pma_free(&pma);
    assert(error == 0);
}

void test_pma_delete() {
    test_pma_delete_shrink(256);  memory_check_all_free();
    test_pma_delete_random(256);  memory_check_all_free();
    test_pma_delete_cursor(32);   memory_check_all_free();
    test_pma_delete_insert();     memory_check_all_free();
    test_pma_double_delete();     memory_check_all_free();
    test_pma_cursor_first_delete_last(); memory_check_all_free();
    test_pma_cursor_last_delete_first(); memory_check_all_free();
}

void test_pma_already_there() {
    printf("test_pma_already_there\n");

    int error;
    PMA pma;

    u_int32_t rand4fingerprint = random();
    u_int32_t sum = 0;

    error = toku_pma_create(&pma, toku_default_compare_fun, null_db, null_filenum, 0);
    assert(error == 0);

    DBT key, val;
    int k, v;

    k = 1; v = 1;
    toku_fill_dbt(&key, &k, sizeof k);
    toku_fill_dbt(&val, &v, sizeof v);
    error = toku_pma_insert(pma, &key, &val, NULL_ARGS, rand4fingerprint, &sum);
    assert(error == 0);
    u_int32_t savesum = sum;
    error = toku_pma_insert(pma, &key, &val, NULL_ARGS, rand4fingerprint, &sum);
    assert(error == BRT_ALREADY_THERE);
    assert(sum==savesum);

    error = toku_pma_free(&pma);
    assert(error == 0);
}

void test_pma_cursor_set_key() {
    printf("test_pma_cursor_set_key\n");

    int error;
    PMA pma;

    error = toku_pma_create(&pma, toku_default_compare_fun, null_db, null_filenum, 0);
    assert(error == 0);

    DBT key, val;
    int k, v;

    u_int32_t rand4fingerprint = random();
    u_int32_t sum = 0;
    u_int32_t expect_fingerprint = 0;

    const int n = 100;
    int i;
    for (i=0; i<n; i += 10) {
        k = htonl(i);
        v = i;
	do_insert(pma, &k, sizeof k, &v, sizeof v, rand4fingerprint, &sum, &expect_fingerprint);
    }

    PMA_CURSOR cursor;
    error = toku_pma_cursor(pma, &cursor);
    assert(error == 0);

    for (i=0; i<n; i += 1) {
        k = htonl(i);
        toku_fill_dbt(&key, &k, sizeof k);
        error = toku_pma_cursor_set_key(cursor, &key);
        if (i % 10 == 0) {
            assert(error == 0);
            toku_init_dbt(&val); val.flags = DB_DBT_MALLOC;
            error = toku_pma_cursor_get_current(cursor, &key, &val);
            assert(error == 0);
            int vv;
            assert(val.size == sizeof vv);
            memcpy(&vv, val.data, val.size);
            assert(vv == i);
            toku_free(val.data);
        } else 
            assert(error == DB_NOTFOUND);
    }

    error = toku_pma_cursor_free(&cursor);
    assert(error == 0);

    error = toku_pma_free(&pma);
    assert(error == 0);
}

/*
 * verify that set range works with a pma with keys 10, 20, 30 ... 90
 */
void test_pma_cursor_set_range() {
    printf("test_pma_cursor_set_range\n");

    int error;
    PMA pma;

    u_int32_t rand4fingerprint = random();
    u_int32_t sum = 0;
    u_int32_t expect_fingerprint = 0;

    error = toku_pma_create(&pma, toku_default_compare_fun, null_db, null_filenum, 0);
    assert(error == 0);

    DBT key, val;
    int k, v;

    const int smallest_key = 10;
    const int largest_key = 90;
    int i;
    for (i=smallest_key; i<=largest_key; i += 10) {
        k = htonl(i);
        v = i;
	do_insert(pma, &k, sizeof k, &v, sizeof v, rand4fingerprint, &sum, &expect_fingerprint);
    }

    PMA_CURSOR cursor;
    error = toku_pma_cursor(pma, &cursor);
    assert(error == 0);

    for (i=0; i<100; i += 1) {
        k = htonl(i);
        toku_fill_dbt(&key, &k, sizeof k);
        error = toku_pma_cursor_set_range(cursor, &key);
        if (error != 0) {
            assert(error == DB_NOTFOUND);
            assert(i > largest_key);
        } else {
            toku_init_dbt(&val); val.flags = DB_DBT_MALLOC;
            error = toku_pma_cursor_get_current(cursor, &key, &val);
            assert(error == 0);
            int vv;
            assert(val.size == sizeof vv);
            memcpy(&vv, val.data, val.size);
            if (i <= smallest_key)
                assert(vv == smallest_key);
            else
                assert(vv == (((i+9)/10)*10));
            toku_free(val.data);
        }
    }

    error = toku_pma_cursor_free(&cursor);
    assert(error == 0);

    error = toku_pma_free(&pma);
    assert(error == 0);
}

void test_pma_cursor_delete_under() {
    printf("test_pma_cursor_delete_under\n");

    int error;
    PMA pma;

    u_int32_t rand4fingerprint = random();
    u_int32_t sum = 0;
    u_int32_t expect_fingerprint = 0;

    const int n = 1000;

    error = toku_pma_create(&pma, toku_default_compare_fun, null_db, null_filenum, n * (8 + sizeof (int) + sizeof (int)));
    assert(error == 0);

    PMA_CURSOR cursor;
    error = toku_pma_cursor(pma, &cursor);
    assert(error == 0);

    int kvsize;

    /* delete under an uninitialized cursor should fail */
    error = toku_pma_cursor_delete_under(cursor, &kvsize);
    assert(error == DB_NOTFOUND);

    DBT key, val;
    int k, v;

    int i;

    /* insert 0 .. n-1 */
    for (i=0; i<n; i++) {
        k = htonl(i);
        v = i;
	do_insert(pma, &k, sizeof k, &v, sizeof v, rand4fingerprint, &sum, &expect_fingerprint);
    }

    for (i=0;;i++) {
        error = toku_pma_cursor_set_position_next(cursor);
        if (error != 0) {
            assert(error == DB_NOTFOUND);
            break;
        } 
        toku_init_dbt(&key); key.flags = DB_DBT_MALLOC;
        toku_init_dbt(&val); val.flags = DB_DBT_MALLOC;
        error = toku_pma_cursor_get_current(cursor, &key, &val);
        assert(error == 0);
        int vv;
        assert(val.size == sizeof vv);
        memcpy(&vv, val.data, val.size);
        assert(vv == i);
        toku_free(key.data);
        toku_free(val.data);

        /* delete under should succeed */
        error = toku_pma_cursor_delete_under(cursor, &kvsize);
        assert(error == 0);

        /* 2nd delete under should fail */
        error = toku_pma_cursor_delete_under(cursor, &kvsize);
        assert(error == DB_NOTFOUND);
    }
    assert(i == n);

    error = toku_pma_cursor_free(&cursor);
    assert(error == 0);
    assert(toku_pma_n_entries(pma) == 0);

    error = toku_pma_free(&pma);
    assert(error == 0);
}

void test_pma_cursor_set_both() {
    printf("test_pma_cursor_set_both\n");

    int error;
    PMA pma;

    u_int32_t rand4fingerprint = random();
    u_int32_t sum = 0;
    u_int32_t expect_fingerprint = 0;

    const int n = 1000;

    error = toku_pma_create(&pma, toku_default_compare_fun, null_db, null_filenum, n * (8 + sizeof (int) + sizeof (int)));
    assert(error == 0);

    PMA_CURSOR cursor;
    error = toku_pma_cursor(pma, &cursor);
    assert(error == 0);

    DBT key, val;
    int k, v;

    int i;

    /* insert 0->0, 1->1, .. n-1->n-1 */
    for (i=0; i<n; i++) {
        k = htonl(i);
        v = i;
	do_insert(pma, &k, sizeof k, &v, sizeof v, rand4fingerprint, &sum, &expect_fingerprint);
    }

    /* verify key not in pma fails */
    k = n+1; v = 0;
    toku_fill_dbt(&key, &k, sizeof k); 
    toku_fill_dbt(&val, &v, sizeof v);
    error = toku_pma_cursor_set_both(cursor, &key, &val);
    assert(error == DB_NOTFOUND);
    
    /* key match, data mismatch should fail */
    for (i=0; i<n; i++) {
        k = htonl(i);
        v = i+1;
        toku_fill_dbt(&key, &k, sizeof k); 
        toku_fill_dbt(&val, &v, sizeof v);
        error = toku_pma_cursor_set_both(cursor, &key, &val);
        assert(error == DB_NOTFOUND);
    }

    /* key match, data match should succeed */
    for (i=0; i<n; i++) {
        k = htonl(i);
        v = i;
        toku_fill_dbt(&key, &k, sizeof k); 
        toku_fill_dbt(&val, &v, sizeof v);
        error = toku_pma_cursor_set_both(cursor, &key, &val);
        assert(error == 0);

        toku_init_dbt(&key); key.flags = DB_DBT_MALLOC;
        toku_init_dbt(&val); val.flags = DB_DBT_MALLOC;
        error = toku_pma_cursor_get_current(cursor, &key, &val);
        assert(error == 0);
        int vv;
        assert(val.size == sizeof vv);
        memcpy(&vv, val.data, val.size);
        assert(vv == i);
        toku_free(key.data);
        toku_free(val.data);
    }

    error = toku_pma_cursor_free(&cursor);
    assert(error == 0);

    error = toku_pma_free(&pma);
    assert(error == 0);
}

/* insert n duplicate keys */
void test_nodup_key_insert(int n) {
    printf("test_nodup_key_insert:%d\n", n);

    PMA pma;
    int r;

    u_int32_t rand4fingerprint = random();
    u_int32_t sum = 0;
    u_int32_t expect_fingerprint = 0;

    r = toku_pma_create(&pma, toku_default_compare_fun, null_db, null_filenum, n * (8 + sizeof (int) + sizeof (int)));
    assert(r == 0);

    /* insert 0->0, 0->1, .. 0->n-1 */
    DBT key, val;
    int k, v;
    int i;
    for (i=0; i<n; i++) {
        k = htonl(0);
        v = i;
        toku_fill_dbt(&key, &k, sizeof k);
        toku_fill_dbt(&val, &v, sizeof v);
        r = toku_pma_insert(pma, &key, &val, NULL_ARGS, rand4fingerprint, &sum);
        if (i == 0) {
            assert(r == 0);
	    add_fingerprint_and_check(rand4fingerprint, sum, &expect_fingerprint, &k, sizeof k, &v, sizeof v);
	} else {
            assert(r != 0);
	    assert(sum==expect_fingerprint);
	}
    }

    r = toku_pma_free(&pma);
    assert(r == 0);
}

/* insert n duplicate keys */
void test_dup_key_insert(int n) {
    printf("test_dup_key_insert:%d\n", n);

    PMA pma;
    int r;

    u_int32_t rand4fingerprint = random();
    u_int32_t sum = 0;
    u_int32_t expect_fingerprint = 0;

    r = toku_pma_create(&pma, toku_default_compare_fun, null_db, null_filenum, (n + 2) * (8 + sizeof (int) + sizeof (int)));
    assert(r == 0);
    toku_pma_verify(pma);

    r = toku_pma_set_dup_mode(pma, TOKU_DB_DUP);
    assert(r == 0);


    DBT key, val;
    int k, v;

    /* insert 1->1, 3->3 */
    k = htonl(1); v = 1;
    do_insert(pma, &k, sizeof k, &v, sizeof v, rand4fingerprint, &sum, &expect_fingerprint);
    toku_pma_verify(pma);
    k = htonl(3); v = 3;
    do_insert(pma, &k, sizeof k, &v, sizeof v, rand4fingerprint, &sum, &expect_fingerprint);
    toku_pma_verify(pma);

    int i;
    /* insert 2->0, 2->1, .. 2->n-1 */
    for (i=0; i<n; i++) {
        k = htonl(2);
        v = i;
	do_insert(pma, &k, sizeof k, &v, sizeof v, rand4fingerprint, &sum, &expect_fingerprint);
        toku_pma_verify(pma);
    }

    /* cursor walk from key k should find values 0, 1, .. n-1 */
    PMA_CURSOR cursor;
    r = toku_pma_cursor(pma, &cursor);
    assert(r == 0);

    k = htonl(2);
    toku_fill_dbt(&key, &k, sizeof k);
    r = toku_pma_cursor_set_key(cursor, &key);
    if (r != 0) {
        assert(n == 0);
    } else {
        i = 0;
        while (1) {
            toku_init_dbt(&key); key.flags = DB_DBT_MALLOC;
            toku_init_dbt(&val); val.flags = DB_DBT_MALLOC;
            r = toku_pma_cursor_get_current(cursor, &key, &val);
            assert(r == 0);
            int kk;
            assert(key.size == sizeof kk);
            memcpy(&kk, key.data, key.size);
            if (k != kk) {
                toku_free(key.data);
                toku_free(val.data);
                break;
            }
            int vv;
            assert(val.size == sizeof vv);
            memcpy(&vv, val.data, val.size);
            assert(vv == i);
            toku_free(key.data);
            toku_free(val.data);

            i += 1;

            r = toku_pma_cursor_set_position_next(cursor);
            if (r != 0)
                break;
        }
        assert(i == n);
    }

    r = toku_pma_cursor_free(&cursor);
    assert(r == 0);

    r = toku_pma_free(&pma);
    assert(r == 0);
}

/* insert n duplicate keys, delete key, verify all keys are deleted */
void test_dup_key_delete(int n, int mode) {
    printf("test_dup_key_delete:%d %x\n", n, mode);

    PMA pma;
    int r;

    u_int32_t rand4fingerprint = random();
    u_int32_t sum = 0;
    u_int32_t expect_fingerprint = 0;

    r = toku_pma_create(&pma, toku_default_compare_fun, null_db, null_filenum, (n + 2) * (8 + sizeof (int) + sizeof (int)));
    assert(r == 0);
    toku_pma_verify(pma);

    r = toku_pma_set_dup_mode(pma, mode);
    assert(r == 0);

    if (mode & TOKU_DB_DUPSORT) {
        r = toku_pma_set_dup_compare(pma, toku_default_compare_fun);
        assert(r == 0);
    }

    DBT key, val;
    int k, v;

    /* insert 1->1, 3->3 */
    k = htonl(1); v = 1;
    do_insert(pma, &k, sizeof k, &v, sizeof v, rand4fingerprint, &sum, &expect_fingerprint);
    toku_pma_verify(pma);
    k = htonl(3); v = 3;
    do_insert(pma, &k, sizeof k, &v, sizeof v, rand4fingerprint, &sum, &expect_fingerprint);
    toku_pma_verify(pma);

    u_int32_t sum_before_all_the_duplicates = sum;
    int i;
    /* insert 2->0, 2->1, .. 2->n-1 */
    for (i=0; i<n; i++) {
        k = htonl(2);
        v = i;
	do_insert(pma, &k, sizeof k, &v, sizeof v, rand4fingerprint, &sum, &expect_fingerprint);
        toku_pma_verify(pma);
    }

    k = htonl(2);
    r = toku_pma_delete(pma, toku_fill_dbt(&key, &k, sizeof k), rand4fingerprint, &sum, 0);
    if (r != 0) assert(n == 0);
    expect_fingerprint = sum_before_all_the_duplicates;
    assert(sum == expect_fingerprint);
    toku_pma_verify(pma);
    toku_pma_verify_fingerprint(pma, rand4fingerprint, sum);

    /* cursor walk should find keys 1, 3 */
    PMA_CURSOR cursor;
    r = toku_pma_cursor(pma, &cursor);
    assert(r == 0);

    r = toku_pma_cursor_set_position_first(cursor);
    assert(r == 0);

    int kk, vv;

    k = htonl(1); v = 1;
    toku_init_dbt(&key); key.flags = DB_DBT_MALLOC;
    toku_init_dbt(&val); val.flags = DB_DBT_MALLOC;
    r = toku_pma_cursor_get_current(cursor, &key, &val);
    assert(r == 0);
    assert(key.size == sizeof kk);
    memcpy(&kk, key.data, key.size);
    assert(k == kk);
    assert(val.size == sizeof vv);
    memcpy(&vv, val.data, val.size);
    assert(v == vv);
    toku_free(key.data);
    toku_free(val.data);

    r = toku_pma_cursor_set_position_next(cursor);
    assert(r == 0);

    k = htonl(3); v = 3;
    toku_init_dbt(&key); key.flags = DB_DBT_MALLOC;
    toku_init_dbt(&val); val.flags = DB_DBT_MALLOC;
    r = toku_pma_cursor_get_current(cursor, &key, &val);
    assert(r == 0);
    assert(key.size == sizeof kk);
    memcpy(&kk, key.data, key.size);
    assert(k == kk);
    assert(val.size == sizeof vv);
    memcpy(&vv, val.data, val.size);
    assert(v == vv);
    toku_free(key.data);
    toku_free(val.data);

    r = toku_pma_cursor_free(&cursor);
    assert(r == 0);

    r = toku_pma_free(&pma);
    assert(r == 0);
}

/* insert n duplicate keys with random data
   verify that the data is sorted  */
void test_dupsort_key_insert(int n, int dup_data) {
    printf("test_dupsort_key_insert:%d %d\n", n, dup_data);

    PMA pma;
    int r;

    u_int32_t rand4fingerprint = random();
    u_int32_t sum = 0;
    u_int32_t expect_fingerprint = 0;

    r = toku_pma_create(&pma, toku_default_compare_fun, null_db, null_filenum, (n + 2) * (8 + sizeof (int) + sizeof (int)));
    assert(r == 0);
    toku_pma_verify(pma);

    r = toku_pma_set_dup_mode(pma, TOKU_DB_DUP+TOKU_DB_DUPSORT);
    assert(r == 0);

    r = toku_pma_set_dup_compare(pma, toku_default_compare_fun);
    assert(r == 0);

    DBT key, val;
    int k, v;

    /* insert 1->1, 3->3 */
    k = htonl(1); v = 1;
    do_insert(pma, &k, sizeof k, &v, sizeof v, rand4fingerprint, &sum, &expect_fingerprint);
    toku_pma_verify(pma);
    k = htonl(3); v = 3;
    do_insert(pma, &k, sizeof k, &v, sizeof v, rand4fingerprint, &sum, &expect_fingerprint);
    toku_pma_verify(pma);

    k = htonl(2);
    int values[n];
    int i;
    for (i=0; i<n; i++)
        values[i] = (i==0 || dup_data) ? (int) htonl(random()) : values[i-1];

    /* insert 2->n-i */
    for (i=0; i<n; i++) {
	do_insert(pma, &k, sizeof k, &values[i], sizeof values[i], rand4fingerprint, &sum, &expect_fingerprint);
        toku_pma_verify(pma);
    }

    /* cursor walk from key k should find values 0, 1, .. n-1 */
    PMA_CURSOR cursor;
    r = toku_pma_cursor(pma, &cursor);
    assert(r == 0);

    toku_fill_dbt(&key, &k, sizeof k);
    r = toku_pma_cursor_set_key(cursor, &key);
    if (r != 0) {
        assert(n == 0);
    } else {
        int cmpint(const void *a, const void *b) {
            return memcmp(a, b, sizeof (int));
        }
        qsort(values, n, sizeof (int), cmpint);
        i = 0;
        while (1) {
            toku_init_dbt(&key); key.flags = DB_DBT_MALLOC;
            toku_init_dbt(&val); val.flags = DB_DBT_MALLOC;
            r = toku_pma_cursor_get_current(cursor, &key, &val);
            assert(r == 0);
            int kk;
            assert(key.size == sizeof kk);
            memcpy(&kk, key.data, key.size);
            if (k != kk) {
                toku_free(key.data);
                toku_free(val.data);
                break;
            }
            int vv;
            assert(val.size == sizeof vv);
            memcpy(&vv, val.data, val.size);
            assert(vv == values[i]);
            toku_free(key.data);
            toku_free(val.data);

            i += 1;

            r = toku_pma_cursor_set_position_next(cursor);
            if (r != 0)
                break;
        }
        assert(i == n);
    }

    r = toku_pma_cursor_free(&cursor);
    assert(r == 0);

    r = toku_pma_free(&pma);
    assert(r == 0);
}

void test_dup_key_lookup(int n, int mode) {
    printf("test_dup_lookup:%d %d\n", n, mode);

    PMA pma;
    int r;

    u_int32_t rand4fingerprint = random();
    u_int32_t sum = 0;
    u_int32_t expect_fingerprint = 0;

    r = toku_pma_create(&pma, toku_default_compare_fun, null_db, null_filenum, (n + 2) * (8 + sizeof (int) + sizeof (int)));
    assert(r == 0);
    toku_pma_verify(pma);

    r = toku_pma_set_dup_mode(pma, mode);
    assert(r == 0);

    if (mode & TOKU_DB_DUPSORT) {
        r = toku_pma_set_dup_compare(pma, toku_default_compare_fun);
        assert(r == 0);
    }

    DBT key, val;
    int k, v;

    /* insert 1->1, 3->3 */
    k = htonl(1); v = 1;
    do_insert(pma, &k, sizeof k, &v, sizeof v, rand4fingerprint, &sum, &expect_fingerprint);
    toku_pma_verify(pma);
    k = htonl(3); v = 3;
    do_insert(pma, &k, sizeof k, &v, sizeof v, rand4fingerprint, &sum, &expect_fingerprint);
    toku_pma_verify(pma);

    int i;
    /* insert 2->0, 2->1, .. 2->n-1 */
    for (i=0; i<n; i++) {
        k = htonl(2);
        v = htonl(i);
	do_insert(pma, &k, sizeof k, &v, sizeof v, rand4fingerprint, &sum, &expect_fingerprint);
        toku_pma_verify(pma);
    }

    /* lookup should find the first insert and smallest value */
    k = htonl(2);
    r = toku_pma_lookup(pma, toku_fill_dbt(&key, &k, sizeof k), toku_fill_dbt(&val, &v, sizeof v));
    assert(r == 0);
    int kk;
    assert(key.size == sizeof k);
    memcpy(&kk, key.data, key.size);
    assert((unsigned int) kk == htonl(2));
    int vv;
    assert(val.size == sizeof v);
    memcpy(&vv, val.data, val.size);
    assert(vv == 0);

    r = toku_pma_free(&pma);
    assert(r == 0);
}

void test_dup() {
    test_nodup_key_insert(2);                            memory_check_all_free();
    test_dup_key_insert(0);                              memory_check_all_free();
    test_dup_key_insert(2);                              memory_check_all_free();
    test_dup_key_insert(1000);                           memory_check_all_free();
    test_dup_key_delete(0, TOKU_DB_DUP);                 memory_check_all_free();
    test_dup_key_delete(1000, TOKU_DB_DUP);              memory_check_all_free();
    test_dupsort_key_insert(2, 0);                       memory_check_all_free();
    test_dupsort_key_insert(1000, 0);                    memory_check_all_free();
    test_dupsort_key_insert(2, 1);                       memory_check_all_free();
    test_dupsort_key_insert(1000, 1);                    memory_check_all_free();
    test_dup_key_delete(0, TOKU_DB_DUP+TOKU_DB_DUPSORT);           memory_check_all_free();
    test_dup_key_delete(1000, TOKU_DB_DUP+TOKU_DB_DUPSORT);        memory_check_all_free();
    test_dup_key_lookup(32, TOKU_DB_DUP);                          memory_check_all_free();
    test_dup_key_lookup(32, TOKU_DB_DUP+TOKU_DB_DUPSORT);          memory_check_all_free();
}

void pma_tests (void) {
    memory_check=1;
    toku_test_keycompare();            memory_check_all_free();
    test_pma_compare_fun(0);      memory_check_all_free();
    test_pma_compare_fun(1);      memory_check_all_free();
    test_pma_iterate();           
    test_pma_iterate2();          memory_check_all_free();
    test_make_space_at();         memory_check_all_free();
    test_smooth_region();         memory_check_all_free();
    test_find_insert();           memory_check_all_free();
    test_pma_find();              memory_check_all_free();
    test_calculate_parameters();  memory_check_all_free();
    test_count_region();          memory_check_all_free();

    test_pma_random_pick();       memory_check_all_free();
    test_pma_cursor();            memory_check_all_free();

    test_pma_split();             memory_check_all_free();
    test_pma_bulk_insert();       memory_check_all_free();
    test_pma_insert_or_replace(); memory_check_all_free();
    test_pma_delete();
    test_pma_already_there();     memory_check_all_free();
    test_pma_cursor_set_key();    memory_check_all_free();
    test_pma_cursor_set_range();  memory_check_all_free();    
    test_pma_cursor_delete_under();  memory_check_all_free();    
    test_pma_cursor_set_both();   memory_check_all_free();
    test_dup();
}

int main (int argc __attribute__((__unused__)), char *argv[] __attribute__((__unused__))) {
    pma_tests();
    malloc_cleanup();
    return 0;
}
