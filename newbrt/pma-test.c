#include "pma-internal.h"
#include "../include/ydb-constants.h"
#include "memory.h"
#include "key.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static void test_make_space_at (void) {
    PMA pma;
    int r=pma_create(&pma);
    assert(r==0);
    assert(pma_n_entries(pma)==0);
    r=pmainternal_make_space_at(pma, 2);
    assert(pma_index_limit(pma)==4);
    assert((unsigned long)pma->pairs[pma_index_limit(pma)].key==0xdeadbeefL);
    print_pma(pma);

    pma->pairs[2].key="A";
    pma->n_pairs_present++;
    r=pmainternal_make_space_at(pma,2);
    printf("Requested space at 2, got space at %d\n", r);
    print_pma(pma);    
    assert(pma->pairs[r].key==0);
    assert((unsigned long)pma->pairs[pma_index_limit(pma)].key==0xdeadbeefL);

    assert(pma_index_limit(pma)==4);
    pma->pairs[0].key="A";
    pma->pairs[1].key="B";
    pma->pairs[2].key=0;
    pma->pairs[3].key=0;
    pma->n_pairs_present=2;
    print_pma(pma);    
    r=pmainternal_make_space_at(pma,0);
    printf("Requested space at 0, got space at %d\n", r);
    print_pma(pma);
    assert((unsigned long)pma->pairs[pma_index_limit(pma)].key==0xdeadbeefL); // make sure it doesn't go off the end.

    assert(pma_index_limit(pma)==8);
    pma->pairs[0].key = "A";
    pma->pairs[1].key = 0;
    pma->pairs[2].key = 0;
    pma->pairs[3].key = 0;
    pma->pairs[4].key = "B";
    pma->pairs[5].key = 0;
    pma->pairs[6].key = 0;
    pma->pairs[7].key = 0;
    pma->n_pairs_present=2;
    print_pma(pma);
    r=pmainternal_make_space_at(pma,5);
    print_pma(pma);
    printf("r=%d\n", r);
    {
	int i;
	for (i=0; i<pma_index_limit(pma); i++) {
	    if (pma->pairs[i].key) {
		assert(i<r);
	    }
	    pma->pairs[i].key=0; // zero it so that we don't mess things up on free
	    pma->pairs[i].val=0;
	}
    }
    r=pma_free(&pma); assert(r==0);
    assert(pma==0);
}

static void test_pma_find (void) {
    PMA pma;
    int i;
    int r;
    const int N = 16;
    MALLOC(pma);
    MALLOC_N(N,pma->pairs);
    // All that is needed to test pma_find is N and pairs.
    pma->N = N;
    for (i=0; i<N; i++) pma->pairs[i].key=0;
    assert(pma_index_limit(pma)==N);
    r=pmainternal_find(pma, "hello", 5);
    assert(r==0);

    pma->pairs[5].key="hello";
    pma->pairs[5].keylen=5;
    assert(pma_index_limit(pma)==N);
    r=pmainternal_find(pma, "hello", 5);
    assert(pma_index_limit(pma)==N);
    assert(r==5);
    r=pmainternal_find(pma, "there", 5);
    assert(r==6);
    r=pmainternal_find(pma, "aaa", 3);
    assert(r==0);

    pma->pairs[N-1].key="there";
    pma->pairs[N-1].keylen=5;
    r=pmainternal_find(pma, "hello", 5);
    assert(r==5);
    r=pmainternal_find(pma, "there", 5);
    assert(r==N-1);
    r=pmainternal_find(pma, "aaa", 3);
    assert(r==0);
    r=pmainternal_find(pma, "hellob", 6);
    assert(r==6);
    r=pmainternal_find(pma, "zzz", 3);
    assert(r==N);
    toku_free(pma->pairs);
    toku_free(pma);
}

