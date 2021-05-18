/*
 Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.
 
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License, version 2.0,
 as published by the Free Software Foundation.

 This program is also distributed with certain software (including
 but not limited to OpenSSL) that is licensed under separate terms,
 as designated in a particular file or component or in included license
 documentation.  The authors of MySQL hereby grant you an additional
 permission to link the program and your derivative works with the
 separately licensed software that they have included with MySQL.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License, version 2.0, for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */
#ifndef NDBMEMCACHE_TIMING_H
#define NDBMEMCACHE_TIMING_H

#include "ndbmemcache_global.h"
#include "ndbmemcache_config.h"

#include <sys/time.h>
#include <pthread.h>

#if defined(HAVE_MACH_MACH_TIME_H)
#include <mach/mach_time.h>
typedef Uint64 time_point_t;

#elif defined(HAVE_GETHRTIME)
typedef hrtime_t time_point_t;

#elif defined(HAVE_CLOCK_GETTIME)
#include <time.h>
typedef Uint64 time_point_t;

#else 
typedef int time_point_t;
#endif 


#ifdef CLOCK_MONOTONIC
#define PREFERRED_CLOCK CLOCK_MONOTONIC
#else
#define PREFERRED_CLOCK CLOCK_REALTIME
#endif


/* On platforms without gethrvtime(), define get_thread_time() to 0. */
#ifdef HAVE_GETHRVTIME
#define get_thread_vtime() gethrvtime()
#else
#define get_thread_vtime() 0
#endif


DECLARE_FUNCTIONS_WITH_C_LINKAGE
/**
 *
 * @param point a context value maintained between calls to timing_point(). 
 *
 * @return the number of elapsed nanoseconds since the previous timing point. 
 *  On the first call to timing_point() in a sequence, you must disregard the
 *  return value.
 */
Uint64 timing_point(time_point_t *point);

/**
 * Initialize a condition variable for use with timeouts
 */
void init_condition_var(pthread_cond_t *c);

/** 
 * Fetch the current time into timespec t in a way that can be used to set
 * wait timeout on a POSIX condition variable
 */
int timespec_get_time(struct timespec *t);
 
/** 
 * Add some number of milliseconds to timespec t
 */
void timespec_add_msec(struct timespec *t, unsigned msec);

END_FUNCTIONS_WITH_C_LINKAGE

#endif
