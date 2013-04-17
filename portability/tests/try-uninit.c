#include <stdio.h>
#include <stdlib.h>

static void foo(int i) {
    printf("%d\n", i);
}

int main(int argc, char *argv[]) {
    int arg;
    int i;
    for (i = 1; i < argc; i++) {
        arg = atoi(argv[i]);
    }
    foo(arg);
    return 0;
}
