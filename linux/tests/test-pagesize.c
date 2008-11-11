#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <assert.h>
#include "os.h"

int main(void) {
    assert(os_get_pagesize() == getpagesize());
    return 0;
}
