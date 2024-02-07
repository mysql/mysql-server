/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef GLOBAL_DATA_H
#define GLOBAL_DATA_H

#include <kernel_types.h>
#include <ndb_global.h>
#include <cstring>
#include "Prio.hpp"
#include "VMSignal.hpp"

#include <BlockNumbers.h>
#include <NdbMutex.h>
#include <NdbTick.h>
#include <ndb_openssl_evp.h>
#include <NodeInfo.hpp>
#include <NodeState.hpp>
#include "ArrayPool.hpp"

// #define GCP_TIMER_HACK

#define JAM_FILE_ID 277

class SimulatedBlock;

enum restartStates {
  initial_state,
  perform_start,
  system_started,
  perform_stop
};

typedef ArrayPool<GlobalPage> GlobalPage_pool;
typedef SafeArrayPool<GlobalPage> GlobalPage_safepool;

struct GlobalData {
  Uint32 m_hb_count[MAX_NODES];    // hb counters
  NodeInfo m_nodeInfo[MAX_NODES];  // At top to ensure cache alignment
  Signal VMSignals[1];             // Owned by FastScheduler::
  NodeVersionInfo m_versionInfo;
  Uint32 m_restart_seq;  //

  NDB_TICKS internalTicksCounter;  // Owned by ThreadConfig::
  Uint32 highestAvailablePrio;     // Owned by FastScheduler::
  Uint32 JobCounter;               // Owned by FastScheduler
  Uint64 JobLap;                   // Owned by FastScheduler
  Uint32 loopMax;                  // Owned by FastScheduler

  Uint32 theNextTimerJob;  // Owned by TimeQueue::
  Uint32 theCurrentTimer;  // Owned by TimeQueue::
  Uint32 theZeroTQIndex;   // Owned by TimeQueue::
  Uint32 theShortTQIndex;  // Owned by TimeQueue::

  Uint32 theLongTQIndex;       // Owned by TimeQueue::
  Uint32 theCountTimer;        // Owned by TimeQueue::
  Uint32 theFirstFreeTQIndex;  // Owned by TimeQueue::
  Uint32 testOn;               // Owned by the Signal Loggers

  NodeId ownId;  // Own processor id

  Uint32 theStartLevel;
  restartStates theRestartFlag;
  Uint32 theSignalId;

  Uint32 sendPackedActivated;
  Uint32 activateSendPacked;

  bool isNdbMt;     // ndbd multithreaded, no workers
  bool isNdbMtLqh;  // ndbd multithreaded, LQH workers
  Uint32 ndbMtLqhWorkers;
  Uint32 ndbMtLqhThreads;
  Uint32 ndbMtTcWorkers;
  Uint32 ndbMtTcThreads;
  Uint32 ndbMtQueryThreads;
  Uint32 ndbMtRecoverThreads;
  Uint32 ndbMtSendThreads;
  Uint32 ndbMtReceiveThreads;
  Uint32 ndbMtMainThreads;
  Uint32 ndbLogParts;
  Uint32 ndbRRGroups;
  Uint32 num_io_laggers;  // Protected by theIO_lag_mutex
  Uint32 QueryThreadsPerLdm;

  Uint64 theMicrosSleep;
  Uint64 theBufferFullMicrosSleep;
  Uint64 theMicrosSend;
  Uint64 theMicrosSpin;

  NdbMutex *theIO_lag_mutex;
  ndb_openssl_evp::byte nodeMasterKey[MAX_NODE_MASTER_KEY_LENGTH];
  Uint32 nodeMasterKeyLength;
  unsigned char filesystemPassword[MAX_BACKUP_ENCRYPTION_PASSWORD_LENGTH];
  Uint32 filesystemPasswordLength;

