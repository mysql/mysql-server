/* -*- mode: C; c-basic-offset: 4 -*- */
// Test quicklz.
// Compare to compress-test which tests the toku compression (which is a composite of quicklz and zlib).
#ident "Copyright (c) 2010 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#include "test.h"
#include "compress.h"

static void test_compress_buf_method (unsigned char *buf, int i, enum toku_compression_method m) {
    int bound = toku_compress_bound(m, i);
    unsigned char *MALLOC_N(bound, cb);
    uLongf actual_clen = bound;
    toku_compress(m, cb, &actual_clen, buf, i);
    unsigned char *MALLOC_N(i, ubuf);
    toku_decompress(ubuf, i, cb, actual_clen);
    assert(0==memcmp(ubuf, buf, i));
    toku_free(ubuf);
    toku_free(cb);
}

static void test_compress_buf (unsigned char *buf, int i) {
    test_compress_buf_method(buf, i, TOKU_ZLIB_METHOD);
    test_compress_buf_method(buf, i, TOKU_QUICKLZ_METHOD);
}

static void test_compress_i (int i) {
    unsigned char *MALLOC_N(i, b);
    for (int j=0; j<i; j++) b[j] = random()%256;
    test_compress_buf (b, i);
    for (int j=0; j<i; j++) b[j] = 0;
    test_compress_buf (b, i);
    for (int j=0; j<i; j++) b[j] = 0xFF;
    test_compress_buf (b, i);
    toku_free(b);
}

static void test_compress (void) {
    // unlike quicklz, we can handle length 0.
    for (int i=0; i<100; i++) {
	test_compress_i(i);
    }
    test_compress_i(1024);
    test_compress_i(1024*1024*4);
    test_compress_i(1024*1024*4 - 123); // just some random lengths
}

int test_main (int argc, const char *argv[]) {
    default_parse_args(argc, argv);
    
    test_compress();

    return 0;
}

