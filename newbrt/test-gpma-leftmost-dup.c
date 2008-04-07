// Find out if the leftmost value is returned when the besselfun returns 0 for more than one thing.

#include "gpma.h"
#include "memory.h"
#include "toku_assert.h"
#include "../include/db.h"

#include <stdio.h>
#include <string.h>


int verbose;

static int compare_strings(u_int32_t alen, void *aval, u_int32_t blen, void *bval, void *extra __attribute__((__unused__))) {
    assert(alen==strlen(aval)+1);
    assert(blen==strlen(bval)+1);
    return strcmp(aval, bval);
}

static int rcall_ok (u_int32_t nitems __attribute__((__unused__)), u_int32_t *froms __attribute__((__unused__)), u_int32_t *tos __attribute__((__unused__)),  struct gitem *items __attribute__((__unused__)), u_int32_t old_N __attribute__((__unused__)), u_int32_t new_N __attribute__((__unused__)), void *extra  __attribute__((__unused__))) {
    return 0;
}

static void lookfor (GPMA pma, u_int32_t strlens, int/*char*/ minc, int /*char*/ maxc, int /*char*/ expectc) {
    // Make a bessel function that returns 0 for anything in the range [minc, maxc] inclusive.
    int zero_for_0_and_1 (u_int32_t dlen, void *dval, void *extra) {
	assert(dlen==strlens);
	assert(extra==0);
	if (((char*)dval)[0]<minc) return -1;
	if (((char*)dval)[0]>maxc) return +1;
	return 0;
    }
    u_int32_t len, idx;
    void *data;
    int r = toku_gpma_lookup_bessel(pma, zero_for_0_and_1, 0, 0, &len, &data, &idx);
    assert(r==0);
    assert(len==strlens);
    //printf("Got %c, expect %c\n", ((char*)data)[0], expectc);
    assert(((char*)data)[0]==expectc);
    }

static void test_leftmost (void) {
    GPMA pma;
    int r = toku_gpma_create(&pma, 0);
    assert(r==0);
    enum { N = 9, strlens=2 };
    char *strings[N];
    int i;
    for (i=0; i<N; i++) {
	assert(N<10); // Or we need to fix our format string
	char str[strlens];
	snprintf(str, strlens, "%d", i);
	strings[i]=strdup(str);
	r = toku_gpma_insert(pma, 1+strlen(strings[i]), strings[i], compare_strings, 0, rcall_ok, strings[i], 0);
	assert(r==0);
    }
    int lo, hi;
    for (lo=0; lo<N; lo++) {
	for (hi=lo; hi<N; hi++) {
	    lookfor(pma, strlens, '0'+lo, '0'+hi, '0'+lo);
	}
    }
    // Other tests go here.  Check when -1 for 0, 0 for 1 and 2, 1 for 3 that we get 1
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
    test_leftmost();
    toku_malloc_cleanup();
    return 0;
}
