#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <fcntl.h>
#include "portability.h"
#include "os.h"

int verbose;

static void test_pread_empty(const char *fname) {
    int fd;
    char c[12];
    uint64_t r;

    unlink(fname);
    fd = open(fname,  O_RDWR | O_CREAT | O_BINARY, S_IRWXU|S_IRWXG|S_IRWXO);
    if (verbose)
        printf("open %s fd %d\n", fname, fd);
    assert(fd != -1);
    r = pread(fd, c, sizeof c, 0);
    assert(r == 0);
    r = close(fd);
    if (verbose)
        printf("close %s %"PRIu64"\n", fname, r);
}

int main(int argc, char *argv[]) {
    int i;

    for (i=1; i<argc; i++) {
        char *arg = argv[i];
        if (strcmp(arg, "-v") == 0 || strcmp(arg, "--verbose") == 0)
            verbose++;
    }

    test_pread_empty("junk");

    return 0;
}
