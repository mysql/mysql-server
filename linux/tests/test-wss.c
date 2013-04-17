#include <stdio.h>
#include <stdlib.h>
#include <toku_assert.h>
#include <string.h>
#include <toku_stdint.h>
#include <toku_os.h>

const int nbuffers = 1000;
const int buffersize = 1024*1024;

static void do_mallocs(void) {
    int i;
    void *vp[nbuffers];
    for (i=0; i<nbuffers; i++) {
        int nbytes = buffersize;
        vp[i] = malloc(nbytes);
        memset(vp[i], 0, nbytes);
    }
    for (i=0; i<nbuffers; i++)
        free(vp[i]);
}

int main(int argc, char *argv[]) {
    int verbose = 0;
    int i;
    for (i=1; i<argc; i++) {
        if (strcmp(argv[i], "-v") == 0) {
            verbose = 1;
            continue;
        }
        if (strcmp(argv[i], "-q") == 0) {
            verbose = 0;
            continue;
        }
    }

    int64_t rss;
    toku_os_get_max_rss(&rss);
    if (verbose) printf("%"PRId64"\n", rss);
    assert(rss < nbuffers*buffersize);
    do_mallocs();
    toku_os_get_max_rss(&rss);
    if (verbose) printf("%"PRId64"\n", rss);
    assert(rss > nbuffers*buffersize);

    return 0;
}

    
