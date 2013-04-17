/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2012 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#ifndef TOKU_AFFINITY_H
#define TOKU_AFFINITY_H

#include <config.h>
#include <stddef.h>
#include <sys/types.h>

#if defined(HAVE_SCHED_GETAFFINITY)
# include <sched.h>
typedef cpu_set_t toku_cpuset_t;
# define TOKU_CPU_ZERO(p) CPU_ZERO(p)
# define TOKU_CPU_SET(n, p) CPU_SET(n, p)
#elif defined(HAVE_CPUSET_GETAFFINITY)
# include <sys/param.h>
# include <sys/cpuset.h>
typedef cpuset_t toku_cpuset_t;
# define TOKU_CPU_ZERO(p) CPU_ZERO(p)
# define TOKU_CPU_SET(n, p) CPU_SET(n, p)
#else
// dummy implementation to get rid of unused warnings etc
typedef int toku_cpuset_t;
# define TOKU_CPU_ZERO(p) (*p = 0)
# define TOKU_CPU_SET(n, p) (((void) n, (void) p))
#endif

// see sched_getaffinity(2)
int toku_getaffinity(pid_t pid, size_t cpusetsize, toku_cpuset_t *cpusetp);
// see sched_setaffinity(2)
int toku_setaffinity(pid_t pid, size_t cpusetsize, const toku_cpuset_t *cpusetp);

#endif // TOKU_AFFINITY_H
