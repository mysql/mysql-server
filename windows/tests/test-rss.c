#include <test.h>
#include <stdio.h>
#include <stdlib.h>
#include <toku_assert.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <toku_os.h>

static void do_mallocs(void) {
    int i;
    for (i=0; i<1000; i++) {
        int nbytes = 1024*1024;
        void *vp = malloc(nbytes);
        memset(vp, 0, nbytes);
    }
}

int test_main(int argc, char *const argv[]) {
    int64_t rss;

    toku_os_get_max_rss(&rss);
    printf("%I64d\n", rss);
    do_mallocs();
    toku_os_get_max_rss(&rss);
    printf("%I64d\n", rss);

    return 0;
}

    
