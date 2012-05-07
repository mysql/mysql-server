#include <stdio.h>
#include <sys/types.h>
#include <toku_assert.h>
#include <toku_time.h>

int main(void) {
    int r;
    struct timeval tv;
    struct timezone tz;

    r = gettimeofday(&tv, &tz);
    assert(r == 0);
    
    return 0;
}
