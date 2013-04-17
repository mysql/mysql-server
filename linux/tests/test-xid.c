#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <assert.h>
#include "toku_os.h"
#include <syscall.h>

int toku_os_getpid(void);

static int gettid(void) {
    return syscall(__NR_gettid);
}

int main(void) {
    assert(toku_os_getpid() == getpid());
    assert(toku_os_gettid() == gettid());
    return 0;
}
