/*
   Copyright (c) 2003, 2010, Oracle and/or its affiliates. All rights reserved.

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
/* EMULATED_JAM_SIZE must be a power of two, so JAM_MASK will work. */
#define EMULATED_JAM_SIZE 1024
#define JAM_MASK (EMULATED_JAM_SIZE - 1)

struct EmulatedJamBuffer {
  Uint32 theEmulatedJamIndex;
  // last block entry, used in dumpJam() if jam contains no block entries
  Uint32 theEmulatedJamBlockNumber;
  Uint32 theEmulatedJam[EMULATED_JAM_SIZE];
};
#endif

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
 * Compute no of pages to be used as job-buffer
 */
Uint32 compute_jb_pages(struct EmulatorData* ed);

#endif 
