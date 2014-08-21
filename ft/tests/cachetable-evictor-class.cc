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
#include "cachetable/cachetable-internal.h"

class evictor_unit_test {
public:
    evictor m_ev;
    pair_list m_pl;
    cachefile_list m_cf_list;
    KIBBUTZ m_kb;
    void init();
    void destroy();
    void run_test();
    void verify_ev_init(long limit);
    void verify_ev_destroy();
    void verify_ev_counts();
    void verify_ev_m_size_reserved();
    void verify_ev_handling_cache_pressure();

    // function to disable the eviction thread from waking up every second
    void disable_ev_thread();
};

// initialize this class to run tests
void evictor_unit_test::init() {
    ZERO_STRUCT(m_pl);
    ZERO_STRUCT(m_cf_list);
    m_pl.init();
    m_cf_list.init();
    m_kb = NULL;
    int r =  toku_kibbutz_create(1, &m_kb);
    assert(r == 0);
}

// destroy class after tests have run
void evictor_unit_test::destroy() {
    m_pl.destroy();
    m_cf_list.destroy();
    toku_kibbutz_destroy(m_kb);
}

// test that verifies evictor.init properly worked
void evictor_unit_test::verify_ev_init(long limit) {
    assert(m_ev.m_kibbutz == m_kb);
    assert(m_ev.m_pl == &m_pl);
    assert(m_ev.m_cf_list == &m_cf_list);
    assert(m_ev.m_low_size_watermark == limit);
    assert(m_ev.m_num_sleepers == 0);
    assert(m_ev.m_run_thread == true);
    assert(m_ev.m_size_current == 0);
    assert(read_partitioned_counter(m_ev.m_size_leaf) == 0);
    assert(read_partitioned_counter(m_ev.m_size_nonleaf) == 0);
    assert(read_partitioned_counter(m_ev.m_size_rollback) == 0);
    assert(read_partitioned_counter(m_ev.m_size_cachepressure) == 0);
    assert(m_ev.m_size_evicting == 0);
    // this comes from definition of unreservable_memory in cachetable.cc
    assert(m_ev.m_size_reserved == (limit/4)); 
}

// test that verifies evictor.destroy properly worked
void evictor_unit_test::verify_ev_destroy() {
    assert(m_ev.m_num_sleepers == 0);
    assert(m_ev.m_run_thread == false);
}

void evictor_unit_test::disable_ev_thread() {
    toku_mutex_lock(&m_ev.m_ev_thread_lock);
    m_ev.m_period_in_seconds = 0;
    // signal eviction thread so that it wakes up
    // and then sleeps indefinitely
    m_ev.signal_eviction_thread();
    toku_mutex_unlock(&m_ev.m_ev_thread_lock);
    // sleep for one second to ensure eviction thread picks up new period
    usleep(1*1024*1024);
}

// test that verifies that counts, such as m_size_current
// are accurately maintained
void evictor_unit_test::verify_ev_counts() {
    long limit = 10;
    long expected_m_size_reserved = limit/4;
    ZERO_STRUCT(m_ev);
    m_ev.init(limit, &m_pl, &m_cf_list, m_kb, 0);
    this->verify_ev_init(limit);

    m_ev.add_to_size_current(1);
    assert(m_ev.m_size_current == 1);
    assert(m_ev.m_size_reserved == expected_m_size_reserved);
    assert(read_partitioned_counter(m_ev.m_size_leaf) == 0);
    assert(read_partitioned_counter(m_ev.m_size_nonleaf) == 0);
    assert(read_partitioned_counter(m_ev.m_size_rollback) == 0);
    assert(read_partitioned_counter(m_ev.m_size_cachepressure) == 0);
    assert(m_ev.m_size_evicting == 0);

    m_ev.add_to_size_current(3);
    assert(m_ev.m_size_current == 4);

    m_ev.remove_from_size_current(4);
    assert(m_ev.m_size_current == 0);
    assert(m_ev.m_size_reserved == expected_m_size_reserved);    

    PAIR_ATTR attr = {
        .size = 1, 
        .nonleaf_size = 2, 
        .leaf_size = 3, 
        .rollback_size = 4, 
        .cache_pressure_size = 5,
        .is_valid = true
    };

    m_ev.add_pair_attr(attr);
    assert(m_ev.m_size_current == 1);
    assert(read_partitioned_counter(m_ev.m_size_nonleaf) == 2);
    assert(read_partitioned_counter(m_ev.m_size_leaf) == 3);
    assert(read_partitioned_counter(m_ev.m_size_rollback) == 4);
    assert(read_partitioned_counter(m_ev.m_size_cachepressure) == 5);
    m_ev.remove_pair_attr(attr);
    assert(m_ev.m_size_current == 0);
    assert(read_partitioned_counter(m_ev.m_size_leaf) == 0);
    assert(read_partitioned_counter(m_ev.m_size_nonleaf) == 0);
    assert(read_partitioned_counter(m_ev.m_size_rollback) == 0);
    assert(read_partitioned_counter(m_ev.m_size_cachepressure) == 0);
    
    PAIR_ATTR other_attr = {
        .size = 2, 
        .nonleaf_size = 3, 
        .leaf_size = 4, 
        .rollback_size = 5, 
        .cache_pressure_size = 6,
        .is_valid = true
    };
    m_ev.change_pair_attr(attr, other_attr);
    assert(m_ev.m_size_current == 1);
    assert(read_partitioned_counter(m_ev.m_size_leaf) == 1);
    assert(read_partitioned_counter(m_ev.m_size_nonleaf) == 1);
    assert(read_partitioned_counter(m_ev.m_size_rollback) == 1);
    assert(read_partitioned_counter(m_ev.m_size_cachepressure) == 1);

    m_ev.m_size_current = 0;
    m_ev.destroy();
    this->verify_ev_destroy();
}

