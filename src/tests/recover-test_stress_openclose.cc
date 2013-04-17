/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#include "stress_openclose.h"


int
test_main(int argc, char *const argv[]) {
    struct cli_args args = get_default_args();
    parse_stress_test_args(argc, argv, &args);
    // checkpointing is a part of the ref count, so do it often
    args.env_args.checkpointing_period = 5;
    // very small dbs, so verification scans are short and sweet
    args.num_elements = 1000;
    // it's okay for update to get DB_LOCK_NOTGRANTED, etc.
    args.crash_on_operation_failure = false;

    // set crash at end to true for the recovery version
    // then run the test or run recovery, depending on args
    stress_openclose_crash_at_end = true;
    if (args.do_test_and_crash) {
        stress_test_main(&args);
    }
    if (args.do_recover) {
        stress_recover(&args);
    }
    return 0;
}
