/*
   Copyright (C) 2003 MySQL AB
    All rights reserved. Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

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
  ulonglong time, timemicro, micropart, secpart;

  GetSystemTimeAsFileTime((FILETIME*)&time);
  timemicro = time/10;
  
  secpart   = timemicro/1000000;
  micropart = timemicro%1000000;
  assert(micropart <= ULONG_MAX);
  assert(secpart*1000000+micropart == timemicro);

  *micros = (Uint32)micropart;
  *secs = secpart;

  return 0;
}
