#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <assert.h>
#include <sys/thr.h>
#include "toku_os.h"

static int gettid(void) {
    long tid; int r = thr_self(&tid); assert(r == 0);
    return tid;
}

int main(void) {
    assert(toku_os_getpid() == getpid());
    assert(toku_os_gettid() == gettid());
    return 0;
}
