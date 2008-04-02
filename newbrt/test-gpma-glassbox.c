// glass box tester looks inside gpma.c
#include "gpma.h"
#include "gpma-internal.h"
#include "memory.h"
#include "toku_assert.h"
#include "../include/db.h"

#include <errno.h>
#include <string.h>

int verbose=0;

static int count_frees=0;
static void free_callback (u_int32_t len __attribute__((__unused__)), void*freeme, void *extra) {
    assert(extra==(void*)&verbose);
    toku_free(freeme);
    count_frees++;
}

static int compare_strings(u_int32_t alen, void *aval, u_int32_t blen, void *bval, void *extra __attribute__((__unused__))) {
    assert(alen==strlen(aval)+1);
    assert(blen==strlen(bval)+1);
    return strcmp(aval, bval);
}
    

static void test_lg (void) {
    assert(toku_lg(1)==0);
    assert(toku_lg(2)==1);
    assert(toku_lg(3)==2);
    assert(toku_lg(4)==2);
    assert(toku_lg(5)==3);
    assert(toku_lg(7)==3);
    assert(toku_lg(8)==3);
    assert(toku_hyperceil(0)==1);
    assert(toku_hyperceil(1)==1);
    assert(toku_hyperceil(2)==2);
    assert(toku_hyperceil(3)==4);
    assert(toku_hyperceil(4)==4);
    assert(toku_hyperceil(5)==8);
    assert(toku_hyperceil(7)==8);
    assert(toku_hyperceil(8)==8);
    assert(toku_max_int(-1,2)==2);
    assert(toku_max_int(2,2)==2);
    assert(toku_max_int(2,3)==3);
    assert(toku_max_int(3,2)==3);
}

static void test_create_sizes (void) {
    GPMA pma;
    int r = toku_gpma_create(&pma, 0);
    assert(r==0);
    toku_gpma_free(&pma, free_callback, &verbose);

    r = toku_gpma_create(&pma, 3);
    assert(r==EINVAL);
}

static void test_create_badmalloc (void) {
    int i;
    // There are two mallocs inside toku_gpma_create.   Make sure that we test the possiblity that either could fail.
    for (i=0; i<2; i++) {
	int killarray[2]={i,-1};
	toku_dead_mallocs=killarray;
	toku_malloc_counter=0;
	
	int r;
	GPMA pma;
	r = toku_gpma_create(&pma, 0);
	assert(r==ENOMEM);
	toku_dead_mallocs=0; // killarray is no longer valid, so get rid of the ref to it.
    }
}

static void test_find_index (void) {
    int r;
    GPMA pma;
    r = toku_gpma_create(&pma, 16);
    assert(r==0);

    assert(toku_gpma_index_limit(pma)==16);

    int found;
    {
	u_int32_t idx;
	idx = toku_gpma_find_index(pma, 6, "hello", compare_strings, 0, &found);
	assert(found==0);
	assert(idx==0);

	void *k;
	toku_gpma_set_at_index(pma, 3, 6, k=toku_strdup("hello"));
	assert(pma->items[3].len = 6);
	assert(pma->items[3].data == k);

	idx = toku_gpma_find_index(pma, 6, "hello", compare_strings, 0, &found);
	assert(found);
	assert(idx==3);

	idx = toku_gpma_find_index(pma, 2, "a", compare_strings, 0, &found);
	assert(!found);
	assert(idx==0);

	idx = toku_gpma_find_index(pma, 2, "z", compare_strings, 0, &found);
	assert(!found);
	assert(idx==4);
    }

    {
	u_int32_t resultlen; void*resultdata;
	int bes (u_int32_t dlen __attribute__((__unused__)), void *dval, void *extra __attribute__((__unused__))) {
	    return strcmp(dval, "a"); // This will return 1 for everything.  For dir<=0 we'll have DB_NOTFOUND, for dir>0 we'll have "fello"
	}
	r = toku_gpma_lookup_bessel(pma, bes, -1, 0, &resultlen, &resultdata, 0);
	assert(r==DB_NOTFOUND);

	r = toku_gpma_lookup_bessel(pma, bes, 0, 0, &resultlen, &resultdata, 0);
	assert(r==DB_NOTFOUND);

	r = toku_gpma_lookup_bessel(pma, bes, +1, 0, &resultlen, &resultdata, 0);
	assert(r==0);
	assert(strcmp(resultdata, "hello")==0);
    }

    {
	u_int32_t resultlen; void*resultdata;
	int bes (u_int32_t dlen __attribute__((__unused__)), void *dval, void *extra __attribute__((__unused__))) {
	    return strcmp(dval, "z"); // This will return -1 for everything.  For dir>=0 we'll have DB_NOTFOUND, for dir<0 we'll have "hello"
	}
	u_int32_t idx;
	r = toku_gpma_lookup_bessel(pma, bes, -1, 0, &resultlen, &resultdata, &idx); // find the rightmost thing
	assert(r==0);
	assert(strcmp(resultdata, "hello")==0);
	{
	    u_int32_t altlen; void*altdata;
	    r = toku_gpma_get_from_index(pma, idx, &altlen, &altdata);
	    assert(r==0);
	    assert(altlen==resultlen);
	    assert(altdata==resultdata);
	}
	

	r = toku_gpma_lookup_bessel(pma, bes, 0, 0, &resultlen, &resultdata, 0);
	assert(r==DB_NOTFOUND);

	r = toku_gpma_lookup_bessel(pma, bes, +1, 0, &resultlen, &resultdata, 0);
	assert(r==DB_NOTFOUND);
    }

    count_frees=0;
    toku_gpma_free(&pma, free_callback, &verbose);
    assert(count_frees==1);
}

