#include <stdio.h>
#include "memory.h"

int main(void) {
    toku_memory_startup();
    MEMORY_STATUS_S s;
    toku_memory_get_status(&s);
    printf("mallocator: %s\n", s.mallocator_version);
    printf("mmap threshold: %" PRIu64 "\n", s.mmap_threshold);
    toku_memory_shutdown();
    return 0;
}
