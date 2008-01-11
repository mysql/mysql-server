#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <db_cxx.h>

int verbose;

int test_fd(const char *dbfile) {
    int r;
    int fd;

    Db db(0, DB_CXX_NO_EXCEPTIONS);
    r = db.fd(&fd); assert(r == EINVAL);
    unlink(dbfile);
    if (verbose) { printf("opening %s\n", dbfile); fflush(stdout); }
    r = db.open(0, dbfile, 0, DB_BTREE, DB_CREATE, 0777); assert(r == 0); 
    r = db.fd(&fd); assert(r == 0); assert(fd >= 0);
    if (verbose) { printf("fd=%d\n", fd); fflush(stdout); }
    if (verbose) { printf("closing\n"); fflush(stdout); }
    r = db.close(0); assert(r == 0);
    if (verbose) { printf("closed\n"); fflush(stdout); }

    return 0;
}

int usage() {
    printf("test_fd [-v] [--verbose]\n");
    return 1;
}

int main(int argc, char *argv[]) {
    for (int i=1; i<argc; i++) {
        char *arg = argv[i];
        if (0 == strcmp(arg, "-h") || 0 == strcmp(arg, "--help")) 
            return usage();
        if (0 == strcmp(arg, "-v") || 0 == strcmp(arg, "--verbose"))
            verbose += 1;
    }

    return test_fd("test.db");
}

