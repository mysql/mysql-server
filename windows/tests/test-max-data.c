/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
#define _GNU_SOURCE
#include <test.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>
#include <toku_assert.h>
#include <toku_os.h>

int test_main(int argc, char *const argv[]) {
    uint64_t maxdata;
    int r = toku_os_get_max_process_data_size(&maxdata);
    assert(r == 0);
    printf("maxdata=%"PRIu64"\n", maxdata);
    return 0;
}
