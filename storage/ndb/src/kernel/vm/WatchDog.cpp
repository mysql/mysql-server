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


#include <ndb_global.h>
#include <my_pthread.h>

#include "WatchDog.hpp"
#include "GlobalData.hpp"
#include <NdbOut.hpp>
#include <NdbSleep.h>
#include <ErrorHandlingMacros.hpp>
   
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

void 
WatchDog::run(){
  unsigned int anIPValue;
  unsigned int alerts = 0;
  unsigned int oldIPValue = 0;
  
  // WatchDog for the single threaded NDB
  while(!theStop){
    Uint32 tmp  = theInterval / 500;
    tmp= (tmp ? tmp : 1);
    
    while(!theStop && tmp > 0){
      NdbSleep_MilliSleep(500);
      tmp--;
    }
    
    if(theStop)
      break;

    // Verify that the IP thread is not stuck in a loop
    anIPValue = *theIPValue;
    if(anIPValue != 0) {
      oldIPValue = anIPValue;
      globalData.incrementWatchDogCounter(0);
      alerts = 0;
    } else {
      const char *last_stuck_action;
      alerts++;
      switch (oldIPValue) {
      case 1:
        last_stuck_action = "Job Handling";
        break;
      case 2:
        last_stuck_action = "Scanning Timers";
        break;
      case 3:
        last_stuck_action = "External I/O";
        break;
      case 4:
        last_stuck_action = "Print Job Buffers at crash";
        break;
      case 5:
        last_stuck_action = "Checking connections";
        break;
      case 6:
        last_stuck_action = "Performing Send";
        break;
      case 7:
        last_stuck_action = "Polling for Receive";
        break;
      case 8:
        last_stuck_action = "Performing Receive";
        break;
      default:
        last_stuck_action = "Unknown place";
        break;
      }//switch
      ndbout << "Ndb kernel is stuck in: " << last_stuck_action << endl;
      if(alerts == 3){
	shutdownSystem(last_stuck_action);
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
