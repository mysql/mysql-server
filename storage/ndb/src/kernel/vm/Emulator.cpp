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

#include <ndb_global.h>

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
#include "ndbd_malloc_impl.hpp"

#include <NdbMem.h>
#include <NdbMutex.h>

#include <EventLogger.hpp>
extern EventLogger * g_eventLogger;

/**
 * Declare the global variables 
 */

#ifndef NO_EMULATED_JAM
/*
  This is the jam buffer used for non-threaded ndbd (but present also
  in threaded ndbd to allow sharing of object files among the two
  binaries).
 */
EmulatedJamBuffer theEmulatedJamBuffer;
#endif

   GlobalData globalData;

   TimeQueue globalTimeQueue;
   FastScheduler globalScheduler;
   extern TransporterRegistry globalTransporterRegistry;

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
  m_socket_server = 0;
  m_mem_manager = 0;
}

void
EmulatorData::create(){
  /*
    Global jam() buffer, for non-multithreaded operation.
    For multithreaded ndbd, each thread will set a local jam buffer later.
  */
#ifndef NO_EMULATED_JAM
  void * jamBuffer = (void *)&theEmulatedJamBuffer;
#else
  void * jamBuffer = 0;
#endif
  NdbThread_SetTlsKey(NDB_THREAD_TLS_JAM, jamBuffer);

  NdbMem_Create();

  theConfiguration = new Configuration();
  theWatchDog      = new WatchDog();
  theThreadConfig  = new ThreadConfig();
  theSimBlockList  = new SimBlockList();
  m_socket_server  = new SocketServer();
  m_mem_manager    = new Ndbd_mem_manager();
  globalData.m_global_page_pool.setMutex();

  if (theConfiguration == NULL ||
      theWatchDog == NULL ||
      theThreadConfig == NULL ||
      theSimBlockList == NULL ||
      m_socket_server == NULL ||
      m_mem_manager == NULL )
  {
    ERROR_SET(fatal, NDBD_EXIT_MEMALLOC,
              "Failed to create EmulatorData", "");
  }

  if (!(theShutdownMutex = NdbMutex_Create()))
  {
    ERROR_SET(fatal, NDBD_EXIT_MEMALLOC,
              "Failed to create shutdown mutex", "");
  }
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
  if(m_socket_server)
    delete m_socket_server; m_socket_server = 0;
  NdbMutex_Destroy(theShutdownMutex);
  if (m_mem_manager)
    delete m_mem_manager; m_mem_manager = 0;
  
  NdbMem_Destroy();
}
