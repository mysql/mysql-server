// Black box tester, uses only the public interfaces.

#include "gpma.h"
#include "memory.h"
#include "toku_assert.h"
#include "../include/db.h"

#include <stdio.h>
#include <string.h>


int verbose;

static int count_frees=0;
static void free_callback (u_int32_t len __attribute__((__unused__)), void*freeme, void *extra) {
    assert(extra==(void*)&verbose);
    toku_free(freeme);
    count_frees++;
}

static void test_create_and_free (void) {
    int r;
    GPMA pma;
    r = toku_gpma_create(&pma, 0);
    assert(r==0);
    count_frees=0;
    toku_gpma_free(&pma, free_callback, &verbose);
    assert(count_frees==0);
}

static int compare_strings(u_int32_t alen, void *aval, u_int32_t blen, void *bval, void *extra __attribute__((__unused__))) {
    assert(alen==strlen(aval)+1);
    assert(blen==strlen(bval)+1);
    return strcmp(aval, bval);
}

static int rcall_never (u_int32_t nitems __attribute__((__unused__)), u_int32_t *froms __attribute__((__unused__)), u_int32_t *tos __attribute__((__unused__)), struct gitem *items __attribute__((__unused__)), u_int32_t old_N __attribute__((__unused__)), u_int32_t new_N __attribute__((__unused__)), void *extra  __attribute__((__unused__))) {
    assert(0);
    return 0;
}
static int rcall_ok (u_int32_t nitems __attribute__((__unused__)), u_int32_t *froms __attribute__((__unused__)), u_int32_t *tos __attribute__((__unused__)),  struct gitem *items __attribute__((__unused__)), u_int32_t old_N __attribute__((__unused__)), u_int32_t new_N __attribute__((__unused__)), void *extra  __attribute__((__unused__))) {
    return 0;
}

static void test_insert_A (void) {
    int r;
    GPMA pma;
    r = toku_gpma_create(&pma, 0);
    assert(r==0);

    char *k1,*k2,*k3;

    r = toku_gpma_insert(pma, 6, k1=strdup("hello"),
			 compare_strings, 0,
			 rcall_never, "hello", 0);
    assert(r==0);
    assert(toku_gpma_n_entries(pma)==1);

    r = toku_gpma_insert(pma, 6, k2=strdup("gello"),
			 compare_strings, 0,
			 rcall_ok, "gello", 0);
    assert(r==0);

    r = toku_gpma_insert(pma, 6, k3=strdup("fello"),
			 compare_strings, 0,
			 rcall_ok, "fello", 0);
    assert(r==0);

    void *k;
    r = toku_gpma_insert(pma, 6, k=strdup("fello"),
			 compare_strings, 0,
			 rcall_ok, "fello", 0);
    assert(r==DB_KEYEXIST);
    toku_free(k);

    //printf("size=%d\n", toku_gpma_index_limit(pma));

    u_int32_t resultlen;
    void *resultdata;
    r = toku_gpma_lookup_item(pma, 6, "hello", compare_strings, 0, &resultlen, &resultdata, 0);
    assert(r==0);
    assert(strcmp(resultdata, "hello")==0);
    assert(resultdata==k1);

    r = toku_gpma_lookup_item(pma, 6, "gello", compare_strings, 0, &resultlen, &resultdata, 0);
    assert(r==0);
    assert(strcmp(resultdata, "gello")==0);    
    assert(resultdata==k2);

    u_int32_t idx=999;
    r = toku_gpma_lookup_item(pma, 6, "fello", compare_strings, 0, &resultlen, &resultdata, &idx);
    assert(r==0);
    assert(strcmp(resultdata, "fello")==0);    
    assert(resultdata==k3);
    assert(idx!=999);

    r = toku_gpma_lookup_item(pma, 6, "aello", compare_strings, 0, &resultlen, &resultdata, 0);
    assert(r==DB_NOTFOUND);

    r = toku_gpma_lookup_item(pma, 6, "fillo", compare_strings, 0, &resultlen, &resultdata, 0);
    assert(r==DB_NOTFOUND);


    r = toku_gpma_lookup_item(pma, 6, "gillo", compare_strings, 0, &resultlen, &resultdata, 0);
    assert(r==DB_NOTFOUND);

    r = toku_gpma_lookup_item(pma, 6, "hillo", compare_strings, 0, &resultlen, &resultdata, 0);
    assert(r==DB_NOTFOUND);

    r = toku_gpma_lookup_item(pma, 6, "zello", compare_strings, 0, &resultlen, &resultdata, 0);
    assert(r==DB_NOTFOUND);

    {
	int bes (u_int32_t dlen __attribute__((__unused__)), void *dval, void *extra __attribute__((__unused__))) {
	    return strcmp(dval, "a"); // This will return 1 for everything.  For dir<=0 we'll have DB_NOTFOUND, for dir>0 we'll have "fello"
	}
	r = toku_gpma_lookup_bessel(pma, bes, -1, 0, &resultlen, &resultdata, 0);
	assert(r==DB_NOTFOUND);

	r = toku_gpma_lookup_bessel(pma, bes, 0, 0, &resultlen, &resultdata, 0);
	assert(r==DB_NOTFOUND);

	r = toku_gpma_lookup_bessel(pma, bes, +1, 0, &resultlen, &resultdata, 0);
	assert(r==0);
	assert(strcmp(resultdata, "fello")==0);
    }

    {
	int bes (u_int32_t dlen __attribute__((__unused__)), void *dval, void *extra __attribute__((__unused__))) {
	    return strcmp(dval, "z"); // This will return -1 for everything.  For dir>=0 we'll have DB_NOTFOUND, for dir<0 we'll have "hello"
	}
	r = toku_gpma_lookup_bessel(pma, bes, -1, 0, &resultlen, &resultdata, 0); // find the rightmost thing
	assert(r==0);
	assert(strcmp(resultdata, "hello")==0);

	r = toku_gpma_lookup_bessel(pma, bes, 0, 0, &resultlen, &resultdata, 0);
	assert(r==DB_NOTFOUND);

	r = toku_gpma_lookup_bessel(pma, bes, +1, 0, &resultlen, &resultdata, 0);
	assert(r==DB_NOTFOUND);
    }
    


    count_frees=0;
    toku_gpma_free(&pma, free_callback, &verbose);
    assert(count_frees==3);
}

