/* Copyright (C) 2008 MySQL AB

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

/*
 * NOTE: This file only built on Windows
 */
#include <my_global.h>
#include <Windows.h>
#include <my_times.h>

clock_t times(struct tms *buf)
{
  BOOL r;
  FILETIME create, exit, kernel, user;
  ULARGE_INTEGER ulint;
  LARGE_INTEGER ticks;

  if(!buf)
  {
    errno= EINVAL;
    return -1;
  }

  r= GetProcessTimes(GetCurrentProcess(), &create, &exit, &kernel, &user);

  if(r==0)
  {
    errno= GetLastError();
    return -1;
  }

  ulint.LowPart= kernel.dwLowDateTime;
  ulint.HighPart= kernel.dwHighDateTime;
  buf->tms_stime= ulint.QuadPart;
  buf->tms_cstime= ulint.QuadPart;

  ulint.LowPart= user.dwLowDateTime;
  ulint.HighPart= user.dwHighDateTime;
  buf->tms_utime= ulint.QuadPart;
  buf->tms_cutime= ulint.QuadPart;

  if(QueryPerformanceCounter(&ticks)==0)
  {
    errno= GetLastError();
    return -1;
  }

  return ticks.QuadPart;
}
