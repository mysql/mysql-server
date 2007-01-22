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

class SimulatedBlock;

enum  restartStates {initial_state, 
                     perform_start, 
                     system_started, 
                     perform_stop};

struct GlobalData {
  Uint32     m_restart_seq;           // 
  NodeVersionInfo m_versionInfo;
  NodeInfo   m_nodeInfo[MAX_NODES];
  Signal     VMSignals[1];            // Owned by FastScheduler::
  
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
  
  GlobalData(){ 
    theSignalId = 0; 
    theStartLevel = NodeState::SL_NOTHING;
    theRestartFlag = perform_start;
  }
  ~GlobalData(){}
  
  void             setBlock(BlockNumber blockNo, SimulatedBlock * block);
  SimulatedBlock * getBlock(BlockNumber blockNo);
  
  void           incrementWatchDogCounter(Uint32 place);
  const Uint32 * getWatchDogPtr();
  
private:
  Uint32     watchDog;
  SimulatedBlock* blockTable[NO_OF_BLOCKS]; // Owned by Dispatcher::
public:
  ArrayPool<GlobalPage> m_global_page_pool;
  ArrayPool<GlobalPage> m_shared_page_pool;
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
const Uint32 *
GlobalData::getWatchDogPtr(){
  return &watchDog;
}

#endif
