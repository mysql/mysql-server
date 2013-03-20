#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <tokudb_buffer.h>

static void test_null() {
    tokudb::buffer b;
    assert(b.data() == NULL && b.size() == 0 && b.limit() == 0);
    b.append(NULL, 0);
    assert(b.data() == NULL && b.size() == 0 && b.limit() == 0);
}

static void append_az(tokudb::buffer &b) {
    for (char c = 'a'; c <= 'z'; c++) {
        b.append(&c, sizeof c);
    }
}

static void assert_az(tokudb::buffer &b) {
    unsigned char *bp = NULL;
    for (size_t i = 0; i < b.size(); i++) {
        bp = (unsigned char *) b.data() + i;
        assert(*bp == 'a'+i);    
    }
    assert(*bp == 'z');
}

static void assert_AZ(tokudb::buffer &b) {
    unsigned char *bp = NULL;
    for (size_t i = 0; i < b.size(); i++) {
        bp = (unsigned char *) b.data() + i;
        assert(*bp == 'A'+i);    
    }
    assert(*bp == 'Z');
}

static void test_append() {
    tokudb::buffer a;

    a.append(NULL, 0);
    append_az(a);
    a.append(NULL, 0);
    assert(a.size() == 'z'-'a'+1);
    assert(a.size() <= a.limit());
    assert_az(a);

    tokudb::buffer b(a.data(), 0, a.size());
    for (size_t i = 0; i < b.limit(); i++) {
        assert(i <= a.size());
        char *ap = (char *) a.data() + i;
        assert(i <= b.limit());
        char *bp = (char *) b.data() + i;
        assert(*ap == *bp);
    }
}

static void test_consume() {
    tokudb::buffer a;
    append_az(a);
    tokudb::buffer b(a.data(), 0, a.size());
    for (size_t i = 0; i < b.limit(); i++) {
        unsigned char c;
        b.consume(&c, 1);
        assert(c == 'a'+i);
    }
    assert(b.size() == b.limit());
}

static void test_consume_ptr() {
    tokudb::buffer a;
    append_az(a);
    tokudb::buffer b(a.data(), 0, a.size());
    for (size_t i = 0; i < b.limit(); i++) {
        void *p = b.consume_ptr(1);
        unsigned char c = *(unsigned char *)p;
        assert(c == 'a'+i);
    }
    assert(b.size() == b.limit());
    assert(b.consume_ptr(1) == NULL);
}

static void test_replace() {
    tokudb::buffer a;
    append_az(a);
    assert_az(a);
    for (size_t i = 0; i < a.size(); i++) {
        unsigned char newc[1] = { (unsigned char)('A' + i) };
        a.replace(i, 1, newc, 1);
    }
    assert_AZ(a);
}

static void test_replace_grow() {
    tokudb::buffer a;
    append_az(a);
    assert_az(a);

    // grow field
    size_t orig_s = a.size();
    for (size_t i = 0; i < orig_s; i++) {
        unsigned char newc[2] = { (unsigned char)('a'+i), (unsigned char)('a'+i) };
        size_t old_s = a.size();
        a.replace(2*i, 1, newc, 2);
        assert(a.size() == old_s+1);
    }
    for (size_t i = 0; i < a.size()/2; i++) {
        unsigned char *cp = (unsigned char *) a.data() + 2*i;
        assert(cp[0] == 'a'+i && cp[1] == 'a'+i);
    }
}

static void test_replace_shrink() {
    tokudb::buffer a;
    for (char c = 'a'; c <= 'z'; c++) {
        a.append(&c, sizeof c);
        a.append(&c, sizeof c);
    }

    // shrink field
    for (size_t i = 0; i < a.size(); i++) {
        unsigned char newc[1] = { (unsigned char)('a'+i) };
        size_t s = a.size();
        a.replace(i, 2, newc, 1);
        assert(a.size() == s-1);
    }
    assert_az(a);
}

static void test_replace_null() {
    tokudb::buffer a;
    append_az(a);
    assert_az(a);

    // insert between all
    size_t n = a.size();
    for (size_t i = 0; i < n; i++) {
        unsigned char newc[1] = { (unsigned char)('a'+i) };
        a.replace(2*i, 0, newc, 1);
    }

    a.replace(a.size(), 0, (void *)"!", 1);
    a.append((void *)"?", 1);
}

namespace tokudb {
    template size_t vlq_encode_ui(uint8_t, void *, size_t);
    template size_t vlq_decode_ui(uint8_t *, void *, size_t);
    template size_t vlq_encode_ui(uint32_t, void *, size_t);
    template size_t vlq_decode_ui(uint32_t *, void *, size_t);
};

static void test_ui8() {
    tokudb::buffer a;
    for (uint8_t n = 0; ; n++) {
        assert(a.append_ui<uint8_t>(n) != 0);
        if (n == 255)
            break;
    }
    tokudb::buffer b(a.data(), 0, a.size());
    for (uint8_t n = 0; ; n++) {
        uint8_t v;
        if (b.consume_ui<uint8_t>(&v) == 0)
            break;
        assert(v == n);
        if (n == 255)
            break;
    }
    assert(b.size() == b.limit());
}

static void test_ui32() {
    tokudb::buffer a;
    for (uint32_t n = 0; ; n++) {
        assert(a.append_ui<uint32_t>(n) != 0);
        if (n == 1<<22)
            break;
    }
    tokudb::buffer b(a.data(), 0, a.size());
    for (uint32_t n = 0; ; n++) {
        uint32_t v;
        if (b.consume_ui<uint32_t>(&v) == 0)
            break;
        assert(v == n);
        if (n == 1<<22)
            break;
    }
    assert(b.size() == b.limit());
}

int main() {
    test_null();
    test_append();
    test_consume();
    test_consume_ptr();
    test_replace();
    test_replace_grow();
    test_replace_shrink();
    test_replace_null();
    test_ui8();
    test_ui32();
    return 0;
}
