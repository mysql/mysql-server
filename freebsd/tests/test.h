#include <toku_portability.h>

int test_main(int argc, char *argv[]);

int
main(int argc, char *argv[]) {
    toku_portability_init();
    int r = test_main(argc, argv);
    toku_portability_destroy();
    return r;
}