struct rcall_0_pair {
    int idx;
    int use_index_case;
};

static int rcall_0 (u_int32_t nitems, u_int32_t *froms, u_int32_t *tos,  struct gitem *items, u_int32_t old_N, u_int32_t new_N, void *extra) {
    assert(old_N==16);
    assert(new_N==16);
    struct rcall_0_pair *p = extra;
    assert(nitems==3);
    u_int32_t i;
    for (i=0; i<3; i++) assert(froms[i]==i);
    for (i=0; i<2; i++) { assert(tos[i]<tos[i+1]); }
    assert(strcmp(items[0].data,"a")==0);
    assert(strcmp(items[1].data,"b")==0);
    switch (p->use_index_case) {
    case 1:
	switch (p->idx) {
	case 0: assert(tos[0]==5); assert(tos[1]==9); assert(tos[2]==13); break;
	case 1: assert(tos[0]==1); assert(tos[1]==9); assert(tos[2]==13); break;
	case 2: assert(tos[0]==1); assert(tos[1]==5); assert(tos[2]==13); break;
	case 3: assert(tos[0]==1); assert(tos[1]==5); assert(tos[2]== 9); break;
	case 4: assert(tos[0]==1); assert(tos[1]==5); assert(tos[2]== 9); break;
	default: assert(0);
	}
	break;
    case 0:
	assert(tos[0]==1); assert(tos[1]==6); assert(tos[2]==11);
	break;
    default: assert(0);
    }
    return 0;
}

static void test_smooth_region (void)  {
    int r;
    GPMA pma;
    
    int use_index_case;
    for (use_index_case = 0; use_index_case<2; use_index_case++) {
	int malloc_failnum;
	for (malloc_failnum=0; malloc_failnum<4; malloc_failnum++) {
	    u_int32_t idx;
	    for (idx=0; idx<4; idx++) {
		r = toku_gpma_create(&pma, 16);
		assert(r==0);
	    
		int j;
		for (j=0; j<3; j++) {
		    char str[]={'a'+j, 0};
		    pma->items[j].len = 2;
		    pma->items[j].data = toku_strdup(str);
		}
	    
		toku_malloc_counter=0;
		int killarray[2]={malloc_failnum,-1};
		if (malloc_failnum<3) {
		    toku_dead_mallocs=killarray;
		}
		u_int32_t newidx;
		struct rcall_0_pair r0 = {idx,use_index_case};
		r = toku_gpma_smooth_region(pma, 0, 16, 3, idx, use_index_case ? &newidx : 0, rcall_0, &r0, pma->N);

		if (malloc_failnum<3) assert(r==ENOMEM); else assert(r==0);

		toku_dead_mallocs=0;

		count_frees=0;
		toku_gpma_free(&pma, free_callback, &verbose);
		assert(count_frees==3);
	    }
	}
    }
}

static int rcall_1 (u_int32_t nitems, u_int32_t *froms, u_int32_t *tos, struct gitem *items, u_int32_t old_N, u_int32_t new_N, void *extra __attribute__((__unused__))) {
    u_int32_t i;
    assert(old_N==8);
    assert(new_N==8);
    for (i=0; i<nitems; i++) assert(froms[i]==i);
    for (i=0; i<nitems-1; i++) { assert(tos[i]<tos[i+1]); }
    assert(tos[0]==3);  assert(tos[1]==6);
    assert(strcmp(items[0].data,"a")==0);
    assert(strcmp(items[1].data,"b")==0);
    return 0;
}

