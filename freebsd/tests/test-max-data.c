#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#if defined(__linux__)
#include <bits/wordsize.h>
#endif
#include "toku_os.h"

int main(int argc, char *argv[]) {
    int verbose = 0;
    int i;
    for (i=1; i<argc; i++) {
        if (strcmp(argv[i], "-v") == 0) {
            verbose = 1;
            continue;
        }
        if (strcmp(argv[i], "-q") == 0) {
            verbose = 0;
            continue;
        }
    }
        
    // get the data size
    uint64_t maxdata;
    int r = toku_os_get_max_process_data_size(&maxdata);
    assert(r == 0);
    if (verbose) printf("maxdata=%"PRIu64"\n", maxdata);

    // check the data size
#if defined(__linux__)
#if __WORDSIZE == 64
    assert(maxdata > (1ULL << 32));
#elif __WORDSIZE == 32
    assert(maxdata < (1ULL << 32));
#else
#error
#endif
#else
    assert(maxdata > (1ULL << 32));
#endif

    return 0;
}
