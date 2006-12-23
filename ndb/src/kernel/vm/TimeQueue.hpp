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

#ifndef TimeQueue_H
#define TimeQueue_H

#include <kernel_types.h>
#include "Prio.hpp"

#define MAX_NO_OF_SHORT_TQ 512
#define MAX_NO_OF_LONG_TQ 512
#define MAX_NO_OF_TQ (MAX_NO_OF_SHORT_TQ + MAX_NO_OF_LONG_TQ)
#define NULL_TQ_ENTRY 65535

class Signal;

struct TimeStruct
{
  Uint16 delay_time;
  Uint16 job_index;
};

union TimerEntry
{
  struct TimeStruct time_struct;
  Uint32 copy_struct;
};

class TimeQueue
{
public:
  TimeQueue();
  ~TimeQueue();

  void   insert(Signal* signal, BlockNumber bnr,
		GlobalSignalNumber gsn, Uint32 delayTime);
  void   clear();
  void   scanTable(); // Called once per millisecond
  Uint32 getIndex();
  void   releaseIndex(Uint32 aIndex);
  void   recount_timers();
  
private:
  TimerEntry  theShortQueue[MAX_NO_OF_SHORT_TQ];
  TimerEntry  theLongQueue[MAX_NO_OF_LONG_TQ];
  Uint16     theFreeIndex[MAX_NO_OF_TQ];
};

#endif
