#include "memory.h"
#include "stdlib.h"

int main () {
    char *m=toku_malloc(5);
    m=m;
    free(m);
    return 0;
}