static void test_make_space_at_up (void) {
    int malloc_failnum;
    for (malloc_failnum=0; malloc_failnum<4; malloc_failnum++) {
	int r;
	GPMA pma;
	r = toku_gpma_create(&pma, 8);
	assert(r==0);
	assert(toku_gpma_n_entries(pma)==0);

	int j;
	for (j=0; j<2; j++) {
	    char str[]={'a'+j, 0};
	    pma->items[j].len = 2;
	    pma->items[j].data = toku_strdup(str);
	}
	u_int32_t newidx;
	toku_malloc_counter=0;
	int killarray[2]={malloc_failnum,-1};
	if (malloc_failnum<3) {
	    toku_dead_mallocs=killarray;
	}
	r = toku_make_space_at(pma, 0, &newidx, rcall_1, 0);
	toku_dead_mallocs=0;
	if (malloc_failnum<3) assert(r==ENOMEM);
	else {
	    assert(r==0);
	    assert(newidx==1);
	    assert(strcmp(pma->items[3].data, "a")==0);
	    assert(strcmp(pma->items[6].data, "b")==0);
	}
	count_frees=0;
	toku_gpma_free(&pma, free_callback, &verbose);
	assert(count_frees==2);
    }
}    

static int rcall_2 (u_int32_t nitems, u_int32_t *froms, u_int32_t *tos, struct gitem *items, u_int32_t old_N, u_int32_t new_N, void *extra __attribute__((__unused__))) {
    assert(old_N==8);
    assert(new_N==8);
    assert(nitems==2);
    assert(froms[0]==6);  assert(froms[1]==7);
    assert(tos[0]==1);  assert(tos[1]==6);
    assert(strcmp(items[0].data,"a")==0);
    assert(strcmp(items[1].data,"b")==0);
    return 0;
}

static void test_make_space_at_down (void) {
    int r;
    GPMA pma;
    int size=8;
    r = toku_gpma_create(&pma, size);
    assert(r==0);
    assert(toku_gpma_n_entries(pma)==0);

    int j;
    for (j=0; j<2; j++) {
	char str[]={'a'+j, 0};
	pma->items[size-2+j].len = 2;
	pma->items[size-2+j].data = toku_strdup(str);
    }
    u_int32_t newidx;
    r = toku_make_space_at(pma, 7, &newidx, rcall_2, 0);
    assert(r==0);
    assert(newidx==3);
    assert(strcmp(pma->items[1].data, "a")==0);
    assert(strcmp(pma->items[6].data, "b")==0);

    count_frees=0;
    toku_gpma_free(&pma, free_callback, &verbose);
    assert(count_frees==2);
}    

static int rcall_3 (u_int32_t nitems, u_int32_t *froms, u_int32_t *tos, struct gitem *items, u_int32_t old_N, u_int32_t new_N, void *extra __attribute__((__unused__))) {
    assert(old_N==8);
    assert(new_N==8);
    assert(nitems==2);
    assert(froms[0]==6);  assert(froms[1]==7);
    assert(tos[0]==1);  assert(tos[1]==3);
    assert(strcmp(items[0].data,"a")==0);
    assert(strcmp(items[1].data,"b")==0);
    return 0;
}

static void test_make_space_at_down_end (void) {
    int no_rcall;
    for (no_rcall=0; no_rcall<2; no_rcall++) {
	int r;
	GPMA pma;
	int size=8;
	r = toku_gpma_create(&pma, size);
	assert(r==0);
	assert(toku_gpma_n_entries(pma)==0);

	int j;
	for (j=0; j<2; j++) {
	    char str[]={'a'+j, 0};
	    pma->items[size-2+j].len = 2;
	    pma->items[size-2+j].data = toku_strdup(str);
	}
	u_int32_t newidx;
	r = toku_make_space_at(pma, 8, &newidx, no_rcall ? 0 : rcall_3, 0);
	assert(r==0);
	assert(newidx==6);
	assert(strcmp(pma->items[1].data, "a")==0);
	assert(strcmp(pma->items[3].data, "b")==0);

	count_frees=0;
	toku_gpma_free(&pma, free_callback, &verbose);
	assert(count_frees==2);
    }
}

static int rcall_ok (u_int32_t nitems __attribute__((__unused__)), u_int32_t *froms __attribute__((__unused__)), u_int32_t *tos __attribute__((__unused__)), struct gitem *items __attribute__((__unused__)), u_int32_t old_N __attribute__((__unused__)), u_int32_t new_N __attribute__((__unused__)), void *extra  __attribute__((__unused__))) {
    return 0;
}

