#include <toku_portability.h>

int test_main(int argc, char *const argv[]);

int
main(int argc, char *const argv[]) {
    toku_portability_init();
    int r = test_main(argc, argv);
    toku_portability_destroy();
    return r;
}

