#include <test.h>
#include <stdio.h>
#include <toku_assert.h>
#include <toku_time.h>

int test_main(int argc, char *const argv[]) {
    int r;
    struct timeval tv;
    struct timezone tz;

    r = gettimeofday(&tv, 0);
    assert(r == 0);
    
    return 0;
}
