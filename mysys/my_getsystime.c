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

#include "mysys_priv.h"
ulonglong my_getsystime()
{
#ifdef HAVE_CLOCK_GETTIME
  struct timespec tp;
  clock_gettime(CLOCK_REALTIME, &tp);
  return (ulonglong)tp.tv_sec*10000000+(ulonglong)tp.tv_nsec/100;
#elif defined(__WIN__)
  /* TODO: use GetSystemTimeAsFileTime here or
     QueryPerformanceCounter/QueryPerformanceFrequency */
  struct _timeb tb;
  _ftime(&tb);
  return (ulonglong)tb.time*10000000+(ulonglong)tb.millitm*10000;
#else
  /* TODO: check for other possibilities for hi-res timestamping */
  struct timeval tv;
  gettimeofday(&tv,NULL);
  return (ulonglong)tv.tv_sec*10000000+(ulonglong)tv.tv_usec*10;
#endif
}
