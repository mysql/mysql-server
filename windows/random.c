//rand_s requires _CRT_RAND_S be defined before including stdlib
#define _CRT_RAND_S

#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <toku_stdlib.h>
#include <windows.h>

long int
random(void) {
    u_int32_t r;
    errno_t r_error = rand_s(&r);
    assert(r_error==0);
    //Should return 0 to 2**31-1 instead of 2**32-1
    r >>= 1;
    return r;
}

//TODO: Implement srandom to modify the way rand_s works (IF POSSIBLE).. or
//reimplement random.
void
srandom(unsigned int seed) {
    seed = seed;
}

