/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include "toku_assert.h"

static int is_prime (int n) {
    int i;
    if (n==2) return 1;
    if (n%2==0) return 0;
    for (i=3; i*i<=n; i+=2) {
	if (n%i==0) return 0;
    }
    return 1;
}

#define N_PRIMES 30
static unsigned int primes[N_PRIMES]={0};

int toku_get_prime (unsigned int idx) {
    if (primes[0]==0) {
	int i;
	for (i=0; i<N_PRIMES; i++) {
	    int j;
	    for (j=2<<i; !is_prime(j); j++) {
	    }
	    primes[i]=j;
	}
    }
    toku_assert(idx<N_PRIMES);
    return primes[idx];
}

void toku_test_primes (void) {
    toku_assert(toku_get_prime(0)==2);
    toku_assert(toku_get_prime(1)==5);
    toku_assert(toku_get_prime(2)==11);
    toku_assert(toku_get_prime(3)==17);
    toku_assert(toku_get_prime(4)==37);
    toku_assert(toku_get_prime(5)==67);
    toku_assert(toku_get_prime(6)==131);
}
