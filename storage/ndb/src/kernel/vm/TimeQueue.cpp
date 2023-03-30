/*
   Copyright (c) 2003, 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "TimeQueue.hpp"
#include <ErrorHandlingMacros.hpp>
#include <GlobalData.hpp>
#include <FastScheduler.hpp>
#include <VMSignal.hpp>
#include <SimulatedBlock.hpp>

#define JAM_FILE_ID 273


static const int MAX_TIME_QUEUE_VALUE = 32000;

TimeQueue::TimeQueue()
{
  clear();
}

TimeQueue::~TimeQueue()
{
}

void 
TimeQueue::clear()
{
  globalData.theNextTimerJob = 65535;
  globalData.theCurrentTimer = 0;
  globalData.theZeroTQIndex = 0;
  globalData.theShortTQIndex = 0;
  globalData.theLongTQIndex = 0;
  for (int i = 0; i < MAX_NO_OF_TQ; i++)
    theFreeIndex[i] = i+1;
  theFreeIndex[MAX_NO_OF_TQ - 1] = NULL_TQ_ENTRY;
  globalData.theFirstFreeTQIndex = 0;
}

void 
TimeQueue::insert(Signal25* signal, Uint32 delayTime)
{
  Uint32 regCurrentTime = globalData.theCurrentTimer;
  Uint32 i;
  Uint32 regSave;
  TimerEntry newEntry;
 
  if (delayTime == 0)
    delayTime = 1;

  newEntry.time_struct.delay_time = regCurrentTime + delayTime;
  newEntry.time_struct.job_index = getIndex();
  regSave = newEntry.copy_struct;
  
  globalScheduler.insertTimeQueue(signal, newEntry.time_struct.job_index);
 
  if (delayTime == SimulatedBlock::BOUNDED_DELAY)
  {
    /**
     * Bounded delay signals are put into special zero time queue, no real
     * check of timers here, it will be put back into the
     * job buffer as soon as we complete the execution in the
     * run job buffer loop.
     */
    Uint32 regZeroIndex = globalData.theZeroTQIndex;
    if (regZeroIndex < MAX_NO_OF_ZERO_TQ - 1)
    {
      theZeroQueue[regZeroIndex].copy_struct = newEntry.copy_struct;
      globalData.theZeroTQIndex = regZeroIndex + 1;
    }
    else
    {
      ERROR_SET(ecError, NDBD_EXIT_TIME_QUEUE_ZERO, 
                "Too many in Zero Time Queue", "TimeQueue.C" );
    }
    return;
  }

  if (newEntry.time_struct.delay_time < globalData.theNextTimerJob)
    globalData.theNextTimerJob = newEntry.time_struct.delay_time;
  if (delayTime < 100){
    Uint32 regShortIndex = globalData.theShortTQIndex;
    if (regShortIndex == 0){
      theShortQueue[0].copy_struct = newEntry.copy_struct;
    } else if (regShortIndex >= MAX_NO_OF_SHORT_TQ - 1) {
      ERROR_SET(ecError, NDBD_EXIT_TIME_QUEUE_SHORT, 
		"Too many in Short Time Queue", "TimeQueue.C" );
    } else {
      for (i = 0; i < regShortIndex; i++) {
        if (theShortQueue[i].time_struct.delay_time > 
	    newEntry.time_struct.delay_time)  {
	  
          regSave = theShortQueue[i].copy_struct;
          theShortQueue[i].copy_struct = newEntry.copy_struct;
          break;
	}
      }
      if (i == regShortIndex) {
        theShortQueue[regShortIndex].copy_struct = regSave;
      } else {
        for (i++; i < regShortIndex; i++) {
	  Uint32 regTmp = theShortQueue[i].copy_struct;
	  theShortQueue[i].copy_struct = regSave;
	  regSave = regTmp;
        }
        theShortQueue[regShortIndex].copy_struct = regSave;
      }
    }
    globalData.theShortTQIndex = regShortIndex + 1;
  } else if (delayTime <= (unsigned)MAX_TIME_QUEUE_VALUE) {
    Uint32 regLongIndex = globalData.theLongTQIndex;
    if (regLongIndex == 0) {
      theLongQueue[0].copy_struct = newEntry.copy_struct;
    } else if (regLongIndex >= MAX_NO_OF_LONG_TQ - 1) {
      ERROR_SET(ecError, NDBD_EXIT_TIME_QUEUE_LONG, 
		"Too many in Long Time Queue", "TimeQueue.C" );
    } else {
      for (i = 0; i < regLongIndex; i++) {
        if (theLongQueue[i].time_struct.delay_time > 
	    newEntry.time_struct.delay_time) {
	  
          regSave = theLongQueue[i].copy_struct;
          theLongQueue[i].copy_struct = newEntry.copy_struct;
          break;
        }
      }
      if (i == regLongIndex) {
        theLongQueue[regLongIndex].copy_struct = regSave;
      } else {
        for (i++; i < regLongIndex; i++) {
          Uint32 regTmp = theLongQueue[i].copy_struct;
          theLongQueue[i].copy_struct = regSave;
          regSave = regTmp;
        }
        theLongQueue[regLongIndex].copy_struct = regSave;
      }
    }
    globalData.theLongTQIndex = regLongIndex + 1;
  } else {
    ERROR_SET(ecError, NDBD_EXIT_TIME_QUEUE_DELAY, 
	      "Too long delay for Time Queue", "TimeQueue.C" );
  }
}

