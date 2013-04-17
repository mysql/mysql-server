/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
#include <stdio.h>
#include <stdlib.h>
#include <toku_stdint.h>
#include <unistd.h>
#include <toku_assert.h>
#include "toku_os.h"
#include <sched.h>

#if defined(HAVE_SCHED_GETAFFINITY)
static void set_cpuset(cpu_set_t *cpuset, int ncpus) {
    CPU_ZERO(cpuset);
    for (int i = 0; i < ncpus; i++)
        CPU_SET(i, cpuset);
}
#endif

int main(void) {
    int r;
    r = unsetenv("TOKU_NCPUS"); 
    assert(r == 0);

    int max_cpus = sysconf(_SC_NPROCESSORS_ONLN);
    assert(toku_os_get_number_active_processors() == max_cpus);

    // change the processor affinity and verify that the correct number is computed
    for (int ncpus = 1; ncpus <= max_cpus; ncpus++) {
#if defined(HAVE_SCHED_GETAFFINITY)
        cpu_set_t cpuset; 
        set_cpuset(&cpuset, ncpus);
        r = sched_setaffinity(getpid(), sizeof cpuset, &cpuset);
        assert(r == 0);

        assert(toku_os_get_number_active_processors() == ncpus);
#endif
    }

    // change the TOKU_NCPUS env variable and verify that the correct number is computed
    for (int ncpus = 1; ncpus <= max_cpus; ncpus++) {
        char ncpus_str[32];
        sprintf(ncpus_str, "%d", ncpus);
        r = setenv("TOKU_NCPUS", ncpus_str, 1);
        assert(r == 0);

        assert(toku_os_get_number_active_processors() == ncpus);
    }

    return 0;
}
