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

#include "Emulator.hpp"
#include <FastScheduler.hpp>
#include <SignalLoggerManager.hpp>
#include <TransporterRegistry.hpp>
#include <TimeQueue.hpp>

#include "Configuration.hpp"
#include "WatchDog.hpp"
#include "ThreadConfig.hpp"
#include "SimBlockList.hpp"

#include <NodeState.hpp>

#include <NdbMem.h>
#include <NdbOut.hpp>
#include <NdbMutex.h>
#include <NdbSleep.h>
#include <stdlib.h>
#include <new>

#ifdef NDB_WIN32
#include <new.h>
#include <process.h>
#define execvp _execvp
#define set_new_handler _set_new_handler
#endif

/**
 * Declare the global variables 
 */

#ifdef USE_EMULATED_JAM
   Uint8 theEmulatedJam[EMULATED_JAM_SIZE * 4];
   Uint32 theEmulatedJamIndex = 0;
   Uint32 theEmulatedJamBlockNumber = 0;
#endif // USE_EMULATED_JAM

   GlobalData globalData;

   TimeQueue globalTimeQueue;
   FastScheduler globalScheduler;
   TransporterRegistry globalTransporterRegistry;

#ifdef VM_TRACE
   SignalLoggerManager globalSignalLoggers;
#endif

EmulatorData globalEmulatorData;
NdbMutex * theShutdownMutex = 0;

EmulatorData::EmulatorData(){
  theConfiguration = 0;
  theWatchDog      = 0;
  theThreadConfig  = 0;
  theSimBlockList  = 0;
  theShutdownMutex = 0;
}

void
ndb_new_handler(){
  ERROR_SET(fatal, ERR_MEMALLOC, "New handler", "");
}

void
EmulatorData::create(){
  NdbMem_Create();

  theConfiguration = new Configuration();
  theWatchDog      = new WatchDog();
  theThreadConfig  = new ThreadConfig();
  theSimBlockList  = new SimBlockList();

  theShutdownMutex = NdbMutex_Create();

#ifdef NDB_WIN32
  set_new_handler((_PNH)ndb_new_handler);
#else
  std::set_new_handler(ndb_new_handler);
#endif
}

void
EmulatorData::destroy(){
  if(theConfiguration)
    delete theConfiguration; theConfiguration = 0;
  if(theWatchDog)
    delete theWatchDog; theWatchDog = 0;
  if(theThreadConfig)
    delete theThreadConfig; theThreadConfig = 0;
  if(theSimBlockList)
    delete theSimBlockList; theSimBlockList = 0;
  
  NdbMem_Destroy();
}

void
NdbRestart(char * programName,
	   NdbRestartType type, char * connString){
#if ! ( defined NDB_OSE || defined NDB_SOFTOSE) 
  int argc = 2;
  switch(type){
  case NRT_NoStart_Restart:
  case NRT_DoStart_InitialStart:
    argc = 3;
    break;
  case NRT_NoStart_InitialStart:
    argc = 4;
    break;
  case NRT_DoStart_Restart:
  case NRT_Default:
  default:
    argc = 2;
    break;
  }

  if(connString != 0){
    argc += 2;
  }
  
  char ** argv = new char * [argc];
  argv[0] = programName;
  argv[argc - 1] = 0;

  switch(type){
  case NRT_NoStart_Restart:
    argv[1] = "-n";
    break;
  case NRT_DoStart_InitialStart:
    argv[1] = "-i";
    break;
  case NRT_NoStart_InitialStart:
    argv[1] = "-n";
    argv[2] = "-i";
    break;
  case NRT_DoStart_Restart:
  case NRT_Default:
  default:
    break;
  }

  if(connString != 0){
    argv[argc-3] = "-c";
    argv[argc-2] = connString;
  }
  
  execvp(programName, argv);
#endif
}

