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

void test_worst_insert_up(void) {
    int r;
    GPMA pma;
    r = toku_gpma_create(&pma, 0);
    assert(r==0);
    count_frees=0;

    int i;
    int initial_N=1000;
    int N=100000;
    int w=6;
    int next_to_delete=0;
    for (i=0; i<initial_N; i++) {
	char buf[w+1];
	snprintf(buf, sizeof(buf), "%0*d", w, i);
	r = toku_gpma_insert(pma, strlen(buf)+1, strdup(buf), compare_strings, 0, rcall_ok, 0, 0);
	assert(r==0);
    }
    for (; i<N; i++) {
	char buf[w+1];
	snprintf(buf, sizeof(buf), "%0*d", w, i);
	r = toku_gpma_insert(pma, strlen(buf)+1, strdup(buf), compare_strings, 0, rcall_ok, 0, 0);
	assert(r==0);
	snprintf(buf, sizeof(buf), "%0*d", w, next_to_delete);
	r = toku_gpma_delete_item(pma,
				  strlen(buf)+1, buf,
				  compare_strings, 0,
				  delete_callback, buf,
				  0, 0);
    }
    toku_gpma_free(&pma, free_callback, &verbose);
    assert(count_frees==0);
}

void test_worst_insert_down(void) {
    int r;
    GPMA pma;
    r = toku_gpma_create(&pma, 0);
    assert(r==0);
    count_frees=0;

    int i;
    int initial_N=1000;
    int N=100000;
    int w=6;
    int next_to_delete=0;
    for (i=0; i<initial_N; i++) {
	char buf[w+1];
	snprintf(buf, sizeof(buf), "%0*d", w, N-1-i);
	r = toku_gpma_insert(pma, strlen(buf)+1, strdup(buf), compare_strings, 0, rcall_ok, 0, 0);
	assert(r==0);
    }
    for (; i<N; i++) {
	char buf[w+1];
	snprintf(buf, sizeof(buf), "%0*d", w, N-1-i);
	r = toku_gpma_insert(pma, strlen(buf)+1, strdup(buf), compare_strings, 0, rcall_ok, 0, 0);
	assert(r==0);
	snprintf(buf, sizeof(buf), "%0*d", w, N-1-next_to_delete);
	r = toku_gpma_delete_item(pma,
				  strlen(buf)+1, buf,
				  compare_strings, 0,
				  delete_callback, buf,
				  0, 0);
	assert(r==0);
	next_to_delete++;
    }
    toku_gpma_free(&pma, free_callback, &verbose);
    assert(count_frees==0);
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
    test_worst_insert_up();
    test_worst_insert_down();
    return 0;
}
