#include <test.h>
#include <stdio.h>
#include <stdlib.h>
#include <toku_assert.h>
#include "toku_os.h"

int verbose;

void testit(int64_t i, int base) {
    int64_t o;
#define SN 32
    char s[SN];
    sprintf(s, "%I64d", i);
    o = strtoll(s, NULL, base);
    if (verbose) 
        printf("%s: %I64d %I64d %s\n", __FUNCTION__, i, o, s);
    assert(i == o);
}

int test_main(int argc, char *const argv[]) {
    int i;
    int64_t n;
    int64_t o;
#define SN 32
    char s[SN];

    for (i=1; i<argc; i++) {
        char *arg = argv[i];
        if (strcmp(arg, "-v") == 0 || strcmp(arg, "--verbose") == 0)
            verbose++;
    }

    for (n=0; n<1000; n++) {
        testit(n, 10);
    }

    testit(1I64 << 31, 10);
    testit((1I64 << 32) - 1, 10);
    testit(1I64 << 32, 10);

    return 0;
}