// test to verify the functionality surrounding m_size_reserved
void evictor_unit_test::verify_ev_m_size_reserved() {
    long limit = 400;
    long expected_m_size_reserved = 100; //limit/4
    ZERO_STRUCT(m_ev);
    m_ev.init(limit, &m_pl, &m_cf_list, m_kb, 0);
    this->verify_ev_init(limit);
    assert(m_ev.m_size_reserved == expected_m_size_reserved);
    m_ev.m_num_eviction_thread_runs = 0;
    m_ev.reserve_memory(0.5, 0);
    assert(m_ev.m_size_reserved == 100+150); //100 original, 150 from last call
    assert(m_ev.m_size_current == 150);
    assert(m_ev.m_size_evicting == 0);
    usleep(1*1024*1024); // sleep to give eviction thread a chance to wake up
    assert(m_ev.m_num_eviction_thread_runs > 0);
    
    m_ev.m_size_current = 0;
    m_ev.destroy();
    this->verify_ev_destroy();
}

// test to verify functionality of handling cache pressure,
// ensures that wait_for_cache_pressure_to_subside works correctly,
// that decrease_m_size_evicting works correctly, and the logic for when to wake
// threads up works correctly
void evictor_unit_test::verify_ev_handling_cache_pressure() {
    long limit = 400;
    ZERO_STRUCT(m_ev);
    m_ev.init(limit, &m_pl, &m_cf_list, m_kb, 0);
    this->verify_ev_init(limit);
    m_ev.m_low_size_watermark = 400;
    m_ev.m_low_size_hysteresis = 400;
    m_ev.m_high_size_hysteresis = 500;
    m_ev.m_high_size_watermark = 500;
    m_ev.m_size_current = 500;
    
    m_ev.m_num_eviction_thread_runs = 0;

    // test that waiting for cache pressure wakes eviction thread
    assert(m_ev.m_num_sleepers == 0);
    m_ev.wait_for_cache_pressure_to_subside();
    assert(m_ev.m_num_eviction_thread_runs == 1);
    assert(m_ev.m_num_sleepers == 0);

    m_ev.m_num_eviction_thread_runs = 0;
    m_ev.m_size_evicting = 101;
    m_ev.decrease_size_evicting(101);
    usleep(1*1024*1024);
    // should not have been signaled because we have no sleepers
    assert(m_ev.m_num_eviction_thread_runs == 0);

    m_ev.m_num_eviction_thread_runs = 0;
    m_ev.m_size_evicting = 101;
    m_ev.m_num_sleepers = 1;
    m_ev.decrease_size_evicting(2);
    usleep(1*1024*1024);
    // should have been signaled because we have sleepers
    assert(m_ev.m_num_eviction_thread_runs == 1);
    assert(m_ev.m_num_sleepers == 1); // make sure fake sleeper did not go away
    
    m_ev.m_num_eviction_thread_runs = 0;
    m_ev.m_size_evicting = 102;
    m_ev.m_num_sleepers = 1;
    m_ev.decrease_size_evicting(1);
    usleep(1*1024*1024);
    // should not have been signaled because we did not go to less than 100
    assert(m_ev.m_num_eviction_thread_runs == 0);
    assert(m_ev.m_num_sleepers == 1); // make sure fake sleeper did not go away
    
    m_ev.m_size_evicting = 0;
    m_ev.m_num_sleepers = 0;
    m_ev.m_size_current = 0;
    m_ev.destroy();
    this->verify_ev_destroy();
}

void evictor_unit_test::run_test() {
    this->verify_ev_counts();
    this->verify_ev_m_size_reserved();
    this->verify_ev_handling_cache_pressure();
    return;
}

int
test_main(int argc, const char *argv[]) {
    default_parse_args(argc, argv);
    evictor_unit_test ev_test;
    ev_test.init();
    ev_test.run_test();
    ev_test.destroy();
    return 0;
}
