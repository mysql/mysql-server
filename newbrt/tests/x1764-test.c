#include "includes.h"


#include "test.h"
static void
test0 (void) {
    u_int32_t c = x1764_memory("", 0);
    assert(c==~(0U));
    struct x1764 cs;
    x1764_init(&cs);
    x1764_add(&cs, "", 0);
    c = x1764_finish(&cs);
    assert(c==~(0U));
}

static void
test1 (void) {
    u_int64_t v=0x123456789abcdef0ULL;
    u_int32_t c;
    int i;
    for (i=0; i<=8; i++) {
	u_int64_t expect64 = (i==8) ? v : v&((1LL<<(8*i))-1);
	u_int32_t expect = expect64 ^ (expect64>>32);
	c = x1764_memory(&v, i);
	//printf("i=%d c=%08x expect=%08x\n", i, c, expect);
	assert(c==~expect);
    }
}

// Compute checksums incrementally, using various strides
static void
test2 (void) {
    enum { N=200 };
    char v[N];
    int i;
    for (i=0; i<N; i++) v[i]=(char)random();
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

static void
test3 (void)
// Compare the simple version to the highly optimized verison.
{
    const int datalen = 1000;
    char data[datalen];
    for (int i=0; i<datalen; i++) data[i]=random();
    for (int off=0; off<32; off++) {
	if (verbose) {printf("."); fflush(stdout);}
	for (int len=0; len+off<datalen; len++) {
	    u_int32_t reference_sum = x1764_memory_simple(data+off, len);
	    u_int32_t fast_sum      = x1764_memory       (data+off, len);
	    assert(reference_sum==fast_sum);
	}
    }
}

int
test_main (int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    if (verbose) printf("0\n");
    test0();
    if (verbose) printf("1\n");
    test1();
    if (verbose) printf("2\n");
    test2();
    if (verbose) printf("3\n");
    test3();
    return 0;
}
