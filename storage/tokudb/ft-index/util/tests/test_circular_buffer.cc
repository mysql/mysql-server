/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
/*
COPYING CONDITIONS NOTICE:

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation, and provided that the
  following conditions are met:

      * Redistributions of source code must retain this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below).

      * Redistributions in binary form must reproduce this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below) in the documentation and/or other materials
        provided with the distribution.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301, USA.

COPYRIGHT NOTICE:

  TokuDB, Tokutek Fractal Tree Indexing Library.
  Copyright (C) 2007-2013 Tokutek, Inc.

DISCLAIMER:

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

UNIVERSITY PATENT NOTICE:

  The technology is licensed by the Massachusetts Institute of
  Technology, Rutgers State University of New Jersey, and the Research
  Foundation of State University of New York at Stony Brook under
  United States of America Serial No. 11/760379 and to the patents
  and/or patent applications resulting from it.

PATENT MARKING NOTICE:

  This software is covered by US Patent No. 8,185,551.
  This software is covered by US Patent No. 8,489,638.

PATENT RIGHTS GRANT:

  "THIS IMPLEMENTATION" means the copyrightable works distributed by
  Tokutek as part of the Fractal Tree project.

  "PATENT CLAIMS" means the claims of patents that are owned or
  licensable by Tokutek, both currently or in the future; and that in
  the absence of this license would be infringed by THIS
  IMPLEMENTATION or by using or running THIS IMPLEMENTATION.

  "PATENT CHALLENGE" shall mean a challenge to the validity,
  patentability, enforceability and/or non-infringement of any of the
  PATENT CLAIMS or otherwise opposing any of the PATENT CLAIMS.

  Tokutek hereby grants to you, for the term and geographical scope of
  the PATENT CLAIMS, a non-exclusive, no-charge, royalty-free,
  irrevocable (except as stated in this section) patent license to
  make, have made, use, offer to sell, sell, import, transfer, and
  otherwise run, modify, and propagate the contents of THIS
  IMPLEMENTATION, where such license applies only to the PATENT
  CLAIMS.  This grant does not include claims that would be infringed
  only as a consequence of further modifications of THIS
  IMPLEMENTATION.  If you or your agent or licensee institute or order
  or agree to the institution of patent litigation against any entity
  (including a cross-claim or counterclaim in a lawsuit) alleging that
  THIS IMPLEMENTATION constitutes direct or contributory patent
  infringement, or inducement of patent infringement, then any rights
  granted to you under this License shall terminate as of the date
  such litigation is filed.  If you or your agent or exclusive
  licensee institute or order or agree to the institution of a PATENT
  CHALLENGE, then Tokutek may terminate any rights granted to you
  under this License.
*/

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "test.h"
#include <util/circular_buffer.h>

#include <toku_assert.h>
#include <portability/toku_atomic.h>
#include <memory.h>
#include <toku_pthread.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

static volatile bool running;
static volatile bool producers_joined;

static void *producer(void *extra) {
    toku::circular_buffer<uint32_t> *buf = static_cast<toku::circular_buffer<uint32_t> *>(extra);

    while (running) {
        buf->push(random());
        if (running) {
            usleep(random() % 1000);
        }
    }

    return nullptr;
}

struct consumer_extra {
    toku::circular_buffer<uint32_t> *buf;
    uint32_t xorsum;
};

static void *consumer(void *extra) {
    struct consumer_extra *e = static_cast<struct consumer_extra *>(extra);

    while (!producers_joined) {
        e->xorsum ^= e->buf->pop();
        if (running) {
            usleep(random() % 100);
        }
    }
    uint32_t x;
    while (e->buf->trypop(&x)) {
        e->xorsum ^= x;
    }

    return nullptr;
}

static void test_with_threads(void) {
    const size_t asize = 10000;
    uint32_t array[asize];
    toku::circular_buffer<uint32_t> buf;
    ZERO_STRUCT(buf);
    buf.init(array, asize);

    bool swapped = toku_sync_bool_compare_and_swap(&running, false, true);
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

    swapped = toku_sync_bool_compare_and_swap(&running, true, false);
    invariant(swapped);

    for (int i = 0; i < nproducers; ++i) {
        r = toku_pthread_join(producer_thds[i], nullptr);
        invariant_zero(r);
    }
    swapped = toku_sync_bool_compare_and_swap(&producers_joined, false, true);
    invariant(swapped);

    // kick it in case it's still waiting
    bool pushed = buf.trypush(1);
    (void) pushed;  // yes, really ignore it

    r = toku_pthread_join(consumer_thd, nullptr);
    invariant_zero(r);

    buf.deinit();

    if (verbose) {
        printf("%" PRIu32 "\n", extra.xorsum);
    }
}

int test_main(int argc, const char *argv[]) {
    default_parse_args(argc, argv);

    test_with_threads();

    return 0;
}
