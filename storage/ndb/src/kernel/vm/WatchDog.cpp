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
#include <my_pthread.h>
#include <sys/times.h>

#include "WatchDog.hpp"
#include "GlobalData.hpp"
#include <NdbOut.hpp>
#include <NdbSleep.h>
#include <ErrorHandlingMacros.hpp>
#include <EventLogger.hpp>

#include <NdbTick.h>

extern EventLogger g_eventLogger;

extern "C" 
void* 
runWatchDog(void* w){
  ((WatchDog*)w)->run();
  return NULL;
}

WatchDog::WatchDog(Uint32 interval) : 
  theIPValue(globalData.getWatchDogPtr())
{
  setCheckInterval(interval);
  theStop = false;
  theThreadPtr = 0;
}

WatchDog::~WatchDog(){
  doStop();
}

Uint32
WatchDog::setCheckInterval(Uint32 interval){
  // An interval of less than 70ms is not acceptable
  return theInterval = (interval < 70 ? 70 : interval);
}

void
WatchDog::doStart(){
  theStop = false;
  theThreadPtr = NdbThread_Create(runWatchDog, 
				  (void**)this, 
				  32768,
				  "ndb_watchdog",
                                  NDB_THREAD_PRIO_HIGH);
}

void
WatchDog::doStop(){
  void *status;
  theStop = true;
  if(theThreadPtr){
    NdbThread_WaitFor(theThreadPtr, &status);
    NdbThread_Destroy(&theThreadPtr);
  }
}

const char *get_action(Uint32 IPValue)
{
  const char *action;
  switch (IPValue) {
  case 1:
    action = "Job Handling";
    break;
  case 2:
    action = "Scanning Timers";
    break;
  case 3:
    action = "External I/O";
    break;
  case 4:
    action = "Print Job Buffers at crash";
    break;
  case 5:
    action = "Checking connections";
    break;
  case 6:
    action = "Performing Send";
    break;
  case 7:
    action = "Polling for Receive";
    break;
  case 8:
    action = "Performing Receive";
    break;
  case 9:
    action = "Allocating memory";
    break;
  default:
    action = "Unknown place";
    break;
  }//switch
  return action;
}

void 
WatchDog::run()
{
  unsigned int anIPValue, sleep_time;
  unsigned int oldIPValue = 0;
  unsigned int theIntervalCheck = theInterval;
  struct MicroSecondTimer start_time, last_time, now;
  NdbTick_getMicroTimer(&start_time);
  last_time = start_time;

  // WatchDog for the single threaded NDB
  while (!theStop)
  {
    sleep_time= 100;

    NdbSleep_MilliSleep(sleep_time);
    if(theStop)
      break;

    NdbTick_getMicroTimer(&now);
    if (NdbTick_getMicrosPassed(last_time, now)/1000 > sleep_time*2)
    {
      struct tms my_tms;
      times(&my_tms);
      g_eventLogger.info("Watchdog: User time: %llu  System time: %llu",
                         (Uint64)my_tms.tms_utime,
                         (Uint64)my_tms.tms_stime);
      g_eventLogger.warning("Watchdog: Warning overslept %u ms, expected %u ms.",
                            NdbTick_getMicrosPassed(last_time, now)/1000,
                            sleep_time);
    }
    last_time = now;

    // Verify that the IP thread is not stuck in a loop
    anIPValue = *theIPValue;
    if (anIPValue != 0)
    {
      oldIPValue = anIPValue;
      globalData.incrementWatchDogCounter(0);
      NdbTick_getMicroTimer(&start_time);
      theIntervalCheck = theInterval;
    }
    else
    {
      int warn = 1;
      Uint32 elapsed = NdbTick_getMicrosPassed(start_time, now)/1000;
      /*
        oldIPValue == 9 indicates malloc going on, this can take some time
        so only warn if we pass the watchdog interval
      */
      if (oldIPValue == 9)
        if (elapsed < theIntervalCheck)
          warn = 0;
        else
          theIntervalCheck += theInterval;

      if (warn)
      {
        const char *last_stuck_action = get_action(oldIPValue);
        g_eventLogger.warning("Ndb kernel is stuck in: %s", last_stuck_action);
        {
          struct tms my_tms;
          times(&my_tms);
          g_eventLogger.info("Watchdog: User time: %llu  System time: %llu",
                             (Uint64)my_tms.tms_utime,
                             (Uint64)my_tms.tms_stime);
        }
        if (elapsed > 3 * theInterval)
        {
          shutdownSystem(last_stuck_action);
        }
      }
    }
  }
  return;
}

void
WatchDog::shutdownSystem(const char *last_stuck_action){
  
  ErrorReporter::handleError(NDBD_EXIT_WATCHDOG_TERMINATE,
			     last_stuck_action,
			     __FILE__,
			     NST_Watchdog);
}
