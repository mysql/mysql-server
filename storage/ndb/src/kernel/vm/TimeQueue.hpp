/*
   Copyright (c) 2003, 2015, Oracle and/or its affiliates. All rights reserved.

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

#ifndef TimeQueue_H
#define TimeQueue_H

#include <kernel_types.h>
#include "Prio.hpp"

#define JAM_FILE_ID 247


#define MAX_NO_OF_ZERO_TQ 128
#define MAX_NO_OF_SHORT_TQ 512
#define MAX_NO_OF_LONG_TQ 512
#define MAX_NO_OF_TQ (MAX_NO_OF_ZERO_TQ + MAX_NO_OF_SHORT_TQ + \
                      MAX_NO_OF_LONG_TQ)
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
  void   scanZeroTimeQueue(); // Called after each doJob call
  Uint32 getIndex();
  void   releaseIndex(Uint32 aIndex);
  void   recount_timers();
  
private:
  TimerEntry  theZeroQueue[MAX_NO_OF_ZERO_TQ];
  TimerEntry  theShortQueue[MAX_NO_OF_SHORT_TQ];
  TimerEntry  theLongQueue[MAX_NO_OF_LONG_TQ];
  Uint16     theFreeIndex[MAX_NO_OF_TQ];
};


#undef JAM_FILE_ID

#endif
