#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <tokudb_vlq.h>

namespace tokudb {
    template size_t vlq_encode_ui(uint32_t n, void *p, size_t s);
    template size_t vlq_decode_ui(uint32_t *np, void *p, size_t s);
    template size_t vlq_encode_ui(uint64_t n, void *p, size_t s);
    template size_t vlq_decode_ui(uint64_t *np, void *p, size_t s);
};

static void test_vlq_uint32_error(void) {
    uint32_t n;
    unsigned char b[5];
    size_t out_s, in_s;

    out_s = tokudb::vlq_encode_ui<uint32_t>(128, b, 0);
    assert(out_s == 0);
    out_s = tokudb::vlq_encode_ui<uint32_t>(128, b, 1);
    assert(out_s == 0);
    out_s = tokudb::vlq_encode_ui<uint32_t>(128, b, 2);
    assert(out_s == 2);
    in_s = tokudb::vlq_decode_ui<uint32_t>(&n, b, 0);
    assert(in_s == 0);
    in_s = tokudb::vlq_decode_ui<uint32_t>(&n, b, 1);
    assert(in_s == 0);
    in_s = tokudb::vlq_decode_ui<uint32_t>(&n, b, 2);
    assert(in_s == 2 && n == 128);
}

static void test_vlq_uint32(void) {
    uint32_t n;
    unsigned char b[5];
    size_t out_s, in_s;

    printf("%u\n", 0);
    for (uint32_t v = 0; v < (1<<7); v++) {
        out_s = tokudb::vlq_encode_ui<uint32_t>(v, b, sizeof b);
        assert(out_s == 1);
        in_s = tokudb::vlq_decode_ui<uint32_t>(&n, b, out_s);
        assert(in_s == 1 && n == v);
    }

    printf("%u\n", 1<<7);
    for (uint32_t v = (1<<7); v < (1<<14); v++) {
        out_s = tokudb::vlq_encode_ui<uint32_t>(v, b, sizeof b);
        assert(out_s == 2);
        in_s = tokudb::vlq_decode_ui<uint32_t>(&n, b, out_s);
        assert(in_s == 2 && n == v);
    }

    printf("%u\n", 1<<14);
    for (uint32_t v = (1<<14); v < (1<<21); v++) {
        out_s = tokudb::vlq_encode_ui<uint32_t>(v, b, sizeof b);
        assert(out_s == 3);
        in_s = tokudb::vlq_decode_ui<uint32_t>(&n, b, out_s);
        assert(in_s == 3 && n == v);
    }

    printf("%u\n", 1<<21);
    for (uint32_t v = (1<<21); v < (1<<28); v++) {
        out_s = tokudb::vlq_encode_ui<uint32_t>(v, b, sizeof b);
        assert(out_s == 4);
        in_s = tokudb::vlq_decode_ui<uint32_t>(&n, b, out_s);
        assert(in_s == 4 && n == v);
    }

    printf("%u\n", 1<<28);
    for (uint32_t v = (1<<28); v != 0; v++) {
        out_s = tokudb::vlq_encode_ui<uint32_t>(v, b, sizeof b);
        assert(out_s == 5);
        in_s = tokudb::vlq_decode_ui<uint32_t>(&n, b, out_s);
        assert(in_s == 5 && n == v);
    }
}

static void test_vlq_uint64(void) {
    uint64_t n;
    unsigned char b[10];
    size_t out_s, in_s;

    printf("%u\n", 0);
    for (uint64_t v = 0; v < (1<<7); v++) {
        out_s = tokudb::vlq_encode_ui<uint64_t>(v, b, sizeof b);
        assert(out_s == 1);
        in_s = tokudb::vlq_decode_ui<uint64_t>(&n, b, out_s);
        assert(in_s == 1 && n == v);
    }

    printf("%u\n", 1<<7);
    for (uint64_t v = (1<<7); v < (1<<14); v++) {
        out_s = tokudb::vlq_encode_ui<uint64_t>(v, b, sizeof b);
        assert(out_s == 2);
        in_s = tokudb::vlq_decode_ui<uint64_t>(&n, b, out_s);
        assert(in_s == 2 && n == v);
    }

    printf("%u\n", 1<<14);
    for (uint64_t v = (1<<14); v < (1<<21); v++) {
        out_s = tokudb::vlq_encode_ui<uint64_t>(v, b, sizeof b);
        assert(out_s == 3);
        in_s = tokudb::vlq_decode_ui<uint64_t>(&n, b, out_s);
        assert(in_s == 3 && n == v);
    }

    printf("%u\n", 1<<21);
    for (uint64_t v = (1<<21); v < (1<<28); v++) {
        out_s = tokudb::vlq_encode_ui<uint64_t>(v, b, sizeof b);
        assert(out_s == 4);
        in_s = tokudb::vlq_decode_ui<uint64_t>(&n, b, out_s);
        assert(in_s == 4 && n == v);
    }

    printf("%u\n", 1<<28);
    for (uint64_t v = (1<<28); v < (1ULL<<35); v++) {
        out_s = tokudb::vlq_encode_ui<uint64_t>(v, b, sizeof b);
        assert(out_s == 5);
        in_s = tokudb::vlq_decode_ui<uint64_t>(&n, b, out_s);
        assert(in_s == 5 && n == v);
    }
}

static void test_80000000(void) {
    uint64_t n;
    unsigned char b[10];
    size_t out_s, in_s;
    uint64_t v = 0x80000000;
    out_s = tokudb::vlq_encode_ui<uint64_t>(v, b, sizeof b);
    assert(out_s == 5);
    in_s = tokudb::vlq_decode_ui<uint64_t>(&n, b, out_s);
    assert(in_s == 5 && n == v);
}

static void test_100000000(void) {
    uint64_t n;
    unsigned char b[10];
    size_t out_s, in_s;
    uint64_t v = 0x100000000;
    out_s = tokudb::vlq_encode_ui<uint64_t>(v, b, sizeof b);
    assert(out_s == 5);
    in_s = tokudb::vlq_decode_ui<uint64_t>(&n, b, out_s);
    assert(in_s == 5 && n == v);
}   

int main(void) {
    if (1) test_vlq_uint32_error();
    if (1) test_80000000();
    if (1) test_100000000();
    if (1) test_vlq_uint32();
    if (1) test_vlq_uint64();
    return 0;
}
