#include "test.h"



int test_main (int argc __attribute__((__unused__)), char *argv[] __attribute__((__unused__))) {
  uint32_t h,n;

    parse_args(argc, argv);

    for (h = 0; h<4; h++) {
      n = htonl(h);
      printf("h = %d, n = %d\n", h,n);
    }
    return 0;
}
