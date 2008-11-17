#include "toku_portability.h"
#include <string.h>
#include "brttypes.h"
#include "includes.h"

static void test_leafentry_1 (void) {
    LEAFENTRY l;
    int r;
    u_int32_t msize, dsize;
    r = le_committed(4, "abc", 3, "xy", &msize, &dsize, &l, 0, 0, 0);
    assert(r==0);
    char expect[] = {LE_COMMITTED,
		     0, 0, 0, 4,
		     'a', 'b', 'c', 0,
		     0, 0, 0, 3,
		     'x', 'y', 0};
    assert(sizeof(expect)==msize);
    assert(msize==dsize);
    assert(memcmp(l, expect, msize)==0);
    toku_free(l);
}

static void test_leafentry_2 (void) {
    LEAFENTRY l;
    int r;
    u_int32_t msize, dsize;
    r = le_both(0x0123456789abcdef0LL, 3, "ab", 4, "xyz", 5, "lmno", &msize, &dsize, &l, 0, 0, 0);
    assert(r==0);
    char expect[] = {LE_BOTH,
		     0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0,
		     0, 0, 0, 3, 'a', 'b', 0,
		     0, 0, 0, 4, 'x', 'y', 'z', 0,
		     0, 0, 0, 5, 'l', 'm', 'n', 'o', 0};
    assert(sizeof(expect)==msize);
    assert(msize==dsize);
    assert(memcmp(l, expect, msize)==0);
    toku_free(l);
}

static void test_leafentry_3 (void) {
    LEAFENTRY l;
    int r;
    u_int32_t msize, dsize;
    r = le_provdel(0x0123456789abcdef0LL, 3, "ab", 5, "lmno", &msize, &dsize, &l, 0, 0, 0);
    assert(r==0);
    char expect[] = {LE_PROVDEL,
		     0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0,
		     0, 0, 0, 3, 'a', 'b', 0,
		     0, 0, 0, 5, 'l', 'm', 'n', 'o', 0};
    assert(sizeof(expect)==msize);
    assert(msize==dsize);
    assert(memcmp(l, expect, msize)==0);
    toku_free(l);
}

static void test_leafentry_4 (void) {
    LEAFENTRY l;
    int r;
    u_int32_t msize, dsize;
    r = le_provpair(0x0123456789abcdef0LL, 3, "ab", 5, "lmno", &msize, &dsize, &l, 0, 0, 0);
    assert(r==0);
    char expect[] = {LE_PROVPAIR,
		     0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0,
		     0, 0, 0, 3, 'a', 'b', 0,
		     0, 0, 0, 5, 'l', 'm', 'n', 'o', 0};
    assert(sizeof(expect)==msize);
    assert(msize==dsize);
    assert(memcmp(l, expect, msize)==0);
    toku_free(l);
}

char zeros[1026];

#define n5zeros 0,0,0,0,0
#define n10zeros n5zeros,n5zeros
#define n25zeros n5zeros,n10zeros,n10zeros
#define n75zeros n25zeros,n25zeros,n25zeros
#define n125zeros n75zeros,n25zeros,n25zeros
#define n150zeros n75zeros,n75zeros
#define n300zeros n150zeros,n150zeros
#define n301zeros 0,n300zeros
#define n1025zeros n300zeros,n300zeros,n300zeros,n125zeros
char expect_3long[] = {LE_PROVDEL,
		       0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0,
		       0, 0, 1, 45, n301zeros,
		       0, 0, 4, 1,  n1025zeros};

static void test_leafentry_3long (void) {
    LEAFENTRY l;
    int r;
    u_int32_t msize, dsize;
    r = le_provdel(0x0123456789abcdef0LL, 301, zeros, 1025, zeros, &msize, &dsize, &l, 0, 0, 0);
    assert(r==0);
    assert(sizeof(expect_3long)==msize);
    assert(msize==dsize);
    assert(memcmp(l, expect_3long, msize)==0);
    toku_free(l);
}

int main (int argc __attribute__((__unused__)), char *argv[] __attribute__((__unused__))) {
    test_leafentry_1();
    test_leafentry_2();
    test_leafentry_3();
    test_leafentry_4();
    test_leafentry_3long();
    return 0;
}