void test_split_internal (const char *strings[],
			  int expect_n_left,
			  u_int32_t *expect_froms_left,
			  u_int32_t *expect_tos_left,
			  int expect_n_right,
			  u_int32_t *expect_froms_right,
			  u_int32_t *expect_tos_right) {
    GPMA pma1, pma2;
    int r;
    r = toku_gpma_create(&pma1, 0);
    assert(r==0);
    r = toku_gpma_create(&pma2, 0);
    assert(r==0);

    assert(0==toku_gpma_valididx(pma1, toku_gpma_index_limit(pma1))); // because it's off the end of the array
    assert(0==toku_gpma_valididx(pma1, 0)); // because nothing is there
    assert(0!=toku_gpma_get_from_index(pma1, toku_gpma_index_limit(pma1), 0, 0));

    u_int32_t i;
    u_int32_t current_estimate_of_N = toku_gpma_index_limit(pma1);
    //printf("%s:%d N=%d\n", __FILE__, __LINE__, current_estimate_of_N);
    for (i=0; strings[i]; i++) {
	int rcall_a (u_int32_t nitems __attribute__((__unused__)), u_int32_t *froms __attribute__((__unused__)), u_int32_t *tos __attribute__((__unused__)),  struct gitem *items __attribute__((__unused__)), u_int32_t old_N, u_int32_t new_N, void *extra __attribute__((__unused__))) {
	    //printf("%s:%d old_N=%d new_N=%d est=%d\n", __FILE__, __LINE__, old_N, new_N, current_estimate_of_N);
	    assert(old_N==current_estimate_of_N);
	    current_estimate_of_N = new_N;
	    //printf("est=%d\n", current_estimate_of_N);
	    return 0;
	}
	u_int32_t idx, len;
	void *data;
	r = toku_gpma_insert(pma1, 1+strlen(strings[i]), (char*)strings[i], compare_strings, 0, rcall_a, (char*)strings[i], &idx);
	//printf("est=%d\n", current_estimate_of_N);
	assert(r==0);
	r = toku_gpma_get_from_index(pma1, idx, &len, &data);
	assert(r==0);
	assert(len==1+strlen(strings[i]));
	assert(data==strings[i]);
    }
    u_int32_t n_strings = i;
    {
	int do_realloc (u_int32_t len, void *data, void**ndata, void *extra) {
	    assert(extra==0);
	    assert(len=1+strlen(data));
	    *ndata = data; // Don't have to do anything
	    return 0;
	}
	int did_n_left=-1, did_n_right=-1;
	int rcall0 (u_int32_t nitems, u_int32_t *froms, u_int32_t *tos,  struct gitem *items, u_int32_t old_N, u_int32_t new_N, void *extra) {
	    //printf("%s:%d old_N=%d new_N=%d\n", __FILE__, __LINE__, old_N, new_N);
	    assert(old_N==current_estimate_of_N);
	    current_estimate_of_N = new_N;
	    assert(extra==0);
	    u_int32_t j;
	    if (expect_n_left>=0) assert(nitems==(u_int32_t)expect_n_left);
	    did_n_left=nitems;
	    //printf("did_n_left=%d nitems=%d n_strings=%d\n", did_n_left, nitems, n_strings);
	    assert(did_n_left+nitems==n_strings);
	    //printf("inner moved:"); for (j=0; j<nitems; j++) printf(" %d->%d", froms[j], tos[j]); printf("\n");
	    for (j=0; j<nitems; j++) {
		if (expect_froms_left) assert(expect_froms_left[j]==froms[j]);
		if (expect_tos_left)   assert(expect_tos_left  [j]==tos[j]);
		assert(items[j].len==1+strlen(items[j].data));
		if (j>0) {
		    assert(froms[j-1]<froms[j] && tos[j-1]<tos[j]);
		    assert(strcmp(items[j-1].data, items[j].data)<0);
		}
	    }
	    return 0;
	}
	int rcall1 (u_int32_t nitems, u_int32_t *froms, u_int32_t *tos,  struct gitem *items, u_int32_t old_N, u_int32_t new_N __attribute__((__unused__)), void *extra) {
	    assert(old_N==0);
	    //printf("new_N=%d\n", new_N);
	    assert(extra==0);
	    u_int32_t j;
	    if (expect_n_right>=0) assert(nitems==(u_int32_t)expect_n_right);
	    //printf("outer moved:"); for (j=0; j<nitems; j++) printf(" %d->%d", froms[j], tos[j]); printf("\n");
	    for (j=0; j<nitems; j++) {
		if (expect_froms_right) assert(expect_froms_right[j]==froms[j]);
		if (expect_tos_right)   assert(expect_tos_right  [j]==tos[j]);
		assert(items[j].len==1+strlen(items[j].data));
		if (j>0) {
		    assert(froms[j-1]<froms[j] && tos[j-1]<tos[j]);
		    assert(strcmp(items[j-1].data, items[j].data)<0);
		}
	    }
	    did_n_right = nitems;
	    return 0;
	}
	r = toku_gpma_split(pma1, pma2, 1, do_realloc, rcall0, rcall1, 0);
	toku_verify_gpma(pma1);
	toku_verify_gpma(pma2);
	assert (r==0);
	char *prevval=0;
	int foundem_left[]={-1,-1,-1,-1};
	int foundem_right[]={-1,-1,-1,-1};
	GPMA_ITERATE(pma1, idx, vallen,  val,
		     ({
			 assert(toku_gpma_valididx(pma2, idx));
			 if (prevval!=0) assert(strcmp(prevval,val)<0);
			 prevval=val;
			 unsigned int j;
			 for (j=0; j<n_strings; j++) {
			     if (strings[j]==val) { // The strings are EQ
				 assert(foundem_left[j]==-1);
				 foundem_left[j]=idx;
			     }
			 }
		     }));
	GPMA_ITERATE(pma2, idx, vallen,  val,
		     ({
			 assert(toku_gpma_valididx(pma2, idx));
			 if (prevval!=0) assert(strcmp(prevval,val)<0);
			 prevval=val;
			 unsigned int j;
			 for (j=0; j<n_strings; j++) {
			     if (strings[j]==val) { // The strings are EQ
				 assert(foundem_right[j]==-1);
				 foundem_right[j]=idx;
			     }
			 }
		     }));
	{
	    unsigned int j;
	    for (j=0; j<sizeof(strings)/sizeof(*strings); j++) assert(foundem_left[j]>=0 || foundem_right[j]>=0);
	}
    }
    toku_gpma_free(&pma1, 0, 0);
    toku_gpma_free(&pma2, 0, 0);

}

