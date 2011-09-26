#include <stdio.h>
#include <stdlib.h>

static void foo(int i) {
    printf("%d\n", i);
}

int main(void) {
    int i;
    if (0)
        i = 42;
    foo(i);
    return 0;
}
