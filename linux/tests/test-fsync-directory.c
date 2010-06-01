// use strace to very that the toku_fsync_directory function works

#include "test.h"
#include <stdlib.h>
#include <string.h>

#define ENVDIR "dir." __FILE__

static int verbose = 0;

int test_main(int argc, char *const argv[]) {
    int r;

    for (int i=1; i<argc; i++) {
        if (strcmp(argv[i], "-v") == 0) {
            if (verbose < 0) verbose = 0;
            verbose++;
            continue;
        } else if (strcmp(argv[i], "-q") == 0) {
            verbose = 0;
            continue;
        } else {
            exit(1);
        }
    }

    r = system("rm -rf " ENVDIR);
    CKERR(r);
    r = toku_os_mkdir(ENVDIR, S_IRWXU+S_IRWXG+S_IRWXO);
    CKERR(r);
    r = toku_os_mkdir(ENVDIR "/test", S_IRWXU+S_IRWXG+S_IRWXO);
    CKERR(r);

    r = toku_fsync_directory(""); CKERR(r);
    r = toku_fsync_directory("."); CKERR(r);
    r = toku_fsync_directory(ENVDIR "/test/a"); CKERR(r);
    r = toku_fsync_directory("./" ENVDIR "/test/a"); CKERR(r);
    r = toku_fsync_directory("/tmp/x"); CKERR(r);

    return 0;
}
