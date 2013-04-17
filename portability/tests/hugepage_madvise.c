#include <stdlib.h>
#include <assert.h>
#include "memory.h"

enum which_mallocator { M_LIBC, M_JEMALLOC, M_DARWIN } which_mallocator;

int main (int argc, const char *argv[] __attribute__((__unused__))) {
    assert(argc==1);
    void *p;
    int r = posix_memalign(&p, 2L*1024L*1024L, 4096);
    toku_memory_startup();
    which_mallocator = M_JEMALLOC;
    assert(r==0);
    ((char*)p)[0] = 'a';
    toku_memory_dontneed_after_but_i_touched(p, 4096, 0, 1);
    return 0;
}
