/*
   Copyright (c) 2003, 2016, Oracle and/or its affiliates. All rights reserved.

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

#include "ThreadConfig.hpp"
#include "Emulator.hpp"
#include "GlobalData.hpp"
#include "TimeQueue.hpp"
#include "TransporterRegistry.hpp"
#include "FastScheduler.hpp"
#include "pc.hpp"

#include <GlobalSignalNumbers.h>
#include <BlockNumbers.h>

#include <NdbSleep.h>
#include <NdbTick.h>
#include <NdbOut.hpp>
#include <WatchDog.hpp>

#include <EventLogger.hpp>
extern EventLogger * g_eventLogger;

#include <signaldata/StartOrd.hpp>

#define JAM_FILE_ID 263


ThreadConfig::ThreadConfig()
{
}

ThreadConfig::~ThreadConfig()
{
}

void
ThreadConfig::init()
{
}

void
ThreadConfig::scanZeroTimeQueue()
{
  globalTimeQueue.scanZeroTimeQueue();
}
/**
 * For each millisecond that has passed since this function was last called:
 *   Scan the job buffer and increment the internalTicksCounter 
 *      with 1ms to keep track of where we are
 */
inline
void 
ThreadConfig::scanTimeQueue()
{
  unsigned int maxCounter = 0;
  const NDB_TICKS currTicks = NdbTick_getCurrentTicks();
  globalScheduler.setHighResTimer(currTicks);

  if (NdbTick_Compare(currTicks, globalData.internalTicksCounter) < 0) {
//--------------------------------------------------------------------
// This could occur around 2036 or if the operator decides to change
// time backwards. We cannot know how long time has past since last
// time and we make a best try with 0 milliseconds.
//--------------------------------------------------------------------
    const Uint64 backward = 
      NdbTick_Elapsed(currTicks, globalData.internalTicksCounter).milliSec();

    // Silently ignore sub millisecond backticks.
    // Such 'noise' is unfortunately common even for monotonic timers.
    if (backward > 0)
    {
      g_eventLogger->warning("Time moved backwards with %llu ms", backward);
      globalData.internalTicksCounter = currTicks;
      assert(backward < 100 || !NdbTick_IsMonotonic()); 
    }
    return;
  }//if

  Uint64 elapsed = 
    NdbTick_Elapsed(globalData.internalTicksCounter,currTicks).milliSec();
  if (elapsed > 1500) {
//--------------------------------------------------------------------
// Time has moved forward more than a second. Either it could happen
// if operator changed the time or if the OS has misbehaved badly.
// We set the new time to one second from the past.
//--------------------------------------------------------------------
    g_eventLogger->warning("Time moved forward with %llu ms", elapsed);
    elapsed -= 1000;
    globalData.internalTicksCounter = 
      NdbTick_AddMilliseconds(globalData.internalTicksCounter,elapsed);
  }//if
  while ((elapsed > 0) &&
         (maxCounter < 20)){
    globalData.internalTicksCounter = 
      NdbTick_AddMilliseconds(globalData.internalTicksCounter,1);
    elapsed--;
    maxCounter++;
    globalTimeQueue.scanTable();
  }//while
}//ThreadConfig::scanTimeQueue()


