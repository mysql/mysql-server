/* Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <NdbGetRUsage.h>

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif

#ifndef _WIN32
static
Uint64
micros(struct timeval val)
{
  return
    (Uint64)val.tv_sec * (Uint64)1000000 + val.tv_usec;
}
#endif

extern "C"
int
Ndb_GetRUSage(ndb_rusage* dst)
{
  int res = -1;
#ifdef HAVE_GETRUSAGE
  struct rusage tmp;
#ifdef RUSAGE_THREAD
  res = getrusage(RUSAGE_THREAD, &tmp);
#elif defined RUSAGE_LWP
  res = getrusage(RUSAGE_LWP, &tmp);
#endif

  if (res == 0)
  {
    dst->ru_utime = micros(tmp.ru_utime);
    dst->ru_stime = micros(tmp.ru_stime);
    dst->ru_minflt = tmp.ru_minflt;
    dst->ru_majflt = tmp.ru_majflt;
    dst->ru_nvcsw = tmp.ru_nvcsw;
    dst->ru_nivcsw = tmp.ru_nivcsw;
  }
#endif

  if (res != 0)
  {
    bzero(dst, sizeof(* dst));
  }
  return res;
}
