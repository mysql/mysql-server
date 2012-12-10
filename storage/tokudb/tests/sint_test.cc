#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>

#include <tokudb_math.h>
using namespace tokudb;

static void test(int length_bits) {
    int64_t max = (1ULL << (length_bits-1)) - 1;
    for (int64_t x = -max-1; x <= max; x++) {
        for (int64_t y = -max-1; y <= max; y++) {
            bool over;
            int64_t n = int_add(x, y, length_bits, &over);
            printf("%lld %lld %lld %u\n", x, y, n, over);
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
