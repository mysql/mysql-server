#include "brt-internal.h"
#include <assert.h>
#include <string.h>

int keycompare (bytevec key1b, ITEMLEN key1len, bytevec key2b, ITEMLEN key2len) {
    const unsigned char *key1 = key1b;
    const unsigned char *key2 = key2b;
    while (key1len > 0 && key2len > 0) {
	unsigned char b1 = key1[0];
	unsigned char b2 = key2[0];
	if (b1<b2) return -1;
	if (b1>b2) return 1;
	key1len--; key1++;
	key2len--; key2++;
    }
    if (key1len<key2len) return -1;
    if (key1len>key2len) return 1;
    return 0;
}

#if 0
int oldkeycompare (bytevec key1, ITEMLEN key1len, bytevec key2, ITEMLEN key2len) {
    if (key1len==key2len) {
	return memcmp(key1,key2,key1len);
    } else if (key1len<key2len) {
	int r = memcmp(key1,key2,key1len);
	if (r<=0) return -1; /* If the keys are the same up to 1's length, then return -1, since key1 is shorter than key2. */
	else return 1;
    } else {
	return -keycompare(key2,key2len,key1,key1len);
    }
}
#endif

void test_keycompare (void) {
    assert(keycompare("a",1, "a",1)==0);
    assert(keycompare("aa",2, "a",1)>0);
    assert(keycompare("a",1, "aa",2)<0);
    assert(keycompare("b",1, "aa",2)>0);
    assert(keycompare("aa",2, "b",1)<0);
    assert(keycompare("aaaba",5, "aaaba",5)==0);
    assert(keycompare("aaaba",5, "aaaaa",5)>0);
    assert(keycompare("aaaaa",5, "aaaba",5)<0);
    assert(keycompare("aaaaa",3, "aaaba",3)==0);
}

int default_compare_fun (DB *db __attribute__((__unused__)), const DBT *a, const DBT*b) {
    return keycompare(a->data, a->size, b->data, b->size);
}
