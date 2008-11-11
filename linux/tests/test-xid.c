#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <assert.h>
#include "os.h"
#include <syscall.h>

int os_getpid(void);

static int gettid(void) {
    return syscall(__NR_gettid);
}

int main(void) {
    assert(os_getpid() == getpid());
    assert(os_gettid() == gettid());
    return 0;
}
