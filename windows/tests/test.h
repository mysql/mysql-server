#include <toku_portability.h>

int test_main(int argc, char *argv[]);

int
main(int argc, char *argv[]) {
    int ri = toku_portability_init();
    assert(ri==0);
    int r = test_main(argc, argv);
    int rd = toku_portability_destroy();
    assert(rd==0);
    return r;
}

