/* Worst-case insert patterns. */

#include "gpma.h"
#include "toku_assert.h"
#include "memory.h"
#include <string.h>
#include <stdio.h>

int verbose;

static int count_frees=0;
static void free_callback (u_int32_t len __attribute__((__unused__)), void*freeme, void *extra) {
    assert(extra==(void*)&verbose);
    toku_free(freeme);
}

static int compare_strings(u_int32_t alen, void *aval, u_int32_t blen, void *bval, void *extra __attribute__((__unused__))) {
    assert(alen==strlen(aval)+1);
    assert(blen==strlen(bval)+1);
    return strcmp(aval, bval);
}

static int rcall_ok (u_int32_t nitems __attribute__((__unused__)), u_int32_t *froms __attribute__((__unused__)), u_int32_t *tos __attribute__((__unused__)),  struct gitem *items __attribute__((__unused__)), u_int32_t old_N __attribute__((__unused__)), u_int32_t new_N __attribute__((__unused__)), void *extra  __attribute__((__unused__))) {
    return 0;
}

static int delete_callback (u_int32_t slotnum __attribute__((__unused__)), u_int32_t len, void *data, void *extra) {
    assert(strlen(data)+1==len);
    assert(strcmp(data, extra)==0);
    toku_free(data);
    return 0;
}

static const int initial_N=1000;
static const int N=100000;
static const int w=6;

static void insert_n (GPMA pma, int n) {
    char buf[w+1];
    int l = snprintf(buf, sizeof(buf), "%0*d", w, n);
    assert(l==w);
    int r = toku_gpma_insert(pma, strlen(buf)+1, strdup(buf), compare_strings, 0, rcall_ok, 0, 0);
    assert(r==0);
}

static void delete_n (GPMA pma, int n) {
    char buf[w+1];
    int l = snprintf(buf, sizeof(buf), "%0*d", w, n);
    assert(l==w);
    int r = toku_gpma_delete_item(pma,
				  strlen(buf)+1, buf,
				  compare_strings, 0,
				  delete_callback, buf,
				  0, 0);
    if (r!=0) printf("deleted %d\n", n);
    assert(r==0);
}

static int inum (int direction, int itemnum) {
    switch (direction) {
    case 1:
	// Insert things from left to right
	return itemnum;
    case -1:
	// Insert things from right to left
	return 2*N-1-itemnum;
    case 0:
	// Insert things at the outer edges
	if (itemnum%2) {
	    return itemnum/2;
	} else {
	    return 2*N-1-itemnum/2;
	}
    default: assert(0); return 0;
    }
}

static void test_worst_insert(int direction) {
    int r;
    GPMA pma;
    r = toku_gpma_create(&pma, 0);
    assert(r==0);
    count_frees=0;
    int i;
    int next_to_insert=0;
    int next_to_delete=0;
    int max_size = 0;
    for (i=0; i<initial_N; i++) {
	insert_n(pma, inum(direction,next_to_insert++));
    }
    for (; i<N; i++) {
	insert_n(pma, inum(direction,next_to_insert++));
	if (i%10==0) continue; // Make the table get slowly larger
	delete_n(pma, inum(direction, next_to_delete++));
    }
    for (; i<2*N; i++) {
	int this_size = toku_gpma_index_limit(pma);
	if (this_size>max_size) max_size=this_size;
	delete_n(pma, inum(direction,next_to_delete++));
	if (i%20==0) continue; // Make the table get slowly smaller
	insert_n(pma, inum(direction,next_to_insert++));
    }
    assert(count_frees==0);
    if (verbose) printf("size=%d max_size=%d\n", toku_gpma_index_limit(pma), max_size);
    toku_gpma_free(&pma, free_callback, &verbose);
}

int main (int argc, const char *argv[]) {
    int i;
    int which = 0;
    for (i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (0 == strcmp(arg, "-v") || 0 == strcmp(arg, "--verbose"))
            verbose = 1;
        else if (0 == strcmp(arg, "-q") || 0 == strcmp(arg, "--quiet"))
            verbose = 0;
	else if (0 == strcmp(arg, "-a"))
	    which = 1;
	else if (0 == strcmp(arg, "-b"))
	    which = 2;	    
	else if (0 == strcmp(arg, "-c"))
	    which = 3;
    }
    if (which==0 || which==1) test_worst_insert(+1);
    if (which==0 || which==2) test_worst_insert(-1);
    if (which==0 || which==3) test_worst_insert( 0);
    return 0;
}
