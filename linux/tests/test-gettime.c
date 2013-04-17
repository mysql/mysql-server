#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <toku_time.h>

int main(void) {
    int r;
    struct timespec ts;

    r = clock_gettime(CLOCK_REALTIME, &ts);
    assert(r == 0);
    sleep(10);
    r = clock_gettime(CLOCK_REALTIME, &ts);
    assert(r == 0);
    
    return 0;
}
