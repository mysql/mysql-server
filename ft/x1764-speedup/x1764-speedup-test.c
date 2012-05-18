/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id$"
#ident "Copyright (c) 2011 Tokutek Inc.  All rights reserved."

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>

u_int64_t x1764_simple (const u_int64_t *buf, size_t len)
{
    u_int64_t sum=0;
    for (size_t i=0; i<len ;i++) {
	sum = sum*17 + buf[i];
    }
    return sum;
}

u_int64_t x1764_2x (const u_int64_t *buf, size_t len)
{
    assert(len%2==0);
    u_int64_t suma=0, sumb=0;
    for (size_t i=0; i<len ;i+=2) {
	suma = suma*(17L*17L) + buf[i];
	sumb = sumb*(17L*17L) + buf[i+1];
    }
    return suma*17+sumb;
}

u_int64_t x1764_3x (const u_int64_t *buf, size_t len)
{
    assert(len%3==0);
    u_int64_t suma=0, sumb=0, sumc=0;
    for (size_t i=0; i<len ;i+=3) {
	suma = suma*(17LL*17LL*17LL) + buf[i];
	sumb = sumb*(17LL*17LL*17LL) + buf[i+1];
	sumc = sumc*(17LL*17LL*17LL) + buf[i+2];
    }
    u_int64_t r = suma*17L*17L + sumb*17L + sumc;
    return r;
}

u_int64_t x1764_4x (const u_int64_t *buf, size_t len)
{
    assert(len%4==0);
    u_int64_t suma=0, sumb=0, sumc=0, sumd=0;
    for (size_t i=0; i<len ;i+=4) {
	suma = suma*(17LL*17LL*17LL*17LL) + buf[i];
	sumb = sumb*(17LL*17LL*17LL*17LL) + buf[i+1];
	sumc = sumc*(17LL*17LL*17LL*17LL) + buf[i+2];
	sumd = sumd*(17LL*17LL*17LL*17LL) + buf[i+3];
    }
    return suma*17L*17L*17L + sumb*17L*17L + sumc*17L + sumd;

}

float tdiff (struct timeval *start, struct timeval *end) {
    return (end->tv_sec-start->tv_sec) +1e-6*(end->tv_usec - start->tv_usec);
}

int main (int argc, char *argv[]) {
    int size = 1024*1024*4 + 8*4;
    char *data = malloc(size);
    for (int j=0; j<4; j++) {
	struct timeval start,end,end2,end3,end4;
	for (int i=0; i<size; i++) data[i]=i*i+j;
	gettimeofday(&start, 0);
	u_int64_t s = x1764_simple((u_int64_t*)data, size/sizeof(u_int64_t));
	gettimeofday(&end,   0);
	u_int64_t s2 = x1764_2x((u_int64_t*)data, size/sizeof(u_int64_t));
	gettimeofday(&end2,   0);
	u_int64_t s3 = x1764_3x((u_int64_t*)data, size/sizeof(u_int64_t));
	gettimeofday(&end3,   0);
	u_int64_t s4 = x1764_4x((u_int64_t*)data, size/sizeof(u_int64_t));
	gettimeofday(&end4,   0);
	assert(s==s2);
	assert(s==s3);
	assert(s==s4);
	double b1 = tdiff(&start, &end);
	double b2 = tdiff(&end, &end2);
	double b3 = tdiff(&end2, &end3);
	double b4 = tdiff(&end3, &end4);
	printf("s=%016llx t=%.6fs %.6fs (%4.2fx), %.6fs (%4.2fx), %.6fs (%4.2fx) [%5.2f MB/s]\n",
	       (unsigned long long)s,
	       b1, b2, b1/b2, b3, b1/b3, b4, b1/b4, (size/b4)/(1024*1024));
    }
    return 0;
}
