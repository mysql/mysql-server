#include "test.h"
#include "logcursor.h"

#define LOGDIR __FILE__ ".dir"

int test_main(int argc, const char *argv[]) {
    int r;

    default_parse_args(argc, argv);

    r = system("rm -rf " LOGDIR);
    assert(r == 0);

    r = toku_os_mkdir(LOGDIR, S_IRWXU);    
    assert(r == 0);

    TOKULOGCURSOR lc;
    r = toku_logcursor_create(&lc, LOGDIR);
    assert(r == 0);

    toku_logcursor_print(lc);
    
    r = toku_logcursor_destroy(&lc);
    assert(r == 0);

    return 0;
}
