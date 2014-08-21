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

  TokuFT, Tokutek Fractal Tree Indexing Library.
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

#include "test.h"

#include <toku_portability.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <db.h>
#include <string.h>

#include <memory.h>

#include <portability/toku_atomic.h>
#include <portability/toku_pthread.h>
#include <portability/toku_random.h>

#include <util/omt.h>
#include <util/rwlock.h>

namespace toku {

namespace test {

static inline uint32_t fudge(const uint32_t x) { return x + 300; }
static inline uint32_t defudge(const uint32_t fx) { return fx - 300; }

int test_iterator(const uint32_t &v, const uint32_t idx, bool *const UU(unused));
int test_iterator(const uint32_t &v, const uint32_t idx, bool *const UU(unused)) {
    invariant(defudge(v) == idx);
    return 0;
}

int check_iterator_before(const uint32_t &v, const uint32_t idx, bool *const called);
int check_iterator_before(const uint32_t &v, const uint32_t idx, bool *const called) {
    invariant(defudge(v) == idx);
    invariant(idx % 10 < 5);
    called[idx] = true;
    return 0;
}

int check_iterator_after(const uint32_t &v, const uint32_t UU(idx), bool *const called);
int check_iterator_after(const uint32_t &v, const uint32_t UU(idx), bool *const called) {
    invariant(defudge(v) % 10 >= 5);
    called[defudge(v)] = true;
    return 0;
}

int die(const uint32_t &UU(v), const uint32_t UU(idx), void *const UU(unused));
int die(const uint32_t &UU(v), const uint32_t UU(idx), void *const UU(unused)) {
    abort();
    return 0; // hahaha
}

static void run_test(uint32_t nelts) {
    assert(nelts % 10 == 0);  // run_test depends on nelts being a multiple of 10

    omt<uint32_t, uint32_t, true> omt;
    omt.create();
    omt.verify_marks_consistent();
    for (uint32_t i = 0; i < nelts; ++i) {
        omt.insert_at(fudge(i), i);
    }
    omt.verify_marks_consistent();

    int r;
    for (uint32_t i = 0; i < nelts / 10; ++i) {
        r = omt.iterate_and_mark_range<bool, test_iterator>(i * 10, i * 10 + 5, nullptr);
        invariant_zero(r);
        omt.verify_marks_consistent();
    }

    bool called[nelts];
    ZERO_ARRAY(called);
    r = omt.iterate_over_marked<bool, check_iterator_before>(called);
    invariant_zero(r);
    for (uint32_t i = 0; i < nelts; ++i) {
        if (i % 10 < 5) {
            invariant(called[i]);
        } else {
            invariant(!called[i]);
        }
    }
    omt.verify_marks_consistent();

    invariant(omt.size() == nelts);

    omt.delete_all_marked();
    omt.verify_marks_consistent();

    invariant(omt.size() * 2 == nelts);

    r = omt.iterate_over_marked<void, die>(nullptr);
    invariant_zero(r);

    ZERO_ARRAY(called);
    r = omt.iterate<bool, check_iterator_after>(called);
    invariant_zero(r);
    omt.verify_marks_consistent();

    for (uint32_t i = 0; i < nelts; ++i) {
        if (i % 10 < 5) {
            invariant(!called[i]);
        } else {
            invariant(called[i]);
        }
    }

    omt.destroy();
}

typedef omt<uint32_t, uint32_t, true> stress_omt;

int int_heaviside(const uint32_t &v, const uint32_t &target);
int int_heaviside(const uint32_t &v, const uint32_t &target) {
    return (v > target) - (v < target);
}

struct stress_shared {
    stress_omt *omt;
    volatile bool running;
    struct rwlock lock;
    toku_mutex_t mutex;
    int num_marker_threads;
};

struct reader_extra {
    int tid;
    stress_shared *shared;
    uint64_t iterations;
    uint64_t last_iteration;
    char buf_read[8];
    char buf_write[8];
    struct random_data rand_read;
    struct random_data rand_write;
};

static void generate_range(struct random_data *rng, const struct stress_shared &shared, uint32_t *begin, uint32_t *limit) {
    const uint32_t nelts = shared.omt->size();
    double range_limit_d = nelts;
    range_limit_d /= 1000;
    range_limit_d /= shared.num_marker_threads;
    range_limit_d += 1;
    uint32_t range_limit = static_cast<uint32_t>(range_limit_d);
    if (range_limit < 5) {
        range_limit = 5;
    }
    if (range_limit > 1000) {
        range_limit = 1000;
    }
    *begin = rand_choices(rng, nelts - 1);
    if (*begin + range_limit > nelts) {
        range_limit = nelts - *begin;
    }
    *limit = *begin + rand_choices(rng, range_limit);
}

struct pair {
    uint32_t begin;
    uint32_t limit;
};

int mark_read_iterator(const uint32_t &UU(v), const uint32_t idx, struct pair * const pair);
int mark_read_iterator(const uint32_t &UU(v), const uint32_t idx, struct pair * const pair) {
    invariant(defudge(v) == idx);
    invariant(idx >= pair->begin);
    invariant(idx < pair->limit);
    return 0;
}

static void *stress_mark_worker(void *extrav) {
    struct reader_extra *CAST_FROM_VOIDP(extra, extrav);
    struct stress_shared &shared = *extra->shared;
    toku_mutex_t &mutex = shared.mutex;

    while (shared.running) {
        toku_mutex_lock(&mutex);
        rwlock_read_lock(&shared.lock, &mutex);
        toku_mutex_unlock(&mutex);

        struct pair range;
        generate_range(&extra->rand_read, shared, &range.begin, &range.limit);

        shared.omt->iterate_and_mark_range<pair, mark_read_iterator>(range.begin, range.limit, &range);

        ++extra->iterations;

        toku_mutex_lock(&mutex);
        rwlock_read_unlock(&shared.lock);
        toku_mutex_unlock(&mutex);

        usleep(1);
    }

    return nullptr;
}

template<typename T>
class array_ftor {
    int m_count;
    T *m_array;
public:
    array_ftor(int size) : m_count(0) {
        XMALLOC_N(size, m_array);
    }
    ~array_ftor() {
        toku_free(m_array);
    }
    void operator() (const T &x) { m_array[m_count++] = x; }
    template<class callback_t>
    void iterate(callback_t &cb) const {
        for (int i = 0; i < m_count; ++i) {
            cb(m_array[i]);
        }
    }
};

int use_array_ftor(const uint32_t &v, const uint32_t UU(idx), array_ftor<uint32_t> *const fp);
int use_array_ftor(const uint32_t &v, const uint32_t UU(idx), array_ftor<uint32_t> *const fp) {
    array_ftor<uint32_t> &f = *fp;
    f(v);
    return 0;
}

class inserter {
    stress_omt *m_omt;
public:
    inserter(stress_omt *omt) : m_omt(omt) {}
    void operator() (const uint32_t &x) {
        m_omt->insert<uint32_t, int_heaviside>(x, x, nullptr);
    }
};

/*
 * split the range evenly/not evenly between marker threads
 * context tells it the range
 * context also holds iteration number
 *
 * N threads
 * N 'contexts' holds iteration number, seed
 *
 * create rng based on seed
 * loop:
 *   generate random range.  Mark that range, increment iteration number
 *
 *
 * 
 *
 * for each context
     * create rng based on context->last_seed
     *   loop (iteration number times)
     *     mark (in array) random range
     * context->last_seed := context->seed
 * check the array and the omt
 *
 */

static void simulate_reader_marks_on_array(struct reader_extra *const reader, const struct stress_shared &shared, bool *const should_be_marked) {
    if (verbose) {
        fprintf(stderr, "thread %d ran %" PRIu64 " iterations\n", reader->tid, reader->iterations - reader->last_iteration);
    }
    for (; reader->last_iteration < reader->iterations; ++reader->last_iteration) {
        uint32_t begin;
        uint32_t limit;

        generate_range(&reader->rand_write, shared, &begin, &limit);

        for (uint32_t i = begin; i < limit; i++) {
            should_be_marked[i] = true;
        }
    }
}

int copy_marks(const uint32_t &v, const uint32_t idx, bool * const is_marked);
int copy_marks(const uint32_t &v, const uint32_t idx, bool * const is_marked) {
    invariant(defudge(v) == idx);
    is_marked[idx] = true;
    return 0;
}

static inline uint32_t count_true(const bool *const bools, uint32_t n) {
    uint32_t count = 0;
    for (uint32_t i = 0; i < n; ++i) {
        if (bools[i]) {
            ++count;
        }
    }
    return count;
}

static void stress_deleter(struct reader_extra *const readers, int num_marker_threads, stress_omt *omt) {
    // Verify (iterate_over_marked) agrees exactly with iterate_and_mark_range (multithreaded)
    stress_shared &shared = *readers[0].shared;
    bool should_be_marked[omt->size()];
    ZERO_ARRAY(should_be_marked);

    for (int i = 0; i < num_marker_threads; i++) {
        simulate_reader_marks_on_array(&readers[i], shared, should_be_marked);
    }

    bool is_marked_according_to_iterate[omt->size()];
    ZERO_ARRAY(is_marked_according_to_iterate);

    omt->verify_marks_consistent();
    omt->iterate_over_marked<bool, copy_marks>(&is_marked_according_to_iterate[0]);
    omt->verify_marks_consistent();

    invariant(!memcmp(should_be_marked, is_marked_according_to_iterate, sizeof(should_be_marked)));

    if (verbose) {
        double frac_marked = count_true(should_be_marked, omt->size());
        frac_marked /= omt->size();

        fprintf(stderr, "Marked: %0.4f\n", frac_marked);
        omt->verify_marks_consistent();
    }

    array_ftor<uint32_t> aftor(omt->size());
    omt->iterate_over_marked<array_ftor<uint32_t>, use_array_ftor>(&aftor);
    omt->delete_all_marked();
    omt->verify_marks_consistent();
    omt->iterate_over_marked<void, die>(nullptr);
    inserter ins(omt);
    aftor.iterate(ins);
    omt->verify_marks_consistent();
}

static void *stress_delete_worker(void *extrav) {
    reader_extra *CAST_FROM_VOIDP(readers, extrav);
    stress_shared &shared = *readers[0].shared;
    int num_marker_threads = shared.num_marker_threads;
    toku_mutex_t &mutex = shared.mutex;
    const double repetitions = 20;
    for (int i = 0; i < repetitions; ++i) {
        // sleep 0 - 0.15s
        // early iterations sleep for a short time
        // later iterations sleep longer
        int sleep_for = 1000 * 100 * (1.5 * (i+1) / repetitions);
        usleep(sleep_for);

        toku_mutex_lock(&mutex);
        rwlock_write_lock(&shared.lock, &mutex);
        toku_mutex_unlock(&mutex);

        stress_deleter(readers, num_marker_threads, shared.omt);

        toku_mutex_lock(&mutex);
        rwlock_write_unlock(&shared.lock);
        toku_mutex_unlock(&mutex);
    }
    toku_sync_bool_compare_and_swap(&shared.running, true, false);
    return nullptr;
}

static void stress_test(int nelts) {
    stress_omt omt;
    omt.create();
    for (int i = 0; i < nelts; ++i) {
        omt.insert_at(fudge(i), i);
    }

    const int num_marker_threads = 5;
    struct stress_shared extra;
    ZERO_STRUCT(extra);
    extra.omt = &omt;
    toku_mutex_init(&extra.mutex, NULL);
    rwlock_init(&extra.lock);
    extra.running = true;
    extra.num_marker_threads = num_marker_threads;

    struct reader_extra readers[num_marker_threads];
    ZERO_ARRAY(readers);

    srandom(time(NULL));
    toku_pthread_t marker_threads[num_marker_threads];
    for (int i = 0; i < num_marker_threads; ++i) {
        struct reader_extra &reader = readers[i];
        reader.tid = i;
        reader.shared = &extra;

        int r;
        int seed = random();
        r = myinitstate_r(seed, reader.buf_read, 8, &reader.rand_read);
        invariant_zero(r);
        r = myinitstate_r(seed, reader.buf_write, 8, &reader.rand_write);
        invariant_zero(r);

        toku_pthread_create(&marker_threads[i], NULL, stress_mark_worker, &reader);
    }

    toku_pthread_t deleter_thread;
    toku_pthread_create(&deleter_thread, NULL, stress_delete_worker, &readers[0]);
    toku_pthread_join(deleter_thread, NULL);

    for (int i = 0; i < num_marker_threads; ++i) {
        toku_pthread_join(marker_threads[i], NULL);
    }

    rwlock_destroy(&extra.lock);
    toku_mutex_destroy(&extra.mutex);

    omt.destroy();
}

} // end namespace test

} // end namespace toku

int test_main(int argc, const char *argv[]) {
    default_parse_args(argc, argv);

    for (int i = 10; i <= 80; i*=2) {
        toku::test::run_test(i);
    }

    toku::test::run_test(9000);

    toku::test::stress_test(1000 * 100);

    return 0;
}
