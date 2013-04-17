/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2012 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#include <config.h>
#include "toku_affinity.h"

#if defined(HAVE_SCHED_GETAFFINITY)

int toku_getaffinity(pid_t pid, size_t cpusetsize, toku_cpuset_t *cpusetp) {
    return sched_getaffinity(pid, cpusetsize, cpusetp);
}
int toku_setaffinity(pid_t pid, size_t cpusetsize, const toku_cpuset_t *cpusetp) {
    return sched_setaffinity(pid, cpusetsize, cpusetp);
}

#elif defined(HAVE_CPUSET_GETAFFINITY)

int toku_getaffinity(pid_t pid, size_t cpusetsize, toku_cpuset_t *cpusetp) {
    return cpuset_getaffinity(CPU_LEVEL_CPUSET, CPU_WHICH_PID, pid, cpusetsize, cpusetp);
}
int toku_setaffinity(pid_t pid, size_t cpusetsize, const toku_cpuset_t *cpusetp) {
    return cpuset_setaffinity(CPU_LEVEL_CPUSET, CPU_WHICH_PID, pid, cpusetsize, cpusetp);
}

#else

// dummy implementation to get rid of unused warnings etc
int toku_getaffinity(pid_t pid __attribute__((unused)),
                     size_t cpusetsize __attribute__((unused)),
                     toku_cpuset_t *cpusetp) {
    TOKU_CPU_ZERO(cpusetp);
    return 0;
}
int toku_setaffinity(pid_t pid __attribute__((unused)),
                     size_t cpusetsize __attribute__((unused)),
                     const toku_cpuset_t *cpusetp __attribute__((unused))) {
    return 0;
}

#endif
