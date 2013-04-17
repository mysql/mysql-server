/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
#ident "$Id: test_stress1.c 35324 2011-10-04 01:48:45Z zardosht $"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>

#include <toku_pthread.h>
#include <unistd.h>
#include <memory.h>
#include <sys/stat.h>
#include <db.h>

#include "threaded_stress_test_helpers.h"
#include "recover-test_crash_in_flusher_thread.h"


int
test_main(int argc, char *const argv[]) {
    state_to_crash = 7;
    return run_recover_ft_test(argc, argv);
}
