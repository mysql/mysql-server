#include <sys/types.h>
#include <assert.h>
#include <stdio.h>
#include <arpa/inet.h>

#include "murmur.h"

static const u_int32_t m = 0x5bd1e995;
static const int r = 24;
static const u_int32_t seed = 0x3dd3b51a;

static u_int32_t MurmurHash2 ( const void * key, int len)
{
    // 'm' and 'r' are mixing constants generated offline.
    // They're not really 'magic', they just happen to work well.


    // Initialize the hash to a 'random' value

    u_int32_t h = seed;

    // Mix 4 bytes at a time into the hash

    const unsigned char * data = (const unsigned char *)key;

    while(len >= 4)
	{
	    u_int32_t k = *(u_int32_t *)data;

	    k *= m; 
	    k ^= k >> r; 
	    k *= m; 
		
	    h *= m; 
	    h ^= k;

	    data += 4;
	    len -= 4;
	}
	
    // Handle the last few bytes of the input array

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

    return h;
} 

void murmur_init (struct murmur *mm) {
    mm->n_bytes_in_k=0;
    mm->k  =0;
    mm->h = seed;
}

void murmur_add (struct murmur *mm, const void * key, unsigned int len) {
    assert(mm->n_bytes_in_k<4);
    const unsigned char *data = key;
    u_int32_t h = mm->h;
    {
	int n_bytes_in_k =  mm->n_bytes_in_k;
	if (n_bytes_in_k>0) {
	    u_int32_t k = mm->k;
	    while (n_bytes_in_k<4 && len>0) {
		k = (k << 8) | *data;
		n_bytes_in_k++;
		data++;
		len--;
	    }
	    if (n_bytes_in_k==4) {
		//printf(" oldh=%08x k=%08x", h, k);
		k *= m;
		k ^= k >> r;
		k *= m;
		h *= m;
		h ^= k;
		mm->n_bytes_in_k = 0;
		mm->k=0;
		//printf(" h=%08x\n", h);
	    } else {
		assert(len==0);
		mm->n_bytes_in_k = n_bytes_in_k;
		mm->k = k;
		mm->h = h;
		return;
	    }
	}
    }
    // We've used up the partial bytes at the beginning of k.
    assert(mm->n_bytes_in_k==0);
    while (len >= 4) {
	u_int32_t k = ntohl(*(u_int32_t *)data);
	//printf(" oldh=%08x k=%08x", h, k);

	k *= m; 
	k ^= k >> r; 
	k *= m; 
		
	h *= m; 
	h ^= k;

	data += 4;
	len -= 4;
	//printf(" h=%08x\n", h);
    }
    mm->h=h;
    //printf("%s:%d h=%08x\n", __FILE__, __LINE__, h);
    {
	u_int32_t k=0;
	switch (len) {
	case 3: k =  *data << 16;  data++;
	case 2: k |= *data << 8;   data++;
	case 1: k |= *data;
	}
	mm->k = k;
	mm->n_bytes_in_k = len;
	//printf("now extra=%08x (%d bytes) n_bytes=%d\n", mm->k, len, mm->n_bytes_in_k);

    }
}

u_int32_t murmur_finish (struct murmur *mm) {
    u_int32_t h = mm->h;
    if (mm->n_bytes_in_k>0) {
	h ^= mm->k;
	h *= m;
    }
    if (0) {
	// The real murmur function does this extra mixing at the end.  We don't need that for fingerprint.
	h ^= h >> 29;
	h *= m;
	h ^= h >> 31;
    }
    return h;
}
    
void murmur_test_string (void *data, int len) {
    u_int32_t v0 = MurmurHash2(data, len);
    struct murmur mm;
    murmur_init(&mm);
    murmur_add(&mm, data, len);
    u_int32_t v1 = murmur_finish(&mm);
    assert(v0==v1);
}

void murmur_test_string_1_byte_at_a_time (void *data, int len) {
    u_int32_t v0 = MurmurHash2(data, len);
    struct murmur mm;
    murmur_init(&mm);
    int i;
    for (i=0; i<len; i++) {
	murmur_add(&mm, data+i, 1);
    }
    u_int32_t v1 = murmur_finish(&mm);
    assert(v0==v1);
}

u_int32_t murmur_string (void *data, int len) {
    struct murmur mm;
    murmur_init(&mm);
    murmur_add(&mm, data, len);
    return murmur_finish(&mm);
}

#if 0
int main (int argc __attribute__((__unused__)), char *argv[] __attribute__((__unused__))) {
    murmur_test_string("", 0);
    char str[] = "abcdefghijklmnopqrstuvwyz";
    u_int32_t i,j;
    murmur_string(str, sizeof(str));
    for (i=0; i<sizeof(str); i++) {
	for (j=i; j<=i; j++) {
	    murmur_test_string(str+i, j-i); 
	    murmur_test_string_1_byte_at_a_time(str+i, j-i);
	}
    }
    return 0;
}
#endif