static __attribute__((__noreturn__)) int rcall_never (u_int32_t nitems __attribute__((__unused__)), u_int32_t *froms __attribute__((__unused__)), u_int32_t *tos __attribute__((__unused__)), struct gitem *items __attribute__((__unused__)), void *extra  __attribute__((__unused__))) {
    abort();
}

static void test_insert_malloc_fails (void) {
    int malloc_failnum;
    int killarray[2]={-1,-1};
    for (malloc_failnum=0; malloc_failnum<8; malloc_failnum++) {
	toku_dead_mallocs=killarray;
	toku_dead_mallocs[0]=-1;

	int n_inserted=0;
	int r;
	GPMA pma;
	r = toku_gpma_create(&pma, 0);
	assert(r==0);

	toku_malloc_counter=0;
	r = toku_gpma_insert(pma, 6, strdup("hello"),
			     compare_strings, 0, rcall_ok, "hello", 0);
	assert(r==0);
	assert(toku_gpma_n_entries(pma)==1);
	n_inserted++;

	toku_malloc_counter=0;
	if (1<=malloc_failnum && malloc_failnum<5) {
	    toku_dead_mallocs[0]=malloc_failnum-1;
	}
	void *k;
	r = toku_gpma_insert(pma, 6, k=strdup("gello"),
			     compare_strings, 0, rcall_ok, "gello", 0);
	if (1<=malloc_failnum && malloc_failnum<4) {
	    assert(r==ENOMEM);
	    toku_free(k);
	    assert(toku_gpma_n_entries(pma)==1);
	    int countem=0;
	    u_int32_t i;
	    for (i=0; i<pma->N; i++) {
		if (pma->items[i].data) {
		    countem++;
		    assert(strcmp("hello", pma->items[i].data)==0);
		}
	    }
	    assert(countem==1);
	} else {
	    assert(r==0);
	    assert(toku_gpma_n_entries(pma)==2);
	    n_inserted++;

	    r = toku_gpma_insert(pma, 6, k=strdup("fello"),
				 compare_strings, 0, rcall_ok, "fello", 0);
	    assert(pma->N==4);
	    n_inserted++;

	    toku_malloc_counter=0;
	    assert(pma->N==4);
	    if (4<=malloc_failnum && malloc_failnum<8) {
		toku_dead_mallocs=killarray;
		toku_dead_mallocs[0]=malloc_failnum-4;
	    }
	    r = toku_gpma_insert(pma, 6, k=strdup("fellp"),
				 compare_strings, 0, rcall_ok, "fellp", 0);
	    if (4<=malloc_failnum && malloc_failnum<8) {
		assert(r==ENOMEM);
		toku_free(k);
		assert(pma->N==4);
	    } else {
		assert(r==0);
		n_inserted++;
		assert(pma->N==8);
	    }
	}

	count_frees=0;
	toku_gpma_free(&pma, free_callback, &verbose);
	assert(count_frees==n_inserted);
    }
    toku_dead_mallocs=0;
}

static void test_distribute (void) {
    GPMA pma;
    int r = toku_gpma_create(&pma, 16);
    assert(r==0);
    struct gitem items[4] = {{2,"a"},{2,"b"},{2,"c"},{2,"d"}};
    u_int32_t tos[4];
    toku_gpma_distribute(pma, 0, 16, 4, items, tos);
    toku_gpma_free(&pma, 0, 0);
}

static int rcall_4a (u_int32_t nitems, u_int32_t *froms, u_int32_t *tos, struct gitem *items, u_int32_t old_N, u_int32_t new_N, void *extra __attribute__((__unused__))) {
    assert(old_N==16);
    assert(new_N==8);
    assert(nitems==3);
    assert(froms[0]==0); assert(tos[0]==0);
    assert(froms[1]==1); assert(tos[1]==3);
    assert(froms[2]==2); assert(tos[2]==6);
    assert(strcmp(items[0].data,"a")==0);
    assert(strcmp(items[1].data,"b")==0);
    assert(strcmp(items[2].data,"c")==0);
    return 0;
}
static int rcall_4b (u_int32_t nitems, u_int32_t *froms, u_int32_t *tos, struct gitem *items, u_int32_t old_N, u_int32_t new_N, void *extra __attribute__((__unused__))) {
    assert(old_N==8);
    assert(new_N==8);
    assert(nitems==3);
    assert(froms[0]==1); assert(tos[0]==1);
    assert(froms[1]==3); assert(tos[1]==3);
    assert(froms[2]==6); assert(tos[2]==6);
    assert(strcmp(items[0].data,"a")==0);
    assert(strcmp(items[1].data,"b")==0);
    assert(strcmp(items[2].data,"c")==0);
    return 0;
}

