#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <fcntl.h>
#include "toku_portability.h"
#include "toku_os.h"

int verbose;

int main(int argc, char *argv[]) {
    int i;

    for (i=1; i<argc; i++) {
        char *arg = argv[i];
        if (strcmp(arg, "-v") == 0 || strcmp(arg, "--verbose") == 0)
            verbose++;
    }

    for (i=0; i<10; i++) {
        if (verbose) {
            printf("sleep %d\n", i); fflush(stdout);
        }
        sleep(i);
    }

    for (i=0; i<10*1000000; i += 1000000) {
        if (verbose) {
            printf("usleep %d\n", i); fflush(stdout);
        }
        usleep(i);
    }

    return 0;
}
