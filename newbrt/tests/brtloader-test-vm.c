/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id: brtloader-test.c 20778 2010-05-28 20:38:42Z yfogel $"
#ident "Copyright (c) 2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "cachetable.h"

/* Test for #2755.  The brtloader is using too much VM. */

static void test_cachetable_reservation (void) {
    CACHETABLE ct;
    long size = 1L<<28;
    {
	int r = toku_create_cachetable(&ct, size, ZERO_LSN, NULL);
	assert(r==0);
    }
    {
	uint64_t r0 = toku_cachetable_reserve_memory(ct, 0.5);
	uint64_t r0_bound = size/2 + size/16;
	uint64_t r1 = toku_cachetable_reserve_memory(ct, 0.5);
	uint64_t r1_bound = r0_bound/2;
	uint64_t r2 = toku_cachetable_reserve_memory(ct, 0.5);
	uint64_t r2_bound = r1_bound/2;
	assert(r0 < r0_bound);
	assert(r1 < r1_bound);
	assert(r2 < r2_bound);
	assert(r1 < r0);
	assert(r2 < r1);
    }
    {
	int r = toku_cachetable_close(&ct);
	assert(r==0);
    }
    
}

int main (int argc __attribute__((__unused__)), char *argv[] __attribute__((__unused__))) {
    test_cachetable_reservation();
    return 0;
}