void test_split (void) {
    {
	const char *strings[]={"the", "quick", "brown", "fox", 0};
	u_int32_t expect_froms_l[]={1,3};
	u_int32_t expect_tos_l  []={0,2};
	u_int32_t expect_froms_r[]={5,7};
	u_int32_t expect_tos_r  []={0,2};

	test_split_internal(strings,
			    2,
			    expect_froms_l,
			    expect_tos_l, 
			    2,
			    expect_froms_r,
			    expect_tos_r);
    }
}

int delete_free_callback (u_int32_t slotnum __attribute__((__unused__)),
			   u_int32_t deletelen,
			   void     *deletedata,
			   void     *extra) {
    assert(deletelen==6);
    assert(extra==deletedata);
    //printf("Freeing %s\n", (char*)deletedata);
    toku_free(deletedata);
    return 0;
}

void test_delete_n (int N) {
    GPMA pma;
    int r = toku_gpma_create(&pma, 0);
    assert(r==0);
    int i;
    char *strings[N];
    for (i=0; i<N; i++) {
	char str[6];
	snprintf(str, 6, "%05d", i);
	strings[i]=strdup(str);
	r = toku_gpma_insert(pma, 6, strings[i], compare_strings, 0, rcall_ok, strings[i], 0);
	assert(r==0);
    }
    for (i=0; i<N; i++) {
	int number_of_strings_left = N-i;
	int rval = random()%number_of_strings_left;
	//printf("deleting %s\n", strings[rval]);
	r = toku_gpma_delete_item(pma, 6, strings[rval],
				  compare_strings, 0,
				  delete_free_callback, strings[rval],
				  rcall_ok, 0);
	strings[rval] = strings[number_of_strings_left-1];
    }
    toku_gpma_free(&pma, 0, 0);
}

