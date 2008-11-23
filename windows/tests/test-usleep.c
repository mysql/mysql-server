#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

int verbose;

int main(int argc, char *argv[]) {
    int i;
    int n = 1;

    for (i=1; i<argc; i++) {
        char *arg = argv[i];
        if (strcmp(arg, "-v") == 0 || strcmp(arg, "--verbose") == 0)
            verbose++;
        n = atoi(arg);
    }

    for (i=0; i<1000; i++) {
        if (verbose) {
            printf("usleep %d\n", i); fflush(stdout);
        }
        usleep(n);
    }

    return 0;
}
