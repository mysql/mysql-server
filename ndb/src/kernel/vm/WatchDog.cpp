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
  my_thread_init();
  ((WatchDog*)w)->run();
  my_thread_end();
  NdbThread_Exit(0);
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
      alerts++;
      ndbout << "Ndb kernel is stuck in: ";
      switch (oldIPValue) {
      case 1:
        ndbout << "Job Handling" << endl;
        break;
      case 2:
        ndbout << "Scanning Timers" << endl;
        break;
      case 3:
        ndbout << "External I/O" << endl;
        break;
      case 4:
        ndbout << "Print Job Buffers at crash" << endl;
        break;
      case 5:
        ndbout << "Checking connections" << endl;
        break;
      case 6:
        ndbout << "Performing Send" << endl;
        break;
      case 7:
        ndbout << "Polling for Receive" << endl;
        break;
      case 8:
        ndbout << "Performing Receive" << endl;
        break;
      default:
        ndbout << "Unknown place" << endl;
        break;
      }//switch
      if(alerts == 3){
	shutdownSystem();
      }
    }
  }
  return;
}

void
WatchDog::shutdownSystem(){
  
  ErrorReporter::handleError(ecError,
			     ERR_PROGRAMERROR,
			     "WatchDog terminate",
			     __FILE__,
			     NST_Watchdog);
}
