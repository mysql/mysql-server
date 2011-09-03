/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id$"
#ident "Copyright (c) 2007-2011 Tokutek Inc.  All rights reserved."

#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>
#include "test.h"

#include "includes.h"

#define MIN(x, y) (((x) < (y)) ? (x) : (y))
const double USECS_PER_SEC = 1000000.0;

static int
long_key_cmp(DB *UU(e), const DBT *a, const DBT *b)
{
    const long *x = a->data, *y = b->data;
    return (*x > *y) - (*x < *y);
}

void
run_test(long eltsize, long nodesize, long repeat)
{
    int cur = 0;
    long keys[1024];
    char *vals[1024];
    for (int i = 0; i < 1024; ++i) {
        keys[i] = rand();
        vals[i] = toku_xmalloc(eltsize - (sizeof keys[i]));
        int j = 0;
        for (; j < eltsize - (sizeof keys[i]) - sizeof(int); j += sizeof(int)) {
            int *p = (void *) &((char *) vals[i])[j];
            *p = rand();
        }
        for (; j < eltsize - (sizeof keys[i]); ++j) {
            char *p = &((char *) vals[i])[j];
            *p = (rand() & 0xff);
        }
    }
    XIDS xids_0 = xids_get_root_xids();
    XIDS xids_123;
    int r = xids_create_child(xids_0, &xids_123, (TXNID)123);
    CKERR(r);

    NONLEAF_CHILDINFO bnc;
    long long unsigned nbytesinserted = 0;
    struct timeval t[2];
    gettimeofday(&t[0], NULL);

    for (int i = 0; i < repeat; ++i) {
        bnc = toku_create_empty_nl();
        for (; toku_bnc_nbytesinbuf(bnc) <= nodesize; ++cur) {
            r = toku_bnc_insert_msg(bnc,
                                    &keys[cur % 1024], sizeof keys[cur % 1024],
                                    vals[cur % 1024], eltsize - (sizeof keys[cur % 1024]),
                                    BRT_NONE, next_dummymsn(), xids_123, true,
                                    NULL, long_key_cmp); assert_zero(r);
        }
        nbytesinserted += toku_bnc_nbytesinbuf(bnc);
        destroy_nonleaf_childinfo(bnc);
    }

    gettimeofday(&t[1], NULL);
    double dt;
    dt = (t[1].tv_sec - t[0].tv_sec) + ((t[1].tv_usec - t[0].tv_usec) / USECS_PER_SEC);
    double mbrate = ((double) nbytesinserted / (1 << 20)) / dt;
    long long unsigned eltrate = (long) (cur / dt);
    printf("%0.03lf MB/sec\n", mbrate);
    printf("%llu elts/sec\n", eltrate);
}

int
test_main (int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    long eltsize, nodesize, repeat;

    if (argc != 4) {
        fprintf(stderr, "Usage: %s <eltsize> <nodesize> <repeat>\n", argv[0]);
        return 2;
    }
    eltsize = strtol(argv[1], NULL, 0);
    nodesize = strtol(argv[2], NULL, 0);
    repeat = strtol(argv[3], NULL, 0);

    run_test(eltsize, nodesize, repeat);

    return 0;
}
