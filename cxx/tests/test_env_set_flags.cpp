#include <assert.h>
#include <db_cxx.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    int r;

    DbEnv env(DB_CXX_NO_EXCEPTIONS);
    r = env.set_flags(0, 0); assert(r == 0);
    r = env.set_flags(0, 1); assert(r == 0);
    return 0;
}