void
TimeQueue::scanZeroTimeQueue()
{
  /* Put all jobs in zero time queue into job buffer */
  for (Uint32 i = 0; i < globalData.theZeroTQIndex; i++)
  {
    releaseIndex((Uint32)theZeroQueue[i].time_struct.job_index);
    globalScheduler.scheduleTimeQueue(theZeroQueue[i].time_struct.job_index);
  }
  globalData.theZeroTQIndex = 0;
}

// executes the expired signals;
void
TimeQueue::scanTable()
{
  Uint32 i, j;
  
  globalData.theCurrentTimer++;
  if (globalData.theCurrentTimer == 32000)
    recount_timers();
  if (globalData.theNextTimerJob > globalData.theCurrentTimer)
    return;
  globalData.theNextTimerJob = 65535; // If no more timer jobs
  for (i = 0; i < globalData.theShortTQIndex; i++) {
    if (theShortQueue[i].time_struct.delay_time > globalData.theCurrentTimer){
      break;
    } else {
      releaseIndex((Uint32)theShortQueue[i].time_struct.job_index);
      globalScheduler.scheduleTimeQueue(theShortQueue[i].time_struct.job_index);
    }
  }
  if (i > 0) {
    for (j = i; j < globalData.theShortTQIndex; j++)
      theShortQueue[j - i].copy_struct = theShortQueue[j].copy_struct;
    globalData.theShortTQIndex -= i;
  }
  if (globalData.theShortTQIndex != 0) // If not empty
    globalData.theNextTimerJob = theShortQueue[0].time_struct.delay_time;
  for (i = 0; i < globalData.theLongTQIndex; i++) {
    if (theLongQueue[i].time_struct.delay_time > globalData.theCurrentTimer) {
      break;
    } else {
      releaseIndex((Uint32)theLongQueue[i].time_struct.job_index);
      globalScheduler.scheduleTimeQueue(theLongQueue[i].time_struct.job_index);
    }
  }
  if (i > 0) {
    for (j = i; j < globalData.theLongTQIndex; j++)
      theLongQueue[j - i].copy_struct = theLongQueue[j].copy_struct;
    globalData.theLongTQIndex -= i;
  }  
  if (globalData.theLongTQIndex != 0) // If not empty
    if (globalData.theNextTimerJob > theLongQueue[0].time_struct.delay_time)
      globalData.theNextTimerJob = theLongQueue[0].time_struct.delay_time;
}

void
TimeQueue::recount_timers()
{
  Uint32 i;

  globalData.theCurrentTimer = 0;
  globalData.theNextTimerJob -= 32000;

  for (i = 0; i < globalData.theShortTQIndex; i++)
    theShortQueue[i].time_struct.delay_time -= 32000;
  for (i = 0; i < globalData.theLongTQIndex; i++)
    theLongQueue[i].time_struct.delay_time -= 32000;
}

Uint32
TimeQueue::getIndex()
{
  Uint32 retValue = globalData.theFirstFreeTQIndex;
  globalData.theFirstFreeTQIndex = (Uint32)theFreeIndex[retValue];
  if (retValue >= MAX_NO_OF_TQ)
    ERROR_SET(fatal, NDBD_EXIT_TIME_QUEUE_INDEX, 
	      "Index out of range", "TimeQueue.C" );
  return retValue;
}

void
TimeQueue::releaseIndex(Uint32 aIndex)
{
  theFreeIndex[aIndex] = globalData.theFirstFreeTQIndex;
  globalData.theFirstFreeTQIndex = aIndex;
}