  GlobalData() {
    theSignalId = 0;
    theStartLevel = NodeState::SL_NOTHING;
    theRestartFlag = perform_start;
    isNdbMt = false;
    isNdbMtLqh = false;
    ndbMtLqhWorkers = 0;
    ndbMtLqhThreads = 0;
    ndbMtTcWorkers = 0;
    ndbMtTcThreads = 0;
    ndbMtQueryThreads = 0;
    ndbMtRecoverThreads = 0;
    ndbMtSendThreads = 0;
    ndbMtReceiveThreads = 0;
    ndbMtMainThreads = 0;
    ndbLogParts = 0;
    ndbRRGroups = 1;
    num_io_laggers = 0;
    QueryThreadsPerLdm = 0;
    theMicrosSleep = 0;
    theBufferFullMicrosSleep = 0;
    theMicrosSend = 0;
    theMicrosSpin = 0;
    std::memset(m_hb_count, 0, sizeof(m_hb_count));
#ifdef GCP_TIMER_HACK
    gcp_timer_limit = 0;
#endif
    theIO_lag_mutex = NdbMutex_Create();
  }

  ~GlobalData() {
    m_global_page_pool.clear();
    m_shared_page_pool.clear();
    NdbMutex_Destroy(theIO_lag_mutex);
  }

  void setBlock(BlockNumber blockNo, SimulatedBlock *block);
  SimulatedBlock *getBlock(BlockNumber blockNo);
  SimulatedBlock *getBlock(BlockNumber blockNo, Uint32 instanceNo);
  SimulatedBlock *getBlockInstance(BlockNumber fullBlockNo) {
    return getBlock(blockToMain(fullBlockNo), blockToInstance(fullBlockNo));
  }
  SimulatedBlock *mt_getBlock(BlockNumber blockNo, Uint32 instanceNo);

  void incrementWatchDogCounter(Uint32 place);
  Uint32 *getWatchDogPtr();

  Uint32 getBlockThreads() const {
    return ndbMtLqhThreads + ndbMtTcThreads + ndbMtReceiveThreads;
  }

  Uint32 get_hb_count(Uint32 nodeId) const { return m_hb_count[nodeId]; }

  Uint32 &set_hb_count(Uint32 nodeId) { return m_hb_count[nodeId]; }

  void lock_IO_lag() { NdbMutex_Lock(theIO_lag_mutex); }
  void unlock_IO_lag() { NdbMutex_Unlock(theIO_lag_mutex); }
  Uint32 get_io_laggers() { return num_io_laggers; }
  void set_io_laggers(Uint32 new_val) { num_io_laggers = new_val; }

 private:
  Uint32 watchDog;
  SimulatedBlock *blockTable[NO_OF_BLOCKS];  // Owned by Dispatcher::
 public:
  GlobalPage_safepool m_global_page_pool;
  GlobalPage_pool m_shared_page_pool;

#ifdef GCP_TIMER_HACK
  // timings are local to the node

  // from prepare to commit (DIH, TC)
  NDB_TICKS gcp_timer_commit[2];
  // from GCP_SAVEREQ to GCP_SAVECONF (LQH)
  NDB_TICKS gcp_timer_save[2];
  // sysfile update (DIH)
  NDB_TICKS gcp_timer_copygci[2];

  // report threshold in ms, if 0 guessed, set with dump 7901 <limit>
  Uint32 gcp_timer_limit;
#endif
};

extern GlobalData globalData;

#define GLOBAL_TEST_ON (localTestOn)
#define GET_GLOBAL_TEST_FLAG bool localTestOn = globalData.testOn
#define SET_GLOBAL_TEST_ON (globalData.testOn = true)
#define SET_GLOBAL_TEST_OFF (globalData.testOn = false)
#define TOGGLE_GLOBAL_TEST_FLAG \
  (globalData.testOn = (globalData.testOn == true ? false : true))

inline void GlobalData::setBlock(BlockNumber blockNo, SimulatedBlock *block) {
  blockNo -= MIN_BLOCK_NO;
  assert((blockTable[blockNo] == 0) || (blockTable[blockNo] == block));
  blockTable[blockNo] = block;
}

inline SimulatedBlock *GlobalData::getBlock(BlockNumber blockNo) {
  blockNo -= MIN_BLOCK_NO;
  return blockTable[blockNo];
}

inline void GlobalData::incrementWatchDogCounter(Uint32 place) {
  watchDog = place;
}

inline Uint32 *GlobalData::getWatchDogPtr() { return &watchDog; }

#undef JAM_FILE_ID

#endif
