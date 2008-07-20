/* Bradley's variation of murmur.
 *  1) It operates on 64-bit values at a time.
 *  2) It can be computed increntally.
 */

#include <sys/types.h>
#include <sys/time.h>
#include <stdlib.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <string.h>
#include <assert.h>
#include "zlib.h"

#include "murmur.h"

static inline u_int64_t ntoh64 (u_int64_t v) {
	asm("bswapq %%rax" : "=a" (v) : "a" (v));
	return v;
}
#if 0
u_int64_t ntoh64 (u_int64_t v) {
    return ntohl(v>>32) | (((u_int64_t)ntohl(v&0xffffffff))<<32);
}
#endif

u_int64_t bmurmur (const void *key, int len)  {
    const u_int64_t m = 0xd8e9509a5bd1e995;
    const int r = 32;
    const u_int64_t seed = 0x6511611f3dd3b51a;
    u_int64_t h = seed^len;

    const unsigned char *data = key;

    while (len>=8) {
	u_int64_t k = ntohl(*(u_int64_t*)data);
	k *= m;
	k ^= k>>r;
	k *= m;
	h *= m;
	h ^= k;
	data += 8;
	len  -= 8;
    }
    switch(len) {
    case  7: h ^= ((u_int64_t)data[6]) << (6*8);
    case  6: h ^= ((u_int64_t)data[5])<< (5*8);
    case  5: h ^= ((u_int64_t)data[4])<< (4*8);
    case  4: h ^= ((u_int64_t)data[3])<< (3*8);
    case  3: h ^= ((u_int64_t)data[2])<< (2*8);
    case  2: h ^= ((u_int64_t)data[1])<< (1*8);
    case  1: h ^= ((u_int64_t)data[0])<< (0*8);
    }
    h *= m;

    // Do a few final mixes of the hash to ensure the last few
    // bytes are well-incorporated.

    h ^= h >> 29;
    h *= m;
    h ^= h >> 31;

    return h;
}

// network-order bmurmur
u_int64_t bmurmurN (const void *key, int len)  {
    const u_int64_t m = 0xd8e9509a5bd1e995;
    const int r = 32;
    const u_int64_t seed = 0x6511611f3dd3b51a;
    u_int64_t h = seed^len;

    const unsigned char *data = key;

    while (len>=8) {
	u_int64_t k = ntoh64(*(u_int64_t*)data);
	k *= m;
	k ^= k>>r;
	k *= m;
	h *= m;
	h ^= k;
	data += 8;
	len  -= 8;
    }
    switch(len) {
    case  7: h ^= ((u_int64_t)data[6]) << (6*8);
    case  6: h ^= ((u_int64_t)data[5])<< (5*8);
    case  5: h ^= ((u_int64_t)data[4])<< (4*8);
    case  4: h ^= ((u_int64_t)data[3])<< (3*8);
    case  3: h ^= ((u_int64_t)data[2])<< (2*8);
    case  2: h ^= ((u_int64_t)data[1])<< (1*8);
    case  1: h ^= ((u_int64_t)data[0])<< (0*8);
    }
    h *= m;

    // Do a few final mixes of the hash to ensure the last few
    // bytes are well-incorporated.

    h ^= h >> 29;
    h *= m;
    h ^= h >> 31;

    return h;
}


unsigned int MurmurHash2 ( const void * key, int len, unsigned int seed )
{
	// 'm' and 'r' are mixing constants generated offline.
	// They're not really 'magic', they just happen to work well.

	const unsigned int m = 0x5bd1e995;
	const int r = 24;

	// Initialize the hash to a 'random' value

	unsigned int h = seed ^ 0;

	// Mix 4 bytes at a time into the hash

	const unsigned char * data = (const unsigned char *)key;

	while(len >= 4)
	{
		unsigned int k = *(unsigned int *)data;

		k *= m; 
		k ^= k >> r; 
		k *= m; 
		
		h *= m; 
		h ^= k;

		data += 4;
		len -= 4;
	}
	
	// Handle the last few bytes of the input array

	//printf("%s:%d h=%08x\n", __FILE__, __LINE__, h);
	switch(len)
	{
	case 3: h ^= data[2] << 16;
	case 2: h ^= data[1] << 8;
	case 1: h ^= data[0];
	        h *= m;
	};

	// Do a few final mixes of the hash to ensure the last few
	// bytes are well-incorporated.

	h ^= h >> 29;
	h *= m;
	h ^= h >> 31;
	//printf("after Final=%08x\n", h);

	return h;
} 


