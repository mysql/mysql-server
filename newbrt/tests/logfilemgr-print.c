#include "test.h"
#include "logfilemgr.h"

int test_main(int argc __attribute__((unused)), const char *argv[] __attribute__((unused))) {
    int r;

    TOKULOGFILEMGR lfm = NULL;
    r = toku_logfilemgr_create(&lfm);
    assert(r == 0);

    toku_logfilemgr_print(lfm);

    r = toku_logfilemgr_destroy(&lfm);
    assert(r == 0);

    return 0;
}
