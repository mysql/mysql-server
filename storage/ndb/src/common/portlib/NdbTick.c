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
#include <NdbTick.h>

#define NANOSEC_PER_SEC  1000000000
#define MICROSEC_PER_SEC 1000000
#define MILLISEC_PER_SEC 1000
#define MICROSEC_PER_MILLISEC 1000
#define MILLISEC_PER_NANOSEC 1000000


#ifdef HAVE_CLOCK_GETTIME
NDB_TICKS NdbTick_CurrentMillisecond(void)
{
  struct timespec tick_time;
  clock_gettime(CLOCK_REALTIME, &tick_time);

  return 
    ((NDB_TICKS)tick_time.tv_sec)  * ((NDB_TICKS)MILLISEC_PER_SEC) +
    ((NDB_TICKS)tick_time.tv_nsec) / ((NDB_TICKS)MILLISEC_PER_NANOSEC);
}

int 
NdbTick_CurrentMicrosecond(NDB_TICKS * secs, Uint32 * micros){
  struct timespec t;
  int res = clock_gettime(CLOCK_REALTIME, &t);
  * secs   = t.tv_sec;
  * micros = t.tv_nsec / 1000;
  return res;
}
#else
NDB_TICKS NdbTick_CurrentMillisecond(void)
{
  struct timeval tick_time;
  gettimeofday(&tick_time, 0);

  return 
    ((NDB_TICKS)tick_time.tv_sec)  * ((NDB_TICKS)MILLISEC_PER_SEC) +
    ((NDB_TICKS)tick_time.tv_usec) / ((NDB_TICKS)MICROSEC_PER_MILLISEC);
}

int 
NdbTick_CurrentMicrosecond(NDB_TICKS * secs, Uint32 * micros){
  struct timeval tick_time;
  int res = gettimeofday(&tick_time, 0);

  if(secs==0) {
    NDB_TICKS local_secs   = tick_time.tv_sec;
    *micros = tick_time.tv_usec;
    *micros = local_secs*1000000+*micros;    
  } else {
      * secs   = tick_time.tv_sec;
      * micros = tick_time.tv_usec;
    }
  return res;
}

#endif
int
NdbTick_getMicroTimer(struct MicroSecondTimer* input_timer)
{
  NDB_TICKS secs;
  Uint32 mics;
  int ret_value;
  ret_value = NdbTick_CurrentMicrosecond(&secs, &mics);
  input_timer->seconds = secs;
  input_timer->micro_seconds = (NDB_TICKS)mics;
  return ret_value;
}

NDB_TICKS
NdbTick_getMicrosPassed(struct MicroSecondTimer start,
                        struct MicroSecondTimer stop)
{
  NDB_TICKS ret_value = (NDB_TICKS)0;
  if (start.seconds < stop.seconds) {
    NDB_TICKS sec_passed = stop.seconds - start.seconds;
    ret_value = ((NDB_TICKS)MICROSEC_PER_SEC) * sec_passed;
  } else if (start.seconds > stop.seconds) {
    return ret_value;
  }
  if (start.micro_seconds < stop.micro_seconds) {
    ret_value += (stop.micro_seconds - start.micro_seconds);
  } else if (ret_value != (NDB_TICKS)0) {
    ret_value -= (start.micro_seconds - stop.micro_seconds);
  }
  return ret_value;
}
