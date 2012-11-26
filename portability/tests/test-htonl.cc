/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id: test-pagesize.cc 45903 2012-07-19 13:06:39Z leifwalsh $"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <toku_assert.h>
#include <portability/toku_htonl.h>

static void test32(void) {
    uint32_t k = 0xDEADBEEF;
    uint32_t n_k = toku_htonl(k);
    const int num_bytes = 4;
    for (int i = 0; i < num_bytes; i++) {
        unsigned char *c = (unsigned char *) &k;
        unsigned char *n_c = (unsigned char *) &n_k;
        invariant(c[i] == n_c[(num_bytes - 1) - i]);
    }
}

static void test64(void) {
    uint64_t k = 0xDEADBEEFABCDBADC;
    uint64_t n_k = toku_htonl64(k);
    const int num_bytes = 8;
    for (int i = 0; i < num_bytes; i++) {
        unsigned char *c = (unsigned char *) &k;
        unsigned char *n_c = (unsigned char *) &n_k;
        invariant(c[i] == n_c[(num_bytes - 1)- i]);
    }
}

int main(void) {
    test32();
    test64();
    return 0;
}
