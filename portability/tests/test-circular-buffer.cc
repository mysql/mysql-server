/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include "toku_assert.h"
#include "circular_buffer.h"
#include "memory.h"
#include "toku_time.h"
#include "test.h"

static int verbose = 0;
static volatile bool running;

static void *producer(void *extra) {
    toku::circular_buffer<uint32_t> *buf = static_cast<toku::circular_buffer<uint32_t> *>(extra);

    while (running) {
        buf->push(random());
        usleep(random() % 10000);
    }

    return nullptr;
}

struct consumer_extra {
    toku::circular_buffer<uint32_t> *buf;
    uint32_t xorsum;
};

static void *consumer(void *extra) {
    struct consumer_extra *e = static_cast<struct consumer_extra *>(extra);

    while (running) {
        e->xorsum ^= e->buf->pop();
        usleep(random() % 1000);
    }
    uint32_t x;
    bool popped = e->buf->trypop(&x);
    while (popped) {
        e->xorsum ^= x;
        popped = e->buf->trypop(&x);
    }

    return nullptr;
}

static void test_with_threads(void) {
    const size_t asize = 10000;
    uint32_t array[asize];
    toku::circular_buffer<uint32_t> buf;
    ZERO_STRUCT(buf);
    buf.init(array, asize);

    bool swapped = __sync_bool_compare_and_swap(&running, false, true);
    invariant(swapped);

    struct consumer_extra extra = { .buf = &buf, .xorsum = 0 };
    toku_pthread_t consumer_thd;
    int r = toku_pthread_create(&consumer_thd, nullptr, consumer, &extra);
    invariant_zero(r);

    const int nproducers = 10;
    toku_pthread_t producer_thds[nproducers];
    for (int i = 0; i < nproducers; ++i) {
        r = toku_pthread_create(&producer_thds[i], nullptr, producer, &buf);
        invariant_zero(r);
    }

    usleep(20 * 1000 * 1000);

    swapped = __sync_bool_compare_and_swap(&running, true, false);
    invariant(swapped);

    for (int i = 0; i < nproducers; ++i) {
        r = toku_pthread_join(producer_thds[i], nullptr);
        invariant_zero(r);
    }

    r = toku_pthread_join(consumer_thd, nullptr);
    invariant_zero(r);

    buf.deinit();

    if (verbose) {
        printf("%" PRIu32 "\n", extra.xorsum);
    }
}

int test_main(int argc, char *const argv[]) {
    {
        const char *progname=argv[0];
        argc--; argv++;
        while (argc>0) {
            if (strcmp(argv[0],"-v")==0) {
                ++verbose;
            } else if (strcmp(argv[0],"-q")==0) {
                verbose=0;
            } else {
                fprintf(stderr, "Usage:\n %s [-v] [-q]\n", progname);
                exit(1);
            }
            argc--; argv++;
        }
    }

    test_with_threads();

    return 0;
}
