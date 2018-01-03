/*
   Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef MY_SYSTIME_INCLUDED
#define MY_SYSTIME_INCLUDED

/**
  @file include/my_systime.h
  Defines for getting and processing the current system type programmatically.
  Note that these are not monotonic. New code should probably use
  std::chrono::steady_clock instead.
*/

#include <time.h>

#include "my_config.h"
#include "my_inttypes.h"
#include "my_macros.h"

#ifdef _WIN32

#include <windows.h>

static inline void sleep(unsigned long seconds)
{ 
  Sleep(seconds * 1000);
}

/****************************************************************************
** Replacements for localtime_r and gmtime_r
****************************************************************************/

static inline struct tm *localtime_r(const time_t *timep, struct tm *tmp)
{
  localtime_s(tmp, timep);
  return tmp;
}

static inline struct tm *gmtime_r(const time_t *clock, struct tm *res)
{
  gmtime_s(res, clock);
  return res;
}
#endif /* _WIN32 */

C_MODE_START
ulonglong my_getsystime(void);

void set_timespec_nsec(struct timespec *abstime, ulonglong nsec);

void set_timespec(struct timespec *abstime, ulonglong sec);
C_MODE_END

/**
   Compare two timespec structs.

   @retval  1 If ts1 ends after ts2.
   @retval -1 If ts1 ends before ts2.
   @retval  0 If ts1 is equal to ts2.
*/
static inline int cmp_timespec(struct timespec *ts1, struct timespec *ts2)
{
  if (ts1->tv_sec > ts2->tv_sec ||
      (ts1->tv_sec == ts2->tv_sec && ts1->tv_nsec > ts2->tv_nsec))
    return 1;
  if (ts1->tv_sec < ts2->tv_sec ||
      (ts1->tv_sec == ts2->tv_sec && ts1->tv_nsec < ts2->tv_nsec))
    return -1;
  return 0;
}

static inline ulonglong diff_timespec(struct timespec *ts1, struct timespec *ts2)
{
  return (ts1->tv_sec - ts2->tv_sec) * 1000000000ULL +
    ts1->tv_nsec - ts2->tv_nsec;
}

#endif  // MY_SYSTIME_INCLUDED
