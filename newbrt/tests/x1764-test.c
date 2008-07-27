#include "x1764.h"
#include "toku_assert.h"
#include <stdio.h>
#include <stdlib.h>

void test0 (void) {
    u_int32_t c = x1764_memory("", 0);
    assert(c==0);
    struct x1764 cs;
    x1764_init(&cs);
    x1764_add(&cs, "", 0);
    c = x1764_finish(&cs);
    assert(c==0);
}

void test1 (void) {
    u_int64_t v=0x123456789abcdef0;
    u_int32_t c;
    int i;
    for (i=0; i<=8; i++) {
	u_int64_t expect64 = (i==8) ? v : v&((1LL<<(8*i))-1);
	u_int32_t expect = expect64 ^ (expect64>>32);
	c = x1764_memory(&v, i);
	//printf("i=%d c=%08x expect=%08x\n", i, c, expect);
	assert(c==expect);
    }
}

// Compute checksums incrementally, using various strides
void test2 (void) {
    enum { N=200 };
    char v[N];
    int i;
    for (i=0; i<N; i++) v[i]=random();
    for (i=0; i<N; i++) {
	int j;
	for (j=i; j<=N; j++) {
	    // checksum from i (inclusive to j (exclusive)
	    u_int32_t c = x1764_memory(&v[i], j-i);
	    // Now compute the checksum incrementally with various strides.
	    int stride;
	    for (stride=1; stride<=j-i; stride++) {
		int k;
		struct x1764 s;
		x1764_init(&s);
		for (k=i; k+stride<=j; k+=stride) {
		    x1764_add(&s, &v[k], stride);
		}
		x1764_add(&s, &v[k], j-k);
		u_int32_t c2 = x1764_finish(&s);
		assert(c2==c);
	    }
	    // Now use some random strides.
	    {
		int k=i;
		struct x1764 s;
		x1764_init(&s);
		while (1) {
		    stride=random()%16;
		    if (k+stride>j) break;
		    x1764_add(&s, &v[k], stride);
		    k+=stride;
		}
		x1764_add(&s, &v[k], j-k);
		u_int32_t c2 = x1764_finish(&s);
		assert(c2==c);
	    }
	}
    }
}
int main (int argc __attribute__((__unused__)), char *argv[] __attribute__((__unused__))) {
    test0();
    test1();
    test2();
    return 0;
}
