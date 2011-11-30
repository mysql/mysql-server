#include <stdio.h>
#include "memory.h"

int main(void) {
    MEMORY_STATUS_S s;
    toku_memory_get_status(&s);
    printf("tokudb mallocator: %s\n", s.mallocator_version);
    return 0;
}