void test_delete (void) {
    test_delete_n(3);
    test_delete_n(100);
    test_delete_n(300);
}

void test_delete_at (void) {
    GPMA pma;
    int r = toku_gpma_create(&pma, 0);
    assert(r==0);
    int i, j;
    int N=20;
    char *strings[N];
    for (i=0; i<N; i++) {
	char str[6];
	snprintf(str, 6, "%05d", i);
	strings[i]=strdup(str);
	r = toku_gpma_insert(pma, 6, strings[i], compare_strings, 0, rcall_ok, strings[i], 0);
	assert(r==0);
	//printf("insert, N=%d\n", toku_gpma_index_limit(pma));
    }
    u_int32_t max_limit = toku_gpma_index_limit(pma);
    u_int32_t min_limit = max_limit;
    u_int32_t prev_limit = max_limit;
    u_int32_t resultlen, idx;
    void *resultdata;
    for (j=0; j<N; j++) {
	r = toku_gpma_lookup_item(pma, 6, strings[j], compare_strings, 0, &resultlen, &resultdata, &idx);
	assert(r==0);
	assert(resultlen==6);
	assert(0==strcmp(resultdata, strings[j]));
	r = toku_gpma_delete_at_index(pma, idx, 0, 0);
	assert(r==0);
	u_int32_t this_limit = toku_gpma_index_limit(pma);
	if (this_limit<min_limit) min_limit=this_limit;
	assert(this_limit<=prev_limit);
	prev_limit=this_limit;
	//printf("delete, N=%d\n", this_limit);

	for (i=0; i<=j; i++) {
	    r = toku_gpma_lookup_item(pma, 6, strings[i], compare_strings, 0, &resultlen, &resultdata, &idx);
	    assert(r==DB_NOTFOUND);
	}
	for (i=j+1; i<N; i++) {
	    r = toku_gpma_lookup_item(pma, 6, strings[i], compare_strings, 0, &resultlen, &resultdata, &idx);
	    assert(r==0);
	    assert(resultlen==6);
	    assert(0==strcmp(resultdata, strings[i]));
	}
    }
    assert(min_limit<max_limit);
    for (i=0; i<N; i++) toku_free(strings[i]);
    toku_gpma_free(&pma, 0, 0);
}

static int compare_this_string (u_int32_t dlen, void *dval, void *extra) {
    assert(dlen==1+strlen(dval));
    return strcmp(dval, extra);
}

static void test_bes (void) {
    GPMA pma;
    int r = toku_gpma_create(&pma, 0);
    assert(r==0);
    enum { N = 257 };
    char *strings[N];
    int i;
    for (i=0; i<N; i++) {
	char str[4];
	snprintf(str, 4, "%03d", i);
	strings[i]=strdup(str);
	r = toku_gpma_insert(pma, 1+strlen(strings[i]), strings[i], compare_strings, 0, rcall_ok, strings[i], 0);
	assert(r==0);
    }
    for (i=0; i+1<N; i++) {
	u_int32_t len,idx;
	void *data;
	r = toku_gpma_lookup_bessel(pma, compare_this_string, +1, strings[i], &len, &data, &idx);
	assert(r==0);
	assert(len==1+strlen(strings[i+1]));
	assert(data==strings[i+1]);
    }
    for (i=1; i<N; i++) {
	u_int32_t len,idx;
	void *data;
	r = toku_gpma_lookup_bessel(pma, compare_this_string, -1, strings[i], &len, &data, &idx);
	assert(r==0);
	assert(len==1+strlen(strings[i-1]));
	assert(data==strings[i-1]);
    }

    for (i=0; i<N; i++) toku_free(strings[i]);
    toku_gpma_free(&pma, 0, 0);
}

int main (int argc, const char *argv[]) {
    int i;
    for (i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (0 == strcmp(arg, "-v") || 0 == strcmp(arg, "--verbose"))
            verbose = 1;
        else if (0 == strcmp(arg, "-q") || 0 == strcmp(arg, "--quiet"))
            verbose = 0;
    }
    test_create_and_free();
    test_insert_A();
    test_split();
    test_delete();
    test_delete_at();
    test_bes();
    toku_malloc_cleanup();
    return 0;
}
