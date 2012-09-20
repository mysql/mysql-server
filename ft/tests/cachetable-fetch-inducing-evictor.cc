/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id: cachetable-simple-verify.cc 45903 2012-07-19 13:06:39Z leifwalsh $"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#include "includes.h"
#include "test.h"

bool pf_called;

enum pin_evictor_test_type {
    pin_in_memory,
    pin_fetch,
    pin_partial_fetch
};

static bool pf_req_callback(void* UU(ftnode_pv), void* UU(read_extraargs)) {
    return true;
}

static int pf_callback(void* UU(ftnode_pv), void* UU(disk_data), void* UU(read_extraargs), int UU(fd), PAIR_ATTR* sizep) {
    *sizep = make_pair_attr(8);
    return 0;
}


static void
cachetable_test (enum pin_evictor_test_type test_type, bool nonblocking) {
    const int test_limit = 7;
    int r;
    CACHETABLE ct;
    toku_cachetable_create(&ct, test_limit, ZERO_LSN, NULL_LOGGER);
    evictor_test_helpers::set_hysteresis_limits(&ct->ev, test_limit, test_limit);
    evictor_test_helpers::disable_ev_thread(&ct->ev);
    char fname1[] = __SRCFILE__ "test1.dat";
    unlink(fname1);
    CACHEFILE f1;
    r = toku_cachetable_openf(&f1, ct, fname1, O_RDWR|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO); assert(r == 0);
  
    void* v1;
    long s1;
    CACHETABLE_WRITE_CALLBACK wc = def_write_callback(NULL);
    r = toku_cachetable_get_and_pin(f1, make_blocknum(1), 1, &v1, &s1, wc, def_fetch, def_pf_req_callback, def_pf_callback, true, NULL);
    r = toku_test_cachetable_unpin(f1, make_blocknum(1), 1, CACHETABLE_CLEAN, make_pair_attr(8));
  
    // at this point, we should have 8 bytes of data in a cachetable that supports 7
    // adding data via get_and_pin or get_and_pin_nonblocking should induce eviction  
    uint64_t old_num_ev_runs = 0;
    uint64_t new_num_ev_runs = 0;
    if (test_type == pin_in_memory) {
        old_num_ev_runs = evictor_test_helpers::get_num_eviction_runs(&ct->ev);
        if (nonblocking) {
            r = toku_cachetable_get_and_pin_nonblocking(f1, make_blocknum(1), 1, &v1, &s1, wc, def_fetch, def_pf_req_callback, def_pf_callback, PL_WRITE_EXPENSIVE, NULL, NULL);
            assert_zero(r);
        }
        else {
            r = toku_cachetable_get_and_pin(f1, make_blocknum(1), 1, &v1, &s1, wc, def_fetch, def_pf_req_callback, def_pf_callback, true, NULL);
            assert_zero(r);
        }
        new_num_ev_runs = evictor_test_helpers::get_num_eviction_runs(&ct->ev);
        assert(new_num_ev_runs == old_num_ev_runs);
        r = toku_test_cachetable_unpin(f1, make_blocknum(1), 1, CACHETABLE_CLEAN, make_pair_attr(8));
        assert_zero(r);
    }
    else if (test_type == pin_fetch) {
        old_num_ev_runs = evictor_test_helpers::get_num_eviction_runs(&ct->ev);
        if (nonblocking) {
            r = toku_cachetable_get_and_pin_nonblocking(f1, make_blocknum(2), 2, &v1, &s1, wc, def_fetch, def_pf_req_callback, def_pf_callback, PL_WRITE_EXPENSIVE, NULL, NULL);
            assert(r == TOKUDB_TRY_AGAIN);
            new_num_ev_runs = evictor_test_helpers::get_num_eviction_runs(&ct->ev);
            assert(new_num_ev_runs > old_num_ev_runs);
        }
        else {
            r = toku_cachetable_get_and_pin(f1, make_blocknum(2), 2, &v1, &s1, wc, def_fetch, def_pf_req_callback, def_pf_callback, true, NULL);
            assert_zero(r);
            new_num_ev_runs = evictor_test_helpers::get_num_eviction_runs(&ct->ev);
            assert(new_num_ev_runs > old_num_ev_runs);
            r = toku_test_cachetable_unpin(f1, make_blocknum(2), 2, CACHETABLE_CLEAN, make_pair_attr(8));
            assert_zero(r);
        }
    }
    else if (test_type == pin_partial_fetch) {
        old_num_ev_runs = evictor_test_helpers::get_num_eviction_runs(&ct->ev);
        if (nonblocking) {
            r = toku_cachetable_get_and_pin_nonblocking(f1, make_blocknum(1), 1, &v1, &s1, wc, def_fetch, pf_req_callback, pf_callback, PL_WRITE_EXPENSIVE, NULL, NULL);
            assert(r == TOKUDB_TRY_AGAIN);
            new_num_ev_runs = evictor_test_helpers::get_num_eviction_runs(&ct->ev);
            assert(new_num_ev_runs > old_num_ev_runs);
        }
        else {
            r = toku_cachetable_get_and_pin(f1, make_blocknum(1), 1, &v1, &s1, wc, def_fetch, pf_req_callback, pf_callback, true, NULL);
            assert_zero(r);
            new_num_ev_runs = evictor_test_helpers::get_num_eviction_runs(&ct->ev);
            assert(new_num_ev_runs > old_num_ev_runs);
            r = toku_test_cachetable_unpin(f1, make_blocknum(1), 1, CACHETABLE_CLEAN, make_pair_attr(8));
            assert_zero(r);
        }
    }
    else {
        assert(false);
    }
  
    toku_cachetable_verify(ct);
    r = toku_cachefile_close(&f1, false, ZERO_LSN); assert(r == 0);
    toku_cachetable_close(&ct);
}

int
test_main(int argc, const char *argv[]) {
    default_parse_args(argc, argv);
    cachetable_test(pin_in_memory, true);
    cachetable_test(pin_fetch, true);
    cachetable_test(pin_partial_fetch, true);
    cachetable_test(pin_in_memory, false);
    cachetable_test(pin_fetch, false);
    cachetable_test(pin_partial_fetch, false);
    return 0;
}
