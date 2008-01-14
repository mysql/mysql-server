#include <iostream>
#include <fcntl.h>
#include <assert.h>
#include <errno.h>
#include <db_cxx.h>

int verbose;

int test_error_stream(const char *dbfile) {
    int r;

    r = unlink(dbfile);
    r = creat(dbfile, 0777); assert(r >= 0); close(r);

    DbEnv env(DB_CXX_NO_EXCEPTIONS);
    env.set_errpfx("my_env_error_stream");
    env.set_error_stream(&std::cerr);
    
    r = env.open(".", DB_INIT_MPOOL + DB_CREATE + DB_PRIVATE, 0777); assert(r == 0);
    r = env.open(".", DB_INIT_MPOOL + DB_CREATE + DB_PRIVATE, 0777); assert(r == EINVAL);

    Db db(&env, 0);
    db.set_errpfx("my_db_error_stream");
    db.set_error_stream(&std::cerr);
    r = db.open(0, dbfile, 0, DB_BTREE, DB_CREATE, 0777); assert(r != 0);
    r = db.close(0); assert(r == 0);
    r = db.close(0); assert(r == EINVAL);
    r = env.close(0); assert(r == 0);
    r = env.close(0); assert(r == EINVAL);
    return 0;
}

int usage() {
    printf("test_error_stream [-v] [--verbose]\n");
    return 1;
}

int main(int argc, char *argv[]) {
    for (int i=1; i<argc; i++) {
        char *arg = argv[i];
        if (0 == strcmp(arg, "-h") || 0 == strcmp(arg, "--help")) {
            return usage();
        }
        if (0 == strcmp(arg, "-v") || 0 == strcmp(arg, "--verbose")) {
            verbose = 1; continue;
        }
    }

    return test_error_stream("test.db");
}
