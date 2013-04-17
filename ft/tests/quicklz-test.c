/* -*- mode: C; c-basic-offset: 4 -*- */
// Test quicklz.
// Compare to compress-test which tests the toku compression (which is a composite of quicklz and zlib).
#ident "Copyright (c) 2010 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#include "test.h"
#include "quicklz.h"

static void test_qlz_random_i (int i) {
    if (verbose) printf("i=%d\n", i);

    qlz_state_compress *MALLOC(compress_state);
    qlz_state_decompress *MALLOC(decompress_state);

    char *MALLOC_N(i, m);
    char *MALLOC_N(i, m2);
    for (int j=0; j<i; j++) {
	m[j] = (random()%256)-128;
    }
    int csize_bound = i+400;
    char *MALLOC_N(csize_bound, c);
    memset(compress_state,   0, sizeof(*compress_state));
    memset(decompress_state, 0, sizeof(*decompress_state));
    int s = qlz_compress(m, c, i, compress_state);
    assert(s <= csize_bound);
    int r = qlz_decompress(c, m2, decompress_state);
    assert(r==i);
    assert(memcmp(m, m2, i)==0);

    toku_free(m);
    toku_free(c);
    toku_free(m2);
    toku_free(compress_state);
    toku_free(decompress_state);
}

static void test_qlz_random (void) {
    // quicklz cannot handle i==0.
    for (int i=1; i<100; i++) {
	test_qlz_random_i(i);
    }
    for (int i=64; i<=1024*1024*8; i*=4) {
	test_qlz_random_i(i);
	test_qlz_random_i(i+random()%i);
    }
}

int test_main (int argc, const char *argv[]) {
    default_parse_args(argc, argv);
    
    test_qlz_random();

    return 0;
}
