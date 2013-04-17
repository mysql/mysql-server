#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <assert.h>
#include "toku_os.h"

int main(void) {
    assert(toku_os_get_pagesize() == getpagesize());
    return 0;
}
