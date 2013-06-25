/* Copyright (c) 2000, 2013, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/* Functions to get threads more portable */

#include "mysys_priv.h"
#include <signal.h>
#include <m_string.h>
#include <thr_alarm.h>

#if (defined(__BSD__) || defined(_BSDI_VERSION))
#define SCHED_POLICY SCHED_RR
#else
#define SCHED_POLICY SCHED_OTHER
#endif

/* To allow use of pthread_getspecific with two arguments */

/* localtime_r for SCO 3.2V4.2 */

#if !defined(HAVE_LOCALTIME_R) || !defined(HAVE_GMTIME_R)

extern mysql_mutex_t LOCK_localtime_r;

#endif

#if !defined(HAVE_LOCALTIME_R)
struct tm *localtime_r(const time_t *clock, struct tm *res)
{
  struct tm *tmp;
  mysql_mutex_lock(&LOCK_localtime_r);
  tmp=localtime(clock);
  *res= *tmp;
  mysql_mutex_unlock(&LOCK_localtime_r);
  return res;
}
#endif

#if !defined(HAVE_GMTIME_R)
/* 
  Reentrant version of standard gmtime() function. 
  Needed on some systems which don't implement it.
*/

struct tm *gmtime_r(const time_t *clock, struct tm *res)
{
  struct tm *tmp;
  mysql_mutex_lock(&LOCK_localtime_r);
  tmp= gmtime(clock);
  *res= *tmp;
  mysql_mutex_unlock(&LOCK_localtime_r);
  return res;
}
#endif

/* Some help functions */

int pthread_dummy(int ret)
{
  return ret;
}
