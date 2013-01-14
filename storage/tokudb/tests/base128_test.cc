#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <tokudb_base128.h>

int main(void) {
    uint32_t n;
    unsigned char b[5];
    size_t out_s, in_s;

    printf("%u\n", 0);
    for (uint32_t v = 0; v < (1<<7); v++) {
        out_s = tokudb::base128_encode_uint32(v, b, sizeof b);
        assert(out_s == 1);
        in_s = tokudb::base128_decode_uint32(&n, b, out_s);
        assert(in_s == 1 && n == v);
    }

    printf("%u\n", 1<<7);
    for (uint32_t v = (1<<7); v < (1<<14); v++) {
        out_s = tokudb::base128_encode_uint32(v, b, sizeof b);
        assert(out_s == 2);
        in_s = tokudb::base128_decode_uint32(&n, b, out_s);
        assert(in_s == 2 && n == v);
    }

    printf("%u\n", 1<<14);
    for (uint32_t v = (1<<14); v < (1<<21); v++) {
        out_s = tokudb::base128_encode_uint32(v, b, sizeof b);
        assert(out_s == 3);
        in_s = tokudb::base128_decode_uint32(&n, b, out_s);
        assert(in_s == 3 && n == v);
    }

    printf("%u\n", 1<<21);
    for (uint32_t v = (1<<21); v < (1<<28); v++) {
        out_s = tokudb::base128_encode_uint32(v, b, sizeof b);
        assert(out_s == 4);
        in_s = tokudb::base128_decode_uint32(&n, b, out_s);
        assert(in_s == 4 && n == v);
    }

    printf("%u\n", 1<<28);
    for (uint32_t v = (1<<28); v != 0; v++) {
        out_s = tokudb::base128_encode_uint32(v, b, sizeof b);
        assert(out_s == 5);
        in_s = tokudb::base128_decode_uint32(&n, b, out_s);
        assert(in_s == 5 && n == v);
    }

    return 0;
}