void
NdbShutdown(NdbShutdownType type,
	    NdbRestartType restartType){
  
  if(type == NST_ErrorInsert){
    type = NST_Restart;
    restartType = (NdbRestartType)
      globalEmulatorData.theConfiguration->getRestartOnErrorInsert();
    if(restartType == NRT_Default){
      type = NST_ErrorHandler;
      globalEmulatorData.theConfiguration->stopOnError(true);
    }
  }
  
  if(NdbMutex_Trylock(theShutdownMutex) == 0){
    globalData.theRestartFlag = perform_stop;

    bool restart = false;
    char * progName = 0;
    char * connString = 0;
#if ! ( defined NDB_OSE || defined NDB_SOFTOSE) 
    if((type != NST_Normal && 
	globalEmulatorData.theConfiguration->stopOnError() == false) ||
       type == NST_Restart) {
      
      restart  = true;
      progName = strdup(globalEmulatorData.theConfiguration->programName());
      connString = globalEmulatorData.theConfiguration->getConnectStringCopy();
      if(type != NST_Restart){
	/**
	 * If we crash before we started
	 *
	 * Do restart -n
	 */
	if(globalData.theStartLevel == NodeState::SL_STARTED)
	  restartType = NRT_Default;
	else
	  restartType = NRT_NoStart_Restart;
      }
    }
#endif
    
    const char * shutting = "shutting down";
    if(restart){
      shutting = "restarting";
    }
    
    switch(type){
    case NST_Normal:
      ndbout << "Shutdown initiated" << endl;
      break;
    case NST_Watchdog:
      ndbout << "Watchdog " << shutting << " system" << endl;
      break;
    case NST_ErrorHandler:
      ndbout << "Error handler " << shutting << " system" << endl;
      break;
    case NST_Restart:
      ndbout << "Restarting system" << endl;
      break;
    default:
      ndbout << "Error handler " << shutting << " system"
	     << " (unknown type: " << type << ")" << endl;
      type = NST_ErrorHandler;
      break;
    }
    
    const char * exitAbort = 0;
#if defined VM_TRACE && ( ! ( defined NDB_OSE || defined NDB_SOFTOSE) )
    exitAbort = "aborting";
#else
    exitAbort = "exiting";
#endif
    
    if(type == NST_Watchdog){
      if(restart){
	NdbRestart(progName, restartType, connString);
      }
      
      /**
       * Very serious
       */
      ndbout << "Watchdog shutdown completed - " << exitAbort << endl;
#if defined VM_TRACE && ( ! ( defined NDB_OSE || defined NDB_SOFTOSE) )
      abort();
#else
      exit(1);
#endif
    }

    globalEmulatorData.theWatchDog->doStop();
    
#ifdef VM_TRACE
    FILE * outputStream = globalSignalLoggers.setOutputStream(0);
    if(outputStream != 0)
      fclose(outputStream);
#endif
    
    globalTransporterRegistry.stopSending();
    globalTransporterRegistry.stopReceiving();
    
    globalTransporterRegistry.removeAll();

#ifdef VM_TRACE
#define UNLOAD (type != NST_ErrorHandler && type != NST_Watchdog)
#else
#define UNLOAD true
#endif
    if(UNLOAD){
      globalEmulatorData.theSimBlockList->unload();    
      globalEmulatorData.destroy();

    }

    if(type != NST_Normal &&
       type != NST_Restart){
      if(restart){
	NdbRestart(progName, restartType, connString);
      }
      
      ndbout << "Error handler shutdown completed - " << exitAbort << endl;
#if defined VM_TRACE && ( ! ( defined NDB_OSE || defined NDB_SOFTOSE) )
      abort();
#else
      exit(1);
#endif
    }
    
    /**
     * This is a normal restart
     */
    if(type == NST_Restart){
      if(restart){
	NdbRestart(progName, restartType, connString);
      }
      /**
       * What to do if in restart mode, but being unable to do it...
       */
#if defined VM_TRACE && ( ! ( defined NDB_OSE || defined NDB_SOFTOSE) )
      abort();
#else
      exit(1);
#endif
    }

    /**
     * This is normal shutdown
     */
    ndbout << "Shutdown completed - exiting" << endl;
  } else {
    /**
     * Shutdown is already in progress
     */

    /** 
     * If this is the watchdog, kill system the hard way
     */
    if (type== NST_Watchdog){
      ndbout << "Watchdog is killing system the hard way" << endl;
#if defined VM_TRACE && ( ! ( defined NDB_OSE || defined NDB_SOFTOSE) )
      abort();
#else
      exit(1);
#endif
    }
      
    while(true)
      NdbSleep_MilliSleep(10);
  }
}

