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

extern class  JobTable            globalJobTable;
extern class  TimeQueue           globalTimeQueue;
extern class  FastScheduler       globalScheduler;
extern class  TransporterRegistry globalTransporterRegistry;
extern struct GlobalData          globalData;

#ifdef VM_TRACE
extern class SignalLoggerManager globalSignalLoggers;
#endif

/* EMULATED_JAM_SIZE must be a power of two, so JAM_MASK will work. */
#define EMULATED_JAM_SIZE 1024
#define JAM_MASK (EMULATED_JAM_SIZE - 1)

struct EmulatedJamBuffer
{
  Uint32 theEmulatedJamIndex;
  // last block entry, used in dumpJam() if jam contains no block entries
  Uint32 theEmulatedJamBlockNumber;
  Uint32 theEmulatedJam[EMULATED_JAM_SIZE];
};

struct EmulatorData {
  class Configuration * theConfiguration;
  class WatchDog      * theWatchDog;
  class ThreadConfig  * theThreadConfig;
  class SimBlockList  * theSimBlockList;
  class SocketServer  * m_socket_server;
  class Ndbd_mem_manager * m_mem_manager;

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

/**
 * Get number of extra send buffer pages to use
 */
Uint32 mt_get_extra_send_buffer_pages(Uint32 curr_num_pages,
                                      Uint32 extra_mem_pages);

/**
 * Compute no of pages to be used as job-buffer
 */
Uint32 compute_jb_pages(struct EmulatorData* ed);

#endif 
