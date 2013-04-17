/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id: cachetable-simple-verify.cc 45903 2012-07-19 13:06:39Z leifwalsh $"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#include "includes.h"
#include "test.h"
#include "cachetable-internal.h"

class evictor_unit_test {
public:
    evictor m_ev;
    pair_list m_pl;
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
    m_pl.init();
    m_kb = toku_kibbutz_create(1);
}

// destroy class after tests have run
void evictor_unit_test::destroy() {
    m_pl.destroy();
    toku_kibbutz_destroy(m_kb);
}

// test that verifies evictor.init properly worked
void evictor_unit_test::verify_ev_init(long limit) {
    assert(m_ev.m_kibbutz == m_kb);
    assert(m_ev.m_pl == &m_pl);
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
    m_ev.init(limit, &m_pl, m_kb, 0);
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
    m_ev.init(limit, &m_pl, m_kb, 0);
    this->verify_ev_init(limit);
    assert(m_ev.m_size_reserved == expected_m_size_reserved);
    m_ev.m_num_eviction_thread_runs = 0;
    m_ev.reserve_memory(0.5);
    assert(m_ev.m_size_reserved == 100+150); //100 original, 150 from last call
    assert(m_ev.m_size_current == 150);
    assert(m_ev.m_size_evicting == 0);
    usleep(1*1024*1024); // sleep to give eviction thread a chance to wake up
    assert(m_ev.m_num_eviction_thread_runs == 1);
    
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
    m_ev.init(limit, &m_pl, m_kb, 0);
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
