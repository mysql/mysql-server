/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/* get time since epoc in 100 nanosec units */
/* thus to get the current time we should use the system function
   with the highest possible resolution */

#ifdef __NETWARE__
#include <nks/time.h>
#endif

#include "mysys_priv.h"
ulonglong my_getsystime()
{
#ifdef HAVE_CLOCK_GETTIME
  struct timespec tp;
  clock_gettime(CLOCK_REALTIME, &tp);
  return (ulonglong)tp.tv_sec*10000000+(ulonglong)tp.tv_nsec/100;
#elif defined(__WIN__)
#define OFFSET_TO_EPOC ((__int64) 134774 * 24 * 60 * 60 * 1000 * 1000 * 10)
  static __int64 offset=0, freq;
  LARGE_INTEGER t_cnt;
  if (!offset)
  {
    /* strictly speaking there should be a mutex to protect
       initialization section. But my_getsystime() is called from
       UUID() code, and UUID() calls are serialized with a mutex anyway
    */
    LARGE_INTEGER li;
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    li.LowPart=ft.dwLowDateTime;
    li.HighPart=ft.dwHighDateTime;
    offset=li.QuadPart-OFFSET_TO_EPOC;
    QueryPerformanceFrequency(&li);
    freq=li.QuadPart;
    QueryPerformanceCounter(&t_cnt);
    offset-=t_cnt.QuadPart/freq*10000000+t_cnt.QuadPart%freq*10000000/freq;
  }
  QueryPerformanceCounter(&t_cnt);
  return t_cnt.QuadPart/freq*10000000+t_cnt.QuadPart%freq*10000000/freq+offset;
#elif defined(__NETWARE__)
  NXTime_t tm;
  NXGetTime(NX_SINCE_1970, NX_NSECONDS, &tm);
  return (ulonglong)tm/100;
#else
  /* TODO: check for other possibilities for hi-res timestamping */
  struct timeval tv;
  gettimeofday(&tv,NULL);
  return (ulonglong)tv.tv_sec*10000000+(ulonglong)tv.tv_usec*10;
#endif
}
