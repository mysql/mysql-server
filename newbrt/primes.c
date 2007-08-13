#include <assert.h>

int is_prime (int n) {
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

int get_prime (unsigned int idx) {
    if (primes[0]==0) {
	int i;
	for (i=0; i<N_PRIMES; i++) {
	    int j;
	    for (j=2<<i; !is_prime(j); j++) {
	    }
	    primes[i]=j;
	}
    }
    assert(idx<N_PRIMES);
    return primes[idx];
}

void test_primes (void) {
    assert(get_prime(0)==2);
    assert(get_prime(1)==5);
    assert(get_prime(2)==11);
    assert(get_prime(3)==17);
    assert(get_prime(4)==37);
    assert(get_prime(5)==67);
    assert(get_prime(6)==131);
}