static void test_smooth_deleted (void) {
    GPMA pma;
    int r = toku_gpma_create(&pma, 16);
    assert(r==0);
    pma->items[0] = (struct gitem){2, "a"};
    pma->items[1] = (struct gitem){2, "b"};
    pma->items[2] = (struct gitem){2, "c"};
    pma->n_items_present=3;
    r = toku_smooth_deleted_region(pma, 3, 3, rcall_4a, 0);
    assert(r==0);
    r = toku_smooth_deleted_region(pma, 4, 4, rcall_4b, 0);
    assert(r==0);
    toku_gpma_free(&pma, 0, 0);

    r = toku_gpma_create(&pma, 16);
    assert(r==0);
    pma->items[4] = (struct gitem){2, "a"};
    pma->n_items_present = 1;
    r = toku_smooth_deleted_region(pma, 15, 15, rcall_ok, 0);
    assert(pma->N==8);
    int i;
    for (i=0; i<8; i++) {
	if (i==0)  assert(pma->items[i].data && 0==strcmp(pma->items[i].data,"a"));
	else       assert(!pma->items[i].data);
    }
    toku_gpma_free(&pma, 0, 0);

    r = toku_gpma_create(&pma, 16);
    assert(r==0);
    pma->items[7] = (struct gitem){2, "a"};
    pma->n_items_present = 1;
    r = toku_smooth_deleted_region(pma, 0, 0, rcall_ok, 0);
    assert(pma->N==8);
    for (i=0; i<8; i++) {
	if (i==0) assert(pma->items[i].data && 0==strcmp(pma->items[i].data,"a"));
	else      assert(!pma->items[i].data);
    }
    toku_gpma_free(&pma, 0, 0);

    r = toku_gpma_create(&pma, 32);
    assert(r==0);
    r = toku_smooth_deleted_region(pma, 6, 12, 0, 0);
    assert(r==0);
    toku_gpma_free(&pma, 0, 0);
}

int bes_first (u_int32_t dlen, void *dval, void *extra) {
    assert(dlen==2);
    assert(extra==0);
    char *val=dval;
    if (val[0]=='a') return -1;
    else return 1;
}

// Test looking up something with direction = -1, where every element in the array return +1, except for the first
// So we are supposed to return the largest index that has negative value, which is index 0.
static void test_lookup_first (void) {
    GPMA pma;
    int r = toku_gpma_create(&pma, 4);
    assert(r==0);
    pma->items[0] = (struct gitem){2, "a"};
    pma->items[1] = (struct gitem){2, "b"};
    pma->items[2] = (struct gitem){2, "c"};
    pma->items[3] = (struct gitem){2, "d"};
    pma->n_items_present = 3;
    int found;
    u_int32_t idx = toku_gpma_find_index_bes(pma, bes_first, -1, 0, &found);
    // We expect the answer to be found, and we expect index to be 0.
    assert(found);
    assert(idx==0);
    toku_gpma_free(&pma, 0, 0);
}

int bes_last (u_int32_t dlen, void *dval, void *extra) {
    assert(dlen==2);
    assert(extra==0);
    char *val=dval;
    if (val[0]=='d') return 1;
    else return -1;
}

static void test_lookup_last (void) {
    GPMA pma;
    int r = toku_gpma_create(&pma, 4);
    assert(r==0);
    pma->items[0] = (struct gitem){2, "a"};
    pma->items[1] = (struct gitem){2, "b"};
    pma->items[2] = (struct gitem){2, "c"};
    pma->items[3] = (struct gitem){2, "d"};
    pma->n_items_present = 3;
    int found;
    u_int32_t idx = toku_gpma_find_index_bes(pma, bes_first, +1, 0, &found);
    // We expect the answer to be found, and we expect index to be 1.
    assert(found);
    assert(idx==1);
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
    test_lg();
    test_create_sizes();
    test_create_badmalloc();
    test_find_index();
    test_smooth_region();
    test_make_space_at_up();
    test_make_space_at_down();
    test_make_space_at_down_end();
    test_insert_malloc_fails();
    test_distribute();
    toku_malloc_cleanup();
    test_smooth_deleted();
    test_lookup_last();
    test_lookup_first();
    return 0;
}
