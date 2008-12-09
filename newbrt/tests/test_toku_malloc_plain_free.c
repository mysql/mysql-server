#include "memory.h"
#include "stdlib.h"

#include "test.h"

int
test_main (int argc, const char *argv[]) {
    default_parse_args(argc, argv);
    char *m=toku_malloc(5);
    m=m;
    free(m);
    return 0;
}
