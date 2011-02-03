/*
   Copyright (c) 2003, 2010, Oracle and/or its affiliates. All rights reserved.

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

/**
 * For each millisecond that has passed since this function was last called:
 *   Scan the job buffer and increment the internalMillisecCounter 
 *      with 1 to keep track of where we are
 */
inline
void 
ThreadConfig::scanTimeQueue()
{
  unsigned int maxCounter;
  Uint64 currMilliSecond;
  maxCounter = 0;
  currMilliSecond = NdbTick_CurrentMillisecond();
  if (currMilliSecond < globalData.internalMillisecCounter) {
//--------------------------------------------------------------------
// This could occur around 2036 or if the operator decides to change
// time backwards. We cannot know how long time has past since last
// time and we make a best try with 0 milliseconds.
//--------------------------------------------------------------------
    g_eventLogger->warning("Time moved backwards with %llu ms",
                           globalData.internalMillisecCounter-currMilliSecond);
    globalData.internalMillisecCounter = currMilliSecond;
  }//if
  if (currMilliSecond > (globalData.internalMillisecCounter + 1500)) {
//--------------------------------------------------------------------
// Time has moved forward more than a second. Either it could happen
// if operator changed the time or if the OS has misbehaved badly.
// We set the new time to one second from the past.
//--------------------------------------------------------------------
    g_eventLogger->warning("Time moved forward with %llu ms",
                           currMilliSecond-globalData.internalMillisecCounter);
    globalData.internalMillisecCounter = currMilliSecond - 1000;
  }//if
  while (((currMilliSecond - globalData.internalMillisecCounter) > 0) &&
         (maxCounter < 20)){
    globalData.internalMillisecCounter++;
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
void ThreadConfig::ipControlLoop(NdbThread*, Uint32 thread_index)
{
  globalEmulatorData.theConfiguration->setAllLockCPU(true);

  Uint32 execute_loop_constant =
        globalEmulatorData.theConfiguration->schedulerExecutionTimer();
  Uint32 min_spin_time = 
    globalEmulatorData.theConfiguration->schedulerSpinTimer();
  struct MicroSecondTimer start_micro, end_micro, statistics_start_micro;
  struct MicroSecondTimer yield_micro;
  Uint32 no_exec_loops = 0;
  Uint32 no_extra_loops = 0;
  Uint32 tot_exec_time = 0;
  Uint32 tot_extra_time = 0;
  Uint32 timeOutMillis;
  Uint32 micros_passed;
  bool spinning;
  bool yield_flag= FALSE;
  int res1 = 0;
  int res2 = 0;
  int res3 = 0;
  Uint32 i = 0;
  Uint32 exec_again;

//--------------------------------------------------------------------
// initialise the counter that keeps track of the current millisecond
//--------------------------------------------------------------------
  globalData.internalMillisecCounter = NdbTick_CurrentMillisecond();

  Uint32 *watchCounter = globalData.getWatchDogPtr();
  globalEmulatorData.theWatchDog->registerWatchedThread(watchCounter, 0);

  res1 = NdbTick_getMicroTimer(&start_micro);
  yield_micro = statistics_start_micro = end_micro = start_micro;
  while (1)
  {
    timeOutMillis = 0;
//--------------------------------------------------------------------
// We send all messages buffered during execution of job buffers
//--------------------------------------------------------------------
    globalData.incrementWatchDogCounter(6);
    globalTransporterRegistry.performSend();

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
            res2 = NdbTick_getMicroTimer(&end_micro);
          if (!(res1 + res2))
          {
            micros_passed = 
              (Uint32)NdbTick_getMicrosPassed(start_micro, end_micro);
            if (micros_passed < min_spin_time)
              timeOutMillis = 0;
          }
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
        if (yield_flag)
        {
          globalEmulatorData.theConfiguration->yield_main(thread_index, TRUE);
          poll_flag= globalTransporterRegistry.pollReceive(timeOutMillis);
          globalEmulatorData.theConfiguration->yield_main(thread_index, FALSE);
          res3= NdbTick_getMicroTimer(&yield_micro);
        }
        else
          poll_flag= globalTransporterRegistry.pollReceive(timeOutMillis);
        if (poll_flag)
        {
          globalData.incrementWatchDogCounter(8);
          globalTransporterRegistry.performReceive();
        }
        yield_flag= FALSE;
      }
      spinning = true;
      globalScheduler.postPoll();
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
    res1= NdbTick_getMicroTimer(&start_micro);
    if ((res1 + res3) || 
        ((Uint32)NdbTick_getMicrosPassed(start_micro, yield_micro) > 10000))
      yield_flag= TRUE;
    exec_again= 0;
    do
    {
//--------------------------------------------------------------------
// This is where the actual execution of signals occur. We execute
// until all buffers are empty or until we have executed 2048 signals.
//--------------------------------------------------------------------
      globalScheduler.doJob();
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
      res2= NdbTick_getMicroTimer(&end_micro);
      if (res2)
        break;
      micros_passed = (Uint32)NdbTick_getMicrosPassed(start_micro, end_micro);
      tot_exec_time += micros_passed;
      if (no_exec_loops++ >= 8192)
      {
        Uint32 expired_time = 
          (Uint32)NdbTick_getMicrosPassed(statistics_start_micro, end_micro);
        statistics_start_micro = end_micro;
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
      if (micros_passed > execute_loop_constant || (exec_again > 1))
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
      start_micro = end_micro;
      globalData.incrementWatchDogCounter(8);
      globalTransporterRegistry.performReceive();
    } while (1);
  }
out:
  globalData.incrementWatchDogCounter(6);
  globalTransporterRegistry.performSend();

  globalEmulatorData.theWatchDog->unregisterWatchedThread(0);

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

