/* Copyright (C) 2003 MySQL AB

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


#include <windows.h>
#include "NdbTick.h"

/*
#define FILETIME_PER_MICROSEC 10
#define FILETIME_PER_MILLISEC 10000
#define FILETIME_PER_SEC 10000000


NDB_TICKS NdbTick_CurrentMillisecond(void)
{
    ULONGLONG ullTime;
    GetSystemTimeAsFileTime((LPFILETIME)&ullTime);
    return (ullTime / FILETIME_PER_MILLISEC);
}

int 
NdbTick_CurrentMicrosecond(NDB_TICKS * secs, Uint32 * micros)
{
    ULONGLONG ullTime;
    GetSystemTimeAsFileTime((LPFILETIME)&ullTime);
    *secs   = (ullTime / FILETIME_PER_SEC);
    *micros = (Uint32)((ullTime % FILETIME_PER_SEC) / FILETIME_PER_MICROSEC);
    return 0;
}
*/


NDB_TICKS NdbTick_CurrentMillisecond(void)
{
  LARGE_INTEGER liCount, liFreq;
  QueryPerformanceCounter(&liCount);
  QueryPerformanceFrequency(&liFreq);
  return (liCount.QuadPart*1000) / liFreq.QuadPart;
}

int 
NdbTick_CurrentMicrosecond(NDB_TICKS * secs, Uint32 * micros)
{
  LARGE_INTEGER liCount, liFreq;
  QueryPerformanceCounter(&liCount);
  QueryPerformanceFrequency(&liFreq);
  *secs = liCount.QuadPart / liFreq.QuadPart;
  liCount.QuadPart -= *secs * liFreq.QuadPart;
  *micros = (liCount.QuadPart*1000000) / liFreq.QuadPart;
  return 0;
}
