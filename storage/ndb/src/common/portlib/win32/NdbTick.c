/* Copyright (C) 2003 MySQL AB

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

#include <ndb_global.h>
#include "NdbTick.h"

void NdbTick_Init()
{
  return;
}

NDB_TICKS NdbTick_CurrentMillisecond(void)
{
  NDB_TICKS sec;Uint32 usec;
  NdbTick_CurrentMicrosecond(&sec,&usec);
  return sec*1000+usec/1000;
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
