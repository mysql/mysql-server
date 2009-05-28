#include <test.h>
#include <stdio.h>
#include <assert.h>
#include <toku_time.h>

int test_main(int argc, char *argv[]) {
    int r;
    struct timeval tv;
    struct timezone tz;

    r = gettimeofday(&tv, 0);
    assert(r == 0);
    
    return 0;
}
