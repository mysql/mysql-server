#include "x1764.h"
#include "toku_assert.h"
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#define PRINT 0

u_int32_t x1764_memory (const void *buf, int len)
{
    const u_int64_t *lbuf=buf;
    u_int64_t c=0;
    while (len>=8) {
	c = c*17 + *lbuf;
	if (PRINT) printf("%d: c=%016" PRIx64 " sum=%016" PRIx64 "\n", __LINE__, *lbuf, c);
	lbuf++;
	len-=8;
    }
    if (len>0) {
	const u_int8_t *cbuf=(u_int8_t*)lbuf;
	int i;
	u_int64_t input=0;
	for (i=0; i<len; i++) {
	    input |= ((u_int64_t)(cbuf[i]))<<(8*i); 
	}
	c = c*17 + input;
    }
    return (c&0xFFFFFFFF) ^ (c>>32);
}

void x1764_init(struct x1764 *l) {
    l->sum=0;
    l->input=0;
    l->n_input_bytes=0;
}
inline void x1764_add (struct x1764 *l, const void *vbuf, int len) {
    if (PRINT) printf("%d: n_input_bytes=%d len=%d\n", __LINE__, l->n_input_bytes, len);
    int n_input_bytes = l->n_input_bytes;
    const unsigned char *cbuf = vbuf;
    // Special case short inputs
    if (len==1) {
	u_int64_t input = l->input | ((u_int64_t)(*cbuf))<<(8*n_input_bytes);
	n_input_bytes++;
	if (n_input_bytes==8) {
	    l->sum = l->sum*17 + input;
	    l->n_input_bytes = 0;
	    l->input = 0;
	} else {
	    l->input = input;
	    l->n_input_bytes = n_input_bytes;
	}
	return;
    } else if (len==2) {
	u_int64_t input = l->input;
	u_int64_t thisv = ((u_int64_t)(*(u_int16_t*)cbuf));
	if (n_input_bytes==7) {
	    l->sum = l->sum*17 + (input | (thisv<<(8*7)));
	    l->input = thisv>>8;
	    l->n_input_bytes = 1;
	} else if (n_input_bytes==6) {
	    l->sum = l->sum*17 + (input | (thisv<<(8*6)));
	    l->input = 0;
	    l->n_input_bytes = 0;
	} else {
	    l->input = input | (thisv<<(8*n_input_bytes));
	    l->n_input_bytes += 2;
	}
	return;
    }

    u_int64_t sum;
    //assert(len>=0);
    if (n_input_bytes) {
	u_int64_t input = l->input;
	if (len>=8) {
	    sum = l->sum;
	    while (len>=8) {
		u_int64_t thisv = *(u_int64_t*)cbuf;
		input |= thisv<<(8*n_input_bytes);
		sum = sum*17 + input;
		if (PRINT) printf("%d: input=%016" PRIx64 " sum=%016" PRIx64 "\n", __LINE__, input, sum);
		input = thisv>>(8*(8-n_input_bytes));
		if (PRINT) printf("%d: input=%016" PRIx64 "\n", __LINE__, input);
		len-=8;
		cbuf+=8;
		// n_input_bytes remains unchanged
		if (PRINT) printf("%d: n_input_bytes=%d len=%d\n", __LINE__, l->n_input_bytes, len);
	    }
	    l->sum = sum;
	}
	if (len>=4) {
	    u_int64_t thisv = *(u_int32_t*)cbuf;
	    if (n_input_bytes<4) {
		input |= thisv<<(8*n_input_bytes);
		if (PRINT) printf("%d: input=%016" PRIx64 "\n", __LINE__, input);
		n_input_bytes+=4;
	    } else {
		input |= thisv<<(8*n_input_bytes);
		l->sum = l->sum*17 + input;
		if (PRINT) printf("%d: input=%016" PRIx64 " sum=%016" PRIx64 "\n", __LINE__, input, l->sum);
		input = thisv>>(8*(8-n_input_bytes));
		n_input_bytes-=4;
		if (PRINT) printf("%d: input=%016" PRIx64 " n_input_bytes=%d\n", __LINE__, input, n_input_bytes);
	    }
	    len-=4;
	    cbuf+=4;
	    if (PRINT) printf("%d: len=%d\n", __LINE__, len);
	}
	//assert(n_input_bytes<=8);
	while (n_input_bytes<8 && len) {
	    input |= ((u_int64_t)(*cbuf))<<(8*n_input_bytes);
	    n_input_bytes++;
	    cbuf++;
	    len--;
	}
	//assert(len>=0);
	if (n_input_bytes<8) {
	    //assert(len==0);
	    l->input = input;
	    l->n_input_bytes = n_input_bytes;
	    if (PRINT) printf("%d: n_input_bytes=%d\n", __LINE__, l->n_input_bytes);
	    return;
	}
	sum = l->sum*17 + input;
    } else {
	//assert(len>=0);
	sum = l->sum;
    }
    //assert(len>=0);
    while (len>=8) {
	sum = sum*17 + *(u_int64_t*)cbuf;
	cbuf+=8;
	len -=8;
    }
    l->sum = sum;
    n_input_bytes = 0;
    u_int64_t input;
    l->n_input_bytes = len;
    // Surprisingly, the loop is the fastest on bradley's laptop. 
    if (1) {
	int i;
	input=0;
	for (i=0; i<len; i++) {
	    input |= ((u_int64_t)(cbuf[i]))<<(8*i);
	}
    } else if (0) {
	switch (len) {
	case 7: input = ((u_int64_t)(*(u_int32_t*)(cbuf))) | (((u_int64_t)(*(u_int16_t*)(cbuf+4)))<<32) | (((u_int64_t)(*(cbuf+4)))<<48); break;
	case 6: input = ((u_int64_t)(*(u_int32_t*)(cbuf))) | (((u_int64_t)(*(u_int16_t*)(cbuf+4)))<<32); break;
	case 5: input = ((u_int64_t)(*(u_int32_t*)(cbuf))) | (((u_int64_t)(*(cbuf+4)))<<32); break;
	case 4: input = ((u_int64_t)(*(u_int32_t*)(cbuf))); break;
	case 3: input = ((u_int64_t)(*(u_int16_t*)(cbuf))) | (((u_int64_t)(*(cbuf+2)))<<16); break;
	case 2: input = ((u_int64_t)(*(u_int16_t*)(cbuf))); break;
	case 1: input = ((u_int64_t)(*cbuf)); break;
	case 0: input = 0;                      break;
	default: abort();
	}
    } else {
	input=0;
	int i=0;
	if (len>=4) { input  = ((u_int64_t)(*(u_int32_t*)(cbuf)));        cbuf+=4; len-=4; i=4;}
	if (len>=2) { input |= ((u_int64_t)(*(u_int16_t*)(cbuf)))<<(i*8); cbuf+=2; len-=2; i+=2; }
	if (len>=1) { input |= ((u_int64_t)(*(u_int8_t *)(cbuf)))<<(i*8); /*cbuf+=1; len-=1; i++;*/ }
    }
    l->input = input;
    if (PRINT) printf("%d: n_input_bytes=%d\n", __LINE__, l->n_input_bytes);
}
u_int32_t x1764_finish (struct x1764 *l) {
    if (PRINT) printf("%d: n_input_bytes=%d\n", __LINE__, l->n_input_bytes);
    int len = l->n_input_bytes;
    if (len>0) {
	l->sum = l->sum*17 + l->input;
    }
    return (l->sum &0xffffffff) ^ (l->sum>>32);
}
