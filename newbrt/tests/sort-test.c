/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id$"
#ident "Copyright (c) 2007-2011 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "test.h"

#include <stdlib.h>
#include "sort.h"

const int MAX_NUM = 0x0fffffffL;
int MAGIC_EXTRA = 0xd3adb00f;

static int
int_qsort_cmp(const void *va, const void *vb) {
    const int *a = va, *b = vb;
    assert(*a < MAX_NUM);
    assert(*b < MAX_NUM);
    return (*a > *b) - (*a < *b);
}

static int
int_cmp(void *ve, const void *va, const void *vb)
{
    int *e = ve;
    assert(e);
    assert(*e == MAGIC_EXTRA);
    return int_qsort_cmp(va, vb);
}

static void
check_int_array(int a[], int nelts)
{
    assert(a[0] < MAX_NUM);
    for (int i = 1; i < nelts; ++i) {
        assert(a[i] < MAX_NUM);
        assert(a[i-1] <= a[i]);
    }
}

static void
zero_array_test(void)
{
    mergesort_r(NULL, 0, sizeof(int), NULL, int_cmp);
}

static void
dup_array_test(int nelts)
{
    int *MALLOC_N(nelts, a);
    for (int i = 0; i < nelts; ++i) {
        a[i] = 1;
    }
    mergesort_r(a, nelts, sizeof a[0], &MAGIC_EXTRA, int_cmp);
    check_int_array(a, nelts);
    toku_free(a);
}

static void
already_sorted_test(int nelts)
{
    int *MALLOC_N(nelts, a);
    for (int i = 0; i < nelts; ++i) {
        a[i] = i;
    }
    mergesort_r(a, nelts, sizeof a[0], &MAGIC_EXTRA, int_cmp);
    check_int_array(a, nelts);
    toku_free(a);
}

static void
random_array_test(int nelts)
{
    int *MALLOC_N(nelts, a);
    int *MALLOC_N(nelts, b);
    for (int i = 0; i < nelts; ++i) {
        a[i] = rand() % MAX_NUM;
        b[i] = a[i];
    }
    mergesort_r(a, nelts, sizeof a[0], &MAGIC_EXTRA, int_cmp);
    check_int_array(a, nelts);
    qsort(b, nelts, sizeof b[0], int_qsort_cmp);
    for (int i = 0; i < nelts; ++i)
        assert(a[i] == b[i]);
    toku_free(a);
    toku_free(b);
}

static int
uint64_qsort_cmp(const void *va, const void *vb) {
    const uint64_t *a = va, *b = vb;
    return (*a > *b) - (*a < *b);
}

static int
uint64_cmp(void *ve, const void *va, const void *vb)
{
    int *e = ve;
    assert(e);
    assert(*e == MAGIC_EXTRA);
    return uint64_qsort_cmp(va, vb);
}

static void
random_array_test_64(int nelts)
{
    uint64_t *MALLOC_N(nelts, a);
    uint64_t *MALLOC_N(nelts, b);
    for (int i = 0; i < nelts; ++i) {
        a[i] = ((uint64_t)rand() << 32ULL) | rand();
        b[i] = a[i];
    }
    mergesort_r(a, nelts, sizeof a[0], &MAGIC_EXTRA, uint64_cmp);
    qsort(b, nelts, sizeof b[0], uint64_qsort_cmp);
    for (int i = 0; i < nelts; ++i)
        assert(a[i] == b[i]);
    toku_free(a);
    toku_free(b);
}

int
test_main(int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__)))
{
    zero_array_test();
    random_array_test(10);
    random_array_test(1000);
    random_array_test(10001);
    random_array_test(10000000);
    random_array_test_64(10000000);
    dup_array_test(10);
    dup_array_test(1000);
    dup_array_test(10001);
    dup_array_test(10000000);
    already_sorted_test(10);
    already_sorted_test(1000);
    already_sorted_test(10001);
    already_sorted_test(10000000);
    return 0;
}