void test_smooth_region_N (int N) {
    struct pair pairs[N];
    char *strings[100];
    char string[100];
    int i;
    int len;
    if (N<10) len=1;
    else if (N<100) len=2;
    else len=8;
    for (i=0; i<N; i++) {
	snprintf(string, 10, "%0*d", len, i);
	strings[i] = strdup(string);
    }
    assert(N<30);
    for (i=0; i<(1<<N)-1; i++) {
	int insertat;
	for (insertat=0; insertat<=N; insertat++) {
	    int j;
	    int r;
	    for (j=0; j<N; j++) {
		if ((1<<j)&i) {
		    pairs[j].key = strings[j];
		} else {
		    pairs[j].key = 0;
		}
	    }
	    pmainternal_printpairs(pairs, N); printf(" at %d becomes f", insertat);
	    r = pmainternal_smooth_region(pairs, N, insertat);
	    pmainternal_printpairs(pairs, N); printf(" at %d\n", r);
	    assert(0<=r); assert(r<N);
	    assert(pairs[r].key==0);
	    /* Now verify that things are in the right place:
	     *  everything before r should be smaller than keys[insertat].
	     *  everything after is bigger.
	     *  Also, make sure everything appeared. */
	    {
		int cleari = i;
		for (j=0; j<N; j++) {
		    if (pairs[j].key) {
			int whichkey = atoi(pairs[j].key);
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
}

    
void test_smooth_region6 (void) {
    enum {N=7};
    struct pair pairs[N] = {{.key="A"},{.key="B"},{.key=0},{.key=0},{.key=0},{.key=0},{.key=0}};
    int r = pmainternal_smooth_region(pairs, N, 2);
    printf("{%s %s %s %s %s %s %s} %d\n",
	   (char*)pairs[0].key, (char*)pairs[1].key, (char*)pairs[2].key, (char*)pairs[3].key, (char*)pairs[4].key, (char*)pairs[5].key, (char*)pairs[6].key,
	   r);
}
    

static void test_smooth_region (void) {
    test_smooth_region_N(4);
    test_smooth_region_N(5);
    test_smooth_region6();
}

static void test_calculate_parameters (void) {
    struct pma pma;
    pma.N=4; pmainternal_calculate_parameters(&pma); assert(pma.uplgN==2); assert(pma.densitystep==0.5);
    pma.N=8; pmainternal_calculate_parameters(&pma); assert(pma.uplgN==4); assert(pma.densitystep==0.5);
    
}

static void test_count_region (void) {
    struct pair pairs[4]={{.key=0},{.key=0},{.key=0},{.key=0}};
    assert(pmainternal_count_region(pairs,0,4)==0);
    assert(pmainternal_count_region(pairs,2,4)==0);
    assert(pmainternal_count_region(pairs,0,2)==0);
    pairs[2].key="A";
    assert(pmainternal_count_region(pairs,0,4)==1);
    assert(pmainternal_count_region(pairs,2,4)==1);
    assert(pmainternal_count_region(pairs,0,2)==0);
    assert(pmainternal_count_region(pairs,2,2)==0);
    assert(pmainternal_count_region(pairs,2,3)==1);
    pairs[3].key="B";
    pairs[0].key="a";
    assert(pmainternal_count_region(pairs,0,4)==3);
}

static void test_pma_random_pick (void) {
    PMA pma;
    int r = pma_create(&pma);
    bytevec key,val;
    ITEMLEN keylen,vallen;
    assert(r==0);
    r = pma_random_pick(pma, &key, &keylen, &val, &vallen);
    assert(r==DB_NOTFOUND);
    r = pma_insert(pma, "hello", 6, "there", 6);
    assert(r==BRT_OK);
    r = pma_random_pick(pma, &key, &keylen, &val, &vallen);
    assert(r==0);
    assert(keylen==6); assert(vallen==6);
    assert(strcmp(key,"hello")==0);
    assert(strcmp(val,"there")==0);
    r = pma_delete(pma, "nothello", 9);
    assert(r==DB_NOTFOUND);
    r = pma_delete(pma, "hello", 6);
    assert(r==BRT_OK);

    r = pma_random_pick(pma, &key, &keylen, &val, &vallen);
    assert(r==DB_NOTFOUND);
    
    r = pma_insert(pma, "hello", 6, "there", 6);
    assert(r==BRT_OK);


    r = pma_random_pick(pma, &key, &keylen, &val, &vallen);
    assert(r==0);
    assert(keylen==6); assert(vallen==6);
    assert(strcmp(key,"hello")==0);
    assert(strcmp(val,"there")==0);

    r = pma_insert(pma, "aaa", 4, "athere", 7); assert(r==BRT_OK);
    r = pma_insert(pma, "aab", 4, "bthere", 7); assert(r==BRT_OK);
    r = pma_insert(pma, "aac", 4, "cthere", 7); assert(r==BRT_OK);
    r = pma_insert(pma, "aad", 4, "dthere", 7); assert(r==BRT_OK);
    r = pma_insert(pma, "aae", 4, "ethere", 7); assert(r==BRT_OK);
    r = pma_insert(pma, "aaf", 4, "fthere", 7); assert(r==BRT_OK);
    r = pma_insert(pma, "aag", 4, "gthere", 7); assert(r==BRT_OK);
    r = pma_delete(pma, "aaa", 4);              assert(r==BRT_OK);
    r = pma_delete(pma, "aab", 4);              assert(r==BRT_OK);
    r = pma_delete(pma, "aac", 4);              assert(r==BRT_OK);
    r = pma_delete(pma, "aad", 4);              assert(r==BRT_OK);
    r = pma_delete(pma, "aae", 4);              assert(r==BRT_OK);
    r = pma_delete(pma, "aag", 4);              assert(r==BRT_OK);
    r = pma_delete(pma, "hello", 6);            assert(r==BRT_OK);
   
    r = pma_random_pick(pma, &key, &keylen, &val, &vallen);
    assert(r==0);
    assert(keylen==4); assert(vallen==7);
    assert(strcmp(key,"aaf")==0);
    assert(strcmp(val,"fthere")==0);
    r=pma_free(&pma); assert(r==0);
    assert(pma==0);
}

static void test_find_insert (void) {
    PMA pma;
    int r;
    bytevec dv;
    ITEMLEN dl;
    pma_create(&pma);
    r=pma_lookup(pma, "aaa", 3, &dv, &dl);
    assert(r==DB_NOTFOUND);

    r=pma_insert(pma, "aaa", 3, "aaadata", 7);
    assert(r==BRT_OK);

    dv=0; dl=0;
    r=pma_lookup(pma, "aaa", 3, &dv, &dl);
    assert(r==BRT_OK);
    assert(keycompare(dv,dl,"aaadata", 7)==0);

    r=pma_insert(pma, "bbb", 4, "bbbdata", 8);
    assert(r==BRT_OK);

    r=pma_lookup(pma, "aaa", 3, &dv, &dl);
    assert(r==BRT_OK);
    assert(keycompare(dv,dl,"aaadata", 7)==0);

    r=pma_lookup(pma, "bbb", 4, &dv, &dl);
    assert(r==BRT_OK);
    assert(keycompare(dv,dl,"bbbdata", 8)==0);

    assert((unsigned long)pma->pairs[pma_index_limit(pma)].key==0xdeadbeefL);
    
    r=pma_insert(pma, "00000", 6, "d0", 3);
    assert(r==BRT_OK);

    assert((unsigned long)pma->pairs[pma_index_limit(pma)].key==0xdeadbeefL);

    r=pma_free(&pma); assert(r==0); assert(pma==0);
    pma_create(&pma); assert(pma!=0);

    {
	int i;
	for (i=0; i<100; i++) {
	    char string[10];
	    char dstring[10];
	    snprintf(string,10,"%05d",i);
	    snprintf(dstring,10,"d%d", i);
	    printf("Inserting %d: string=%s dstring=%s\n", i, string, dstring);
	    r=pma_insert(pma, string, strlen(string)+1, dstring, strlen(dstring)+1);
	    assert(r==BRT_OK);
	}
    }
    r=pma_free(&pma); assert(r==0); assert(pma==0);
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
    pma_iterate(pma, do_sum_em, (void*)0xdeadbeefL);
    assert(tpi_k==expected_k);
    assert(tpi_v==expected_v);
}

static void test_pma_iterate (void) {
    PMA pma;
    int r;
    pma_create(&pma);
    r=pma_insert(pma, "42", 3, "-19", 4);
    assert(r==BRT_OK);
    test_pma_iterate_internal(pma, 42, -19);

    r=pma_insert(pma, "12", 3, "-100", 5);
    assert(r==BRT_OK);
    test_pma_iterate_internal(pma, 42+12, -19-100);
    r=pma_free(&pma); assert(r==0); assert(pma==0);
}

static void test_pma_iterate2 (void) {
    PMA pma0,pma1;
    int r;
    int sum=0;
    int n_items=0;
    r=pma_create(&pma0); assert(r==0);
    r=pma_create(&pma1); assert(r==0);
    pma_insert(pma0, "a", 2, "aval", 5);
    pma_insert(pma0, "b", 2, "bval", 5);
    pma_insert(pma1, "x", 2, "xval", 5);
    PMA_ITERATE(pma0,kv __attribute__((__unused__)),kl,dv __attribute__((__unused__)),dl, (n_items++,sum+=kl+dl));
    PMA_ITERATE(pma1,kv __attribute__((__unused__)),kl,dv __attribute__((__unused__)), dl, (n_items++,sum+=kl+dl));
    assert(sum==21);
    assert(n_items==3);
    r=pma_free(&pma0); assert(r==0); assert(pma0==0);
    r=pma_free(&pma1); assert(r==0); assert(pma1==0);
}

/* Check to see if we can create and kill a cursor. */
void test_pma_cursor_0 (void) {
    PMA pma;
    PMA_CURSOR c=0;
    int r;
    r=pma_create(&pma); assert(r==0);
    r=pma_cursor(pma, &c); assert(r==0); assert(c!=0);
    printf("%s:%d\n", __FILE__, __LINE__);
    r=pma_free(&pma);      assert(r!=0); /* didn't deallocate the cursor. */
    printf("%s:%d\n", __FILE__, __LINE__);
    r=pma_cursor_free(&c); assert(r==0);
    printf("%s:%d\n", __FILE__, __LINE__);
    r=pma_free(&pma); assert(r==0); /* did deallocate the cursor. */    
}

/* Make sure we can free the cursors in any order.  There is a doubly linked list of cursors
 * and if we free them in a different order, then different unlinking code is invoked. */
void test_pma_cursor_1 (void) {
    PMA pma;
    PMA_CURSOR c0=0,c1=0,c2=0;
    int r;
    int order;
    for (order=0; order<6; order++) {
	r=pma_create(&pma); assert(r==0);
	r=pma_cursor(pma, &c0); assert(r==0); assert(c0!=0);
	r=pma_cursor(pma, &c1); assert(r==0); assert(c1!=0);
	r=pma_cursor(pma, &c2); assert(r==0); assert(c2!=0);

	r=pma_free(&pma); assert(r!=0);

	if (order<2)      { r=pma_cursor_free(&c0); assert(r==0);  c0=c1; c1=c2; }
	else if (order<4) { r=pma_cursor_free(&c1); assert(r==0);  c1=c2; }
	else 	          { r=pma_cursor_free(&c2); assert(r==0); }

	r=pma_free(&pma); assert(r!=0);

	if (order%2==0) { r=pma_cursor_free(&c0); assert(r==0);  c0=c1; }
	else            { r=pma_cursor_free(&c1); assert(r==0); }
	
	r=pma_free(&pma); assert(r!=0);

	r = pma_cursor_free(&c0); assert(r==0);
	
	r=pma_free(&pma); assert(r==0);
    }
}

void test_pma_cursor_2 (void) {
    PMA pma;
    PMA_CURSOR c=0;
    int r;
    DBT key,val;
    ybt_init(&key); key.flags=DB_DBT_REALLOC;
    ybt_init(&val); val.flags=DB_DBT_REALLOC;
    r=pma_create(&pma); assert(r==0);
    r=pma_cursor(pma, &c); assert(r==0); assert(c!=0);
    r=pma_cursor_set_position_last(c); assert(r==DB_NOTFOUND);
    r=pma_cursor_free(&c); assert(r==0);
    r=pma_free(&pma); assert(r==0);
}

void test_pma_cursor_3 (void) {
    PMA pma;
    PMA_CURSOR c=0;
    int r;
    DBT key,val;
    r=pma_create(&pma); assert(r==0);
    r=pma_insert(pma, "x", 2, "xx", 3); assert(r==BRT_OK);
    r=pma_insert(pma, "m", 2, "mm", 3); assert(r==BRT_OK);
    r=pma_insert(pma, "aa", 3, "a", 2); assert(r==BRT_OK);
    ybt_init(&key); key.flags=DB_DBT_REALLOC;
    ybt_init(&val); val.flags=DB_DBT_REALLOC;
    r=pma_cursor(pma, &c); assert(r==0); assert(c!=0);

    r=pma_cursor_set_position_first(c); assert(r==0);
    r=pma_cget_current(c, &key, &val); assert(r==0);
    assert(key.size=3); assert(memcmp(key.data,"aa",3)==0);
    assert(val.size=2); assert(memcmp(val.data,"a",2)==0);

    r=pma_cursor_set_position_next(c); assert(r==0);
    r=pma_cget_current(c, &key, &val); assert(r==0);
    assert(key.size=2); assert(memcmp(key.data,"m",2)==0);
    assert(val.size=3); assert(memcmp(val.data,"mm",3)==0);
    
    r=pma_cursor_set_position_next(c); assert(r==0);
    r=pma_cget_current(c, &key, &val); assert(r==0);
    assert(key.size=2); assert(memcmp(key.data,"x",2)==0);
    assert(val.size=3); assert(memcmp(val.data,"xx",3)==0);
    
    r=pma_cursor_set_position_next(c); assert(r==DB_NOTFOUND);

    /* After an error, the cursor should still point at the same thing. */
    r=pma_cget_current(c, &key, &val); assert(r==0);
    assert(key.size=2); assert(memcmp(key.data,"x",2)==0);
    assert(val.size=3); assert(memcmp(val.data,"xx",3)==0);


    r=pma_cursor_set_position_next(c); assert(r==DB_NOTFOUND);

    toku_free(key.data);
    toku_free(val.data);

    r=pma_cursor_free(&c); assert(r==0);
    r=pma_free(&pma); assert(r==0);

}

void test_pma_cursor (void) {
    test_pma_cursor_0();
    test_pma_cursor_1();
    test_pma_cursor_2();
    test_pma_cursor_3();
}

void pma_tests (void) {
    memory_check=1;
    test_pma_iterate();           memory_check_all_free();
    test_pma_iterate2();          memory_check_all_free();
    test_make_space_at();         memory_check_all_free();
    test_smooth_region();         memory_check_all_free();
    test_find_insert();           memory_check_all_free();
    test_pma_find();              memory_check_all_free();
    test_calculate_parameters();  memory_check_all_free();
    test_count_region();          memory_check_all_free();
    test_keycompare();            memory_check_all_free();
    test_pma_random_pick();       memory_check_all_free();
    test_pma_cursor();            memory_check_all_free();
}

int main (int argc __attribute__((__unused__)), char *argv[] __attribute__((__unused__))) {
    pma_tests();
    return 0;
}