int n;
char *buf;
void setup(void) {
    n=1<<20;
    buf=malloc(n);
    int i;
    for (i=0; i<n; i++) {
	buf[i]=random();
    }
}

double tdiff (struct timeval *end, struct timeval *start) {
    return end->tv_sec-start->tv_sec  + 1e-6*(end->tv_usec - start->tv_usec);
}

#define TIMEIT(str,x) ({ struct timeval start,end; \
 gettimeofday(&start, 0); \
 for (j=0; j<10; j++) x; \
 gettimeofday(&end,   0); \
 double t = tdiff(&end, &start); \
 printf("%s t=%9.6f r=%7.1fMB/s", str, t, n*1e-5/t); \
})


int main (int argc __attribute__((__unused__)), char *argv[] __attribute__((__unused__))) {
    int i;
    {
	struct murmur mm;
	u_int32_t ah = murmur_string("abcdefgh", 8);
	murmur_init(&mm);
	for (i=0; i<8; i++) {
	    char a='a'+i;
	    murmur_add(&mm, &a, 1);
	}
	u_int32_t ih = murmur_finish(&mm);
	assert(ih==ah);
    }


    for (i=0; i<8; i++) {
	char v[8];
	memset(v, 0, 8);
	v[i]=1;
	u_int64_t nv = *(u_int64_t*)&v[0];
	u_int64_t hv = ntoh64(nv);
	u_int64_t expect =  (1ULL << (8*(7-i)));
	//printf("nv=%016llx\nhv=%016llx\nE =%016llx\n", (unsigned long long)nv, (unsigned long long) hv, (unsigned long long)expect);
	assert(hv==expect);
    }
    u_int64_t h=0,h2;
    u_int32_t h3;
    for (i=0; i<10; i++) {
	int j;
	setup();
	h=0;
	TIMEIT("bm   ", h+=(h2=bmurmur(buf, n))); printf(" t=%016llx\n", (unsigned long long)h2);
	TIMEIT("bm+1 ", h+=(h2=bmurmur(buf+1, n-1))); printf(" t=%016llx\n", (unsigned long long)h2);
	TIMEIT("m2   ", h+=(h3=MurmurHash2(buf, n, 0x3dd3b51a))); printf(" t=%08x\n", h3);
	h=0; TIMEIT("m2+1 ", h+=(h3=MurmurHash2(buf+1, n-1, 0x3dd3b51a)));   printf(" t=%08x\n", h3);
	h=0; TIMEIT("mm   ", h+=(h3=murmur_string(buf, n)));                 printf(" t=%08x\n", h3);
	h=0; TIMEIT("mm+1 ", h+=(h3=murmur_string(buf+1, n-1)));             printf(" t=%08x\n", h3);
	{
	    struct murmur mm;
	    h=0; TIMEIT("mm(2)", ({murmur_init(&mm); murmur_add(&mm, buf, n/2); murmur_add(&mm, buf, n-n/2); h+=(h2=murmur_finish(&mm));})); printf(" t=%08llx\n", (unsigned long long)h2);
	}
	h=0; TIMEIT("crc  ", h+=(h3=crc32(0L, (Bytef*)buf, n)));             printf(" t=%08x\n", h3);
	h=0; TIMEIT("crc+1", h+=(h3=crc32(0L, (Bytef*)buf+1, n-1)));         printf(" t=%08x\n", h3);
	printf("\n");
    }
    printf("h=%llu\n", (unsigned long long)h);

    printf("M2(0)=%08x\n", MurmurHash2("", 0, 0x3dd3b51a));
    printf("m2(0)=%08x\n", murmur_string("", 0));
    printf("M2(1)=%08x\n", MurmurHash2("a", 1, 0x3dd3b51a));
    printf("m2(1)=%08x\n", murmur_string("a", 1));
    printf("M2(4)=%08x\n", MurmurHash2("abcd", 4, 0x3dd3b51a));
    printf("m2(4)=%08x\n", murmur_string("abcd", 4));

    return 0;
}
