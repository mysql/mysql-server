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


#include "NdbTick.h"
#include <time.h>

#define NANOSEC_PER_SEC  1000000000
#define MICROSEC_PER_SEC 1000000
#define MILLISEC_PER_SEC 1000
#define MICROSEC_PER_MILLISEC 1000
#define MILLISEC_PER_NANOSEC 1000000

#ifdef NDB_OSE
NDB_TICKS NdbTick_CurrentMillisecond(void)
{
  return get_ticks()*4;
}
#include <rtc.h>
int 
NdbTick_CurrentMicrosecond(NDB_TICKS * secs, Uint32 * micros){
  struct TimePair tvp;
  rtc_get_time(&tvp);
  * secs   = tvp.seconds;
  * micros = tvp.micros;
  return 0;
}

#endif

#if defined NDB_SOFTOSE
NDB_TICKS NdbTick_CurrentMillisecond(void)
{
  /**
   * Depends on the interval counter in solaris
   * that means each "tick" in OSE is really 10 milliseconds
   */
  return get_ticks()*10;
}

#include <rtc.h>
int 
NdbTick_CurrentMicrosecond(NDB_TICKS * secs, Uint32 * micros){
  struct TimePair tvp;
  rtc_get_time(&tvp);
  * secs   = tvp.seconds;
  * micros = tvp.micros;
  return 0;
}
#endif

