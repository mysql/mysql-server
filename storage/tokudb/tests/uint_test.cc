#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>

#include <tokudb_math.h>
using namespace tokudb;

static void test(int length_bits) {
    printf("%s %d\n", __FUNCTION__, length_bits);
    uint64_t max = (1ULL << length_bits) - 1;
    for (uint64_t x = 0; x <= max; x++) {
        for (uint64_t y = 0; y <= max; y++) {
            bool over;
            uint64_t n = uint_add(x, y, max, &over);
            printf("%llu %llu %llu\n", x, y, n);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc > 1) {
        for (int i = 1; i < argc; i++) {
            test(atoi(argv[i]));
        }
    }
    return 0;
}
