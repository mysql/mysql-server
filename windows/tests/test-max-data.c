#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>
#include <toku_assert.h>
#include <test.h>
#include <toku_os.h>

int test_main(int argc, char *argv[]) {
    uint64_t maxdata;
    int r = toku_os_get_max_process_data_size(&maxdata);
    assert(r == 0);
    printf("maxdata=%"PRIu64"\n", maxdata);
    return 0;
}
