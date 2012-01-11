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

#ifndef GLOBAL_DATA_H
#define GLOBAL_DATA_H

#include <ndb_global.h>
#include <kernel_types.h>
#include "Prio.hpp"
#include "VMSignal.hpp"

#include <BlockNumbers.h>
#include <NodeState.hpp>
#include <NodeInfo.hpp>
#include "ArrayPool.hpp"

// #define GCP_TIMER_HACK
#ifdef GCP_TIMER_HACK
#include <NdbTick.h>
#endif

class SimulatedBlock;

enum  restartStates {initial_state, 
                     perform_start, 
                     system_started, 
                     perform_stop};

struct GlobalData {
  Uint32     m_hb_count[MAX_NODES];   // hb counters
  NodeInfo   m_nodeInfo[MAX_NODES];   // At top to ensure cache alignment
  Signal     VMSignals[1];            // Owned by FastScheduler::
  Uint32     m_restart_seq;           //
  NodeVersionInfo m_versionInfo;
  
  Uint64     internalMillisecCounter; // Owned by ThreadConfig::
  Uint32     highestAvailablePrio;    // Owned by FastScheduler::
  Uint32     JobCounter;              // Owned by FastScheduler
  Uint64     JobLap;                  // Owned by FastScheduler
  Uint32     loopMax;                 // Owned by FastScheduler
  
  Uint32     theNextTimerJob;         // Owned by TimeQueue::
  Uint32     theCurrentTimer;         // Owned by TimeQueue::
  Uint32     theShortTQIndex;         // Owned by TimeQueue::
  
  Uint32     theLongTQIndex;          // Owned by TimeQueue::
  Uint32     theCountTimer;           // Owned by TimeQueue::
  Uint32     theFirstFreeTQIndex;     // Owned by TimeQueue::
  Uint32     testOn;                  // Owned by the Signal Loggers
  
  NodeId     ownId;                   // Own processor id
  
  Uint32     theStartLevel;
  restartStates theRestartFlag;
  Uint32     theSignalId;
  
  Uint32     sendPackedActivated;
  Uint32     activateSendPacked;

  bool       isNdbMt;    // ndbd multithreaded, no workers
  bool       isNdbMtLqh; // ndbd multithreaded, LQH workers
  Uint32     ndbMtLqhWorkers;
  Uint32     ndbMtLqhThreads;
  Uint32     ndbMtTcThreads;
  Uint32     ndbMtSendThreads;
  Uint32     ndbMtReceiveThreads;
  Uint32     ndbLogParts;
  
  GlobalData(){ 
    theSignalId = 0; 
    theStartLevel = NodeState::SL_NOTHING;
    theRestartFlag = perform_start;
    isNdbMt = false;
    isNdbMtLqh = false;
    ndbMtLqhWorkers = 0;
    ndbMtLqhThreads = 0;
    ndbMtTcThreads = 0;
    ndbMtSendThreads = 0;
    ndbMtReceiveThreads = 0;
    ndbLogParts = 0;
    bzero(m_hb_count, sizeof(m_hb_count));
#ifdef GCP_TIMER_HACK
    gcp_timer_limit = 0;
#endif
  }
  ~GlobalData() { m_global_page_pool.clear(); m_shared_page_pool.clear();}
  
  void             setBlock(BlockNumber blockNo, SimulatedBlock * block);
  SimulatedBlock * getBlock(BlockNumber blockNo);
  SimulatedBlock * getBlock(BlockNumber blockNo, Uint32 instanceNo);
  SimulatedBlock * getBlockInstance(BlockNumber fullBlockNo) {
    return getBlock(blockToMain(fullBlockNo), blockToInstance(fullBlockNo));
  }
  SimulatedBlock * mt_getBlock(BlockNumber blockNo, Uint32 instanceNo);
  
  void           incrementWatchDogCounter(Uint32 place);
  Uint32 * getWatchDogPtr();

  Uint32 getBlockThreads() const {
    return ndbMtLqhThreads + ndbMtTcThreads + ndbMtReceiveThreads;
  }

  Uint32 get_hb_count(Uint32 nodeId) const {
    return m_hb_count[nodeId];
  }

  Uint32& set_hb_count(Uint32 nodeId) {
    return m_hb_count[nodeId];
  }
private:
  Uint32     watchDog;
  SimulatedBlock* blockTable[NO_OF_BLOCKS]; // Owned by Dispatcher::
public:
  SafeArrayPool<GlobalPage> m_global_page_pool;
  ArrayPool<GlobalPage> m_shared_page_pool;

#ifdef GCP_TIMER_HACK
  // timings are local to the node

  // from prepare to commit (DIH, TC)
  MicroSecondTimer gcp_timer_commit[2];
  // from GCP_SAVEREQ to GCP_SAVECONF (LQH)
  MicroSecondTimer gcp_timer_save[2];
  // sysfile update (DIH)
  MicroSecondTimer gcp_timer_copygci[2];

  // report threshold in ms, if 0 guessed, set with dump 7901 <limit>
  Uint32 gcp_timer_limit;
#endif
};

extern GlobalData globalData;

#define GLOBAL_TEST_ON (localTestOn)
#define GET_GLOBAL_TEST_FLAG bool localTestOn = globalData.testOn
#define SET_GLOBAL_TEST_ON (globalData.testOn = true)
#define SET_GLOBAL_TEST_OFF (globalData.testOn = false)
#define TOGGLE_GLOBAL_TEST_FLAG (globalData.testOn = (globalData.testOn == true ? false : true))

inline
void
GlobalData::setBlock(BlockNumber blockNo, SimulatedBlock * block){
  blockNo -= MIN_BLOCK_NO;
  assert((blockTable[blockNo] == 0) || (blockTable[blockNo] == block));
  blockTable[blockNo] = block;
}

inline
SimulatedBlock *
GlobalData::getBlock(BlockNumber blockNo){
  blockNo -= MIN_BLOCK_NO;
  return blockTable[blockNo];
}

inline
void
GlobalData::incrementWatchDogCounter(Uint32 place){
  watchDog = place;
}

inline
Uint32 *
GlobalData::getWatchDogPtr(){
  return &watchDog;
}

#endif
