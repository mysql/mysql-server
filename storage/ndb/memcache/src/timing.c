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
#include "timing.h"

/** SECTION 1.  Implementations of timing_point() */

#ifdef HAVE_MACH_MACH_TIME_H

Uint64 timing_point(time_point_t *t) {
  static mach_timebase_info_data_t conversion = {0,0};  
  time_point_t old = *t;
  Uint64 mach_time_diff;
  
  /* Initialize once */
  if (conversion.denom == 0) mach_timebase_info(&conversion);
  
  *t = mach_absolute_time();
  
  if(old == 0) return 0;  
  
  mach_time_diff = *t - old;
  
  return (mach_time_diff * conversion.numer / conversion.denom);
}

#elif defined(HAVE_GETHRTIME)

Uint64 timing_point(time_point_t *t) {
  time_point_t old = *t;
  
  *t = gethrtime();
  
  if(old == 0) return 0;  
  return (*t - old);
}

#elif defined(HAVE_CLOCK_GETTIME)

Uint64 timing_point(time_point_t *t) {
  struct timespec ts;
  time_point_t old = *t;

  clock_gettime(CLOCK_MONOTONIC, &ts);
  *t  = ts.tv_sec * 1000000000;
  *t += ts.tv_nsec;
  
  if(old == 0) return 0;  
  return (*t - old);
}

#else 

Uint64 timing_point(time_point_t *t) {
  return 0;
}

#endif


/* SECTION 2.  Implementations used for condition variable timers */

#ifdef _POSIX_TIMERS
int timespec_get_time(struct timespec *t) {
  return clock_gettime(PREFERRED_CLOCK, t);
}

void init_condition_var(pthread_cond_t *c) {
  pthread_condattr_t attr;
  pthread_condattr_init(& attr);
  pthread_condattr_setclock(&attr, PREFERRED_CLOCK);
  pthread_cond_init(c, &attr);
  pthread_condattr_destroy(&attr);
}  

#else 

int timespec_get_time(struct timespec *t)  {
  int r;
  struct timeval tick_time;
  r = gettimeofday(&tick_time, 0);
  if(r == 0) {
    t->tv_sec  = tick_time.tv_sec;
    t->tv_nsec = tick_time.tv_usec * 1000;
  }
  return r;
}

void init_condition_var(pthread_cond_t *c) {
  pthread_cond_init(c, NULL);
}

#endif

void timespec_add_msec(struct timespec * t, unsigned msecs) {  
  t->tv_sec  += (msecs / 1000);
  t->tv_nsec += (msecs % 1000) * 1000000;
  while(t->tv_nsec >= 1000000000) {
    t->tv_nsec -= 1000000000;
    t->tv_sec  += 1;
  }
}
