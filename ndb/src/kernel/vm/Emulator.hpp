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

#ifndef EMULATOR_H
#define EMULATOR_H

//===========================================================================
//
// .DESCRIPTION
//      This is the main fuction for the AXE VM emulator.
//      It contains some global objects and a run method.
//
//===========================================================================
#include <kernel_types.h>
#include <TransporterRegistry.hpp>

extern class  JobTable            globalJobTable;
extern class  TimeQueue           globalTimeQueue;
extern class  FastScheduler       globalScheduler;
extern class  TransporterRegistry globalTransporterRegistry;
extern struct GlobalData          globalData;

#ifdef VM_TRACE
extern class SignalLoggerManager globalSignalLoggers;
#endif

#ifndef NO_EMULATED_JAM
  #define EMULATED_JAM_SIZE 1024
  #define JAM_MASK ((EMULATED_JAM_SIZE * 4) - 1)

  extern Uint8 theEmulatedJam[];
  extern Uint32 theEmulatedJamIndex;
  // last block entry, used in dumpJam() if jam contains no block entries
  extern Uint32 theEmulatedJamBlockNumber;
#else
  const Uint8 theEmulatedJam[]=0;
  const Uint32 theEmulatedJamIndex=0;
#endif

struct EmulatorData {
  class Configuration * theConfiguration;
  class WatchDog      * theWatchDog;
  class ThreadConfig  * theThreadConfig;
  class SimBlockList  * theSimBlockList;
  
  /**
   * Constructor
   *
   *  Sets all the pointers to NULL
   */
  EmulatorData();
  
  /**
   * Create all the objects
   */
  void create();
  
  /**
   * Destroys all the objects
   */
  void destroy();
};

extern struct EmulatorData globalEmulatorData;

enum NdbShutdownType {
  NST_Normal,
  NST_Watchdog,
  NST_ErrorHandler,
  NST_ErrorHandlerSignal,
  NST_Restart,
  NST_ErrorInsert
};

enum NdbRestartType {
  NRT_Default               = 0,
  NRT_NoStart_Restart       = 1, // -n
  NRT_DoStart_Restart       = 2, //
  NRT_NoStart_InitialStart  = 3, // -n -i
  NRT_DoStart_InitialStart  = 4  // -i
};

/**
 * Shutdown/restart Ndb
 *
 * @param type        - Type of shutdown/restart
 * @param restartType - Type of restart (only valid if type == NST_Restart)
 */
void 
NdbShutdown(NdbShutdownType type, 
	    NdbRestartType restartType = NRT_Default);

#endif 
