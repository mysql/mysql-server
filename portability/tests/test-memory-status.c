/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
#include <stdio.h>
#include "memory.h"

int main(void) {
    toku_memory_startup();
    LOCAL_MEMORY_STATUS_S s;
    toku_memory_get_status(&s);
    printf("mallocator: %s\n", s.mallocator_version);
    printf("mmap threshold: %" PRIu64 "\n", s.mmap_threshold);
    toku_memory_shutdown();
    return 0;
}
