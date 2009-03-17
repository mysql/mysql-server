//rand_s requires _CRT_RAND_S be defined before including stdlib
#define _CRT_RAND_S

#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <toku_stdlib.h>
#include <windows.h>

static int used_srand = 0;

long int
random(void) {
    u_int32_t r;
    if (used_srand) {
        //rand is a relatively poor number generator, but can be seeded
        //rand generates 15 bits.  We need 3 calls to generate 31 bits.
        u_int32_t r1 = rand() & ((1<<15)-1);
        u_int32_t r2 = rand() & ((1<<15)-1);;
        u_int32_t r3 = rand() & 0x1;
        r = r1 | (r2<<15) | (r3<<30);
    }
    else {
        //rand_s is a good number generator, but cannot be seeded (for
        //repeatability).
        errno_t r_error = rand_s(&r);
        assert(r_error==0);
        //Should return 0 to 2**31-1 instead of 2**32-1
        r >>= 1;
    }
    return r;
}

//TODO: Implement srandom to modify the way rand_s works (IF POSSIBLE).. or
//reimplement random.
void
srandom(unsigned int seed) {
    srand(seed);
    used_srand = 1;
}

