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

#include <signaldata/StartOrd.hpp>

ThreadConfig::ThreadConfig()
{
}

ThreadConfig::~ThreadConfig()
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
#ifdef VM_TRACE
    ndbout << "Time moved backwards with ";
    ndbout << (globalData.internalMillisecCounter - currMilliSecond);
    ndbout << " milliseconds" << endl;
#endif
    globalData.internalMillisecCounter = currMilliSecond;
  }//if
  if (currMilliSecond > (globalData.internalMillisecCounter + 1500)) {
//--------------------------------------------------------------------
// Time has moved forward more than a second. Either it could happen
// if operator changed the time or if the OS has misbehaved badly.
// We set the new time to one second from the past.
//--------------------------------------------------------------------
#ifdef VM_TRACE
    ndbout << "Time moved forward with ";
    ndbout << (currMilliSecond - globalData.internalMillisecCounter);
    ndbout << " milliseconds" << endl;
#endif
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
void ThreadConfig::ipControlLoop()
{

//--------------------------------------------------------------------
// initialise the counter that keeps track of the current millisecond
//--------------------------------------------------------------------
  globalData.internalMillisecCounter = NdbTick_CurrentMillisecond();
  Uint32 i = 0;
  while (globalData.theRestartFlag != perform_stop)  { 

    Uint32 timeOutMillis = 0;
    if (LEVEL_IDLE == globalData.highestAvailablePrio) {
//--------------------------------------------------------------------
// The buffers are empty, we need to wait for a while until we continue.
// We cannot wait forever since we can also have timed events.
//--------------------------------------------------------------------
//--------------------------------------------------------------------
// Set the time we will sleep on the sockets before waking up
// unconditionally to 10 ms. Will never sleep more than 10 milliseconds
// on a socket.
//--------------------------------------------------------------------
      timeOutMillis = 10;
    }//if
//--------------------------------------------------------------------
// Now it is time to check all interfaces. We will send all buffers
// plus checking for any received messages.
//--------------------------------------------------------------------
    if (i++ >= 20) {
      globalTransporterRegistry.update_connections();
      globalData.incrementWatchDogCounter(5);
      i = 0;
    }//if

    globalData.incrementWatchDogCounter(6);
    globalTransporterRegistry.performSend();
    
    globalData.incrementWatchDogCounter(7);
    if (globalTransporterRegistry.pollReceive(timeOutMillis)) {
      globalData.incrementWatchDogCounter(8);
      globalTransporterRegistry.performReceive();
    }

//--------------------------------------------------------------------
// We scan the time queue to see if there are any timed signals that
// is now ready to be executed.
//--------------------------------------------------------------------
    globalData.incrementWatchDogCounter(2);
    scanTimeQueue(); 

//--------------------------------------------------------------------
// This is where the actual execution of signals occur. We execute
// until all buffers are empty or until we have executed 2048 signals.
//--------------------------------------------------------------------
    globalScheduler.doJob();
  }//while

  globalData.incrementWatchDogCounter(6);
  globalTransporterRegistry.performSend();

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
  
  Uint32 theData[25];
  StartOrd * const  startOrd = (StartOrd *)&theData[0];
  startOrd->restartInfo = 0;
  
  Uint32 secPtrI[3];
  globalScheduler.execute(&sh, JBA, theData, secPtrI);
  return 0;
}

