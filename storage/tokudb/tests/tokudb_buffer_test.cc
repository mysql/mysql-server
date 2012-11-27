#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
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
    char *bp = NULL;
    for (int i = 0; i < b.size(); i++) {
        bp = (char *) b.data() + i;
        assert(*bp == 'a'+i);    
    }
    assert(*bp == 'z');
}

static void assert_AZ(tokudb::buffer &b) {
    char *bp = NULL;
    for (int i = 0; i < b.size(); i++) {
        bp = (char *) b.data() + i;
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
    for (int i = 0; i < b.limit(); i++) {
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
    for (int i = 0; i < b.limit(); i++) {
        char c;
        b.consume(&c, 1);
        assert(c == 'a'+i);
    }
    assert(b.size() == b.limit());
}

static void test_consume_ptr() {
    tokudb::buffer a;
    append_az(a);
    tokudb::buffer b(a.data(), 0, a.size());
    for (int i = 0; i < b.limit(); i++) {
        void *p = b.consume_ptr(1);
        char c = *(char *)p;
        assert(c == 'a'+i);
    }
    assert(b.size() == b.limit());
    assert(b.consume_ptr(1) == NULL);
}

static void test_replace() {
    tokudb::buffer a;
    append_az(a);
    assert_az(a);
    for (int i = 0; i < a.size(); i++) {
        char newc[1] = { 'A' + i };
        a.replace(i, 1, newc, 1);
    }
    assert_AZ(a);
}

static void test_replace_grow() {
    tokudb::buffer a;
    append_az(a);
    assert_az(a);

    // grow field
    int s = a.size();
    for (int i = 0; i < s; i++) {
        char newc[2] = { 'a'+i, 'a'+i };
        size_t s = a.size();
        a.replace(2*i, 1, newc, 2);
        assert(a.size() == s+1);
    }
    for (int i = 0; i < a.size()/2; i++) {
        char *cp = (char *) a.data() + 2*i;
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
    for (int i = 0; i < a.size(); i++) {
        char newc[1] = { 'a'+i };
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
    int n = a.size();
    for (int i = 0; i < n; i++) {
        char newc[1] = { 'a'+i };
        a.replace(2*i, 0, newc, 1);
    }

    a.replace(a.size(), 0, (void *)"!", 1);
    a.append((void *)"?", 1);
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
    return 0;
}