//--------------------------------------------------------------------
// ipControlLoop -- The main loop of ndb.
// Handles the scheduling of signal execution and input/output
// One lap in the loop should take approximately 10 milli seconds
// If the jobbuffer is empty and the laptime is less than 10 milliseconds
// at the end of the loop
// the TransporterRegistry is called in order to sleep on the IO ports
// waiting for another incoming signal to wake us up.
// The timeout value in this call is calculated as (10 ms - laptime)
// This would make ndb use less cpu while improving response time.
//--------------------------------------------------------------------
void ThreadConfig::ipControlLoop(NdbThread* pThis)
{
  Uint32 thread_index = globalEmulatorData.theConfiguration->addThread(pThis,
                                                                 BlockThread);
  globalEmulatorData.theConfiguration->setAllLockCPU(true);

  Uint32 execute_loop_constant =
        globalEmulatorData.theConfiguration->schedulerExecutionTimer();
  Uint32 min_spin_time = 
    globalEmulatorData.theConfiguration->schedulerSpinTimer();
  NDB_TICKS start_ticks, end_ticks, statistics_start_ticks;
  NDB_TICKS yield_ticks;
  Uint32 no_exec_loops = 0;
  Uint32 no_extra_loops = 0;
  Uint32 tot_exec_time = 0;
  Uint32 tot_extra_time = 0;
  Uint32 timeOutMillis;
  Uint32 micros_passed;
  bool spinning;
  bool yield_flag= FALSE;
  Uint32 i = 0;
  Uint32 exec_again;

//--------------------------------------------------------------------
// initialise the counter that keeps track of the current millisecond
//--------------------------------------------------------------------
  globalData.internalTicksCounter = NdbTick_getCurrentTicks();

  Uint32 *watchCounter = globalData.getWatchDogPtr();
  globalEmulatorData.theWatchDog->registerWatchedThread(watchCounter, 0);

  start_ticks = NdbTick_getCurrentTicks();
  globalScheduler.setHighResTimer(start_ticks);
  yield_ticks = statistics_start_ticks = end_ticks = start_ticks;
  while (1)
  {
    timeOutMillis = 0;
//--------------------------------------------------------------------
// We send all messages buffered during execution of job buffers
//--------------------------------------------------------------------
    globalData.incrementWatchDogCounter(6);
    {
      NDB_TICKS before = NdbTick_getCurrentTicks();
      globalTransporterRegistry.performSend();
      NDB_TICKS after = NdbTick_getCurrentTicks();
      globalData.theMicrosSend +=
        NdbTick_Elapsed(before, after).microSec();
    }

//--------------------------------------------------------------------
// Now it is time to check all interfaces. We will send all buffers
// plus checking for any received messages.
//--------------------------------------------------------------------
    if (i++ >= 20)
    {
      execute_loop_constant = 
        globalEmulatorData.theConfiguration->schedulerExecutionTimer();
      min_spin_time = 
        globalEmulatorData.theConfiguration->schedulerSpinTimer();
      globalData.incrementWatchDogCounter(5);
      globalTransporterRegistry.update_connections();
      i = 0;
    }
    spinning = false;
    do
    {
//--------------------------------------------------------------------
// We scan the time queue to see if there are any timed signals that
// is now ready to be executed.
//--------------------------------------------------------------------
      globalData.incrementWatchDogCounter(2);
      scanZeroTimeQueue(); 
      scanTimeQueue(); 

      if (LEVEL_IDLE == globalData.highestAvailablePrio)
      {
//--------------------------------------------------------------------
// The buffers are empty, we need to wait for a while until we continue.
// We cannot wait forever since we can also have timed events.
//--------------------------------------------------------------------
// We set the time to sleep on sockets before waking up to 10
// milliseconds unless we have set spin timer to be larger than 0. In
// this case we spin checking for events on the transporter until we
// have expired the spin time.
//--------------------------------------------------------------------
        timeOutMillis = 10;
        if (min_spin_time && !yield_flag)
        {
          if (spinning)
          {
            end_ticks = NdbTick_getCurrentTicks();
            globalScheduler.setHighResTimer(end_ticks);
          }

          micros_passed = 
            (Uint32)NdbTick_Elapsed(start_ticks, end_ticks).microSec();
          if (micros_passed < min_spin_time)
            timeOutMillis = 0;
        }
      }
      if (spinning && timeOutMillis > 0 && i++ >= 20)
      {
        globalData.incrementWatchDogCounter(5);
        globalTransporterRegistry.update_connections();
        i = 0;
      }

//--------------------------------------------------------------------
// Perform receive before entering execute loop
//--------------------------------------------------------------------
      globalData.incrementWatchDogCounter(7);
      {
        bool poll_flag;
        NDB_TICKS before = NdbTick_getCurrentTicks();
        if (yield_flag)
        {
          globalEmulatorData.theConfiguration->yield_main(thread_index, TRUE);
          poll_flag= globalTransporterRegistry.pollReceive(timeOutMillis);
          globalEmulatorData.theConfiguration->yield_main(thread_index, FALSE);
        }
        else
          poll_flag= globalTransporterRegistry.pollReceive(timeOutMillis);

        NDB_TICKS after = NdbTick_getCurrentTicks();
        yield_ticks = after;
        globalData.theMicrosSleep +=
          NdbTick_Elapsed(before, after).microSec();
        if (poll_flag)
        {
          globalData.incrementWatchDogCounter(8);
          globalTransporterRegistry.performReceive();
        }
        yield_flag= FALSE;
        globalScheduler.setHighResTimer(yield_ticks);
        globalScheduler.postPoll();
        if (min_spin_time > 0 && spinning &&
            (timeOutMillis > 0 ||
            LEVEL_IDLE != globalData.highestAvailablePrio))
        {
          /* Sum up the spin time to total spin time count */
          micros_passed = 
            (Uint32)NdbTick_Elapsed(start_ticks, before).microSec();
          globalData.theMicrosSpin += micros_passed;
          if (timeOutMillis > 0)
          {
            start_ticks = after;
          }
        }
      }
      spinning = true;
//--------------------------------------------------------------------
// In an idle system we will use this loop to wait either for external
// signal received or a message generated by the time queue.
//--------------------------------------------------------------------
    } while (LEVEL_IDLE == globalData.highestAvailablePrio);
//--------------------------------------------------------------------
// Get current microsecond to ensure we will continue executing
// signals for at least a configured time while there are more
// signals to receive.
//--------------------------------------------------------------------
    start_ticks = NdbTick_getCurrentTicks();
    globalScheduler.setHighResTimer(start_ticks);
    if ((NdbTick_Elapsed(yield_ticks, start_ticks).microSec() > 10000))
      yield_flag= TRUE;
    exec_again= 0;
    Uint32 loopStartCount = 0;
    do
    {
//--------------------------------------------------------------------
// This is where the actual execution of signals occur. We execute
// until all buffers are empty or until we have executed 2048 signals.
//--------------------------------------------------------------------
      loopStartCount = globalScheduler.doJob(loopStartCount);
      if (unlikely(globalData.theRestartFlag == perform_stop))
        goto out;
//--------------------------------------------------------------------
// Get timer after executing this set of jobs. If we have passed the
// maximum execution time we will break out of the loop always
// otherwise we will check for new received signals before executing
// the send of the buffers.
// By setting exec_loop_constant to 0 we go back to the traditional
// algorithm of sending once per receive instance.
//--------------------------------------------------------------------
      if (!execute_loop_constant && !min_spin_time)
        break;

      end_ticks = NdbTick_getCurrentTicks();
      globalScheduler.setHighResTimer(end_ticks);
      micros_passed = (Uint32)NdbTick_Elapsed(start_ticks, end_ticks).microSec();
      tot_exec_time += micros_passed;
      if (no_exec_loops++ >= 8192)
      {
        Uint32 expired_time = 
          (Uint32)NdbTick_Elapsed(statistics_start_ticks, end_ticks).microSec();
        statistics_start_ticks = end_ticks;
        globalScheduler.reportThreadConfigLoop(expired_time,
                                               execute_loop_constant,
                                               &no_exec_loops,
                                               &tot_exec_time,
                                               &no_extra_loops,
                                               &tot_extra_time);
      }
      /*
        Continue our execution if micros_passed since last round is smaller
        than the configured constant. Given that we don't recall the
        actual start time of this loop we insert an extra check to ensure we
        don't enter an eternal loop here. We'll never execute more than
        3 times before sending.
      */
      if (micros_passed >= execute_loop_constant || (exec_again > 1))
        break;
      exec_again++;
//--------------------------------------------------------------------
// There were still time for execution left, we check if there are
// signals newly received on the transporters and if so we execute one
// more round before sending the buffered signals.
//--------------------------------------------------------------------
      globalData.incrementWatchDogCounter(7);
      if (!globalTransporterRegistry.pollReceive(0))
        break;

      no_extra_loops++;
      tot_extra_time += micros_passed;
      start_ticks = end_ticks;
      globalData.incrementWatchDogCounter(8);
      globalTransporterRegistry.performReceive();
    } while (1);
  }
out:
  globalData.incrementWatchDogCounter(6);
  globalTransporterRegistry.performSend();

  globalEmulatorData.theWatchDog->unregisterWatchedThread(0);
  globalEmulatorData.theConfiguration->removeThread(pThis);
}//ThreadConfig::ipControlLoop()

int
ThreadConfig::doStart(NodeState::StartLevel startLevel){
  
  SignalHeader sh;
  memset(&sh, 0, sizeof(SignalHeader));
  
  sh.theVerId_signalNumber   = GSN_START_ORD;
  sh.theReceiversBlockNumber = CMVMI;
  sh.theSendersBlockRef      = 0;
  sh.theTrace                = 0;
  sh.theSignalId             = 0;
  sh.theLength               = StartOrd::SignalLength;
  
  union {
    Uint32 theData[25];
    StartOrd startOrd;
  };
  startOrd.restartInfo = 0;
  
  Uint32 secPtrI[3];
  globalScheduler.execute(&sh, JBA, theData, secPtrI);
  return 0;
}

