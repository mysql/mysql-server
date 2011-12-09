/*
   Copyright (c) 2003, 2011, Oracle and/or its affiliates. All rights reserved.

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

#ifndef ClusterMgr_H
#define ClusterMgr_H

#include <ndb_limits.h>
#include <NdbThread.h>
#include <NdbMutex.h>
#include <NdbCondition.h>
#include <signaldata/ArbitSignalData.hpp>
#include <signaldata/NodeStateSignalData.hpp>
#include "trp_client.hpp"
#include "trp_node.hpp"
#include <signaldata/DisconnectRep.hpp>

extern "C" void* runClusterMgr_C(void * me);


/**
 * @class ClusterMgr
 */
class ClusterMgr : public trp_client
{
  friend class TransporterFacade;
  friend class ArbitMgr;
  friend void* runClusterMgr_C(void * me);
public:
  ClusterMgr(class TransporterFacade &);
  virtual ~ClusterMgr();
  void configure(Uint32 nodeId, const ndb_mgm_configuration* config);
  
  void reportConnected(NodeId nodeId);
  void reportDisconnected(NodeId nodeId);
  
  bool checkUpgradeCompatability(Uint32 nodeVersion);

  void doStop();
  void startThread();

  void forceHB();
  void set_max_api_reg_req_interval(unsigned int millisec) {
    m_max_api_reg_req_interval = millisec;
  }

  void lock() { NdbMutex_Lock(clusterMgrThreadMutex); trp_client::lock(); }
  void unlock() { trp_client::unlock();NdbMutex_Unlock(clusterMgrThreadMutex); }

private:
  void startup();
  void threadMain();
  
  int  theStop;
  class TransporterFacade & theFacade;
  class ArbitMgr * theArbitMgr;

public:
  enum Cluster_state {
    CS_waiting_for_clean_cache = 0,
    CS_waiting_for_first_connect,
    CS_connected
  };

  struct Node : public trp_node
  {
    Node();

    /**
     * Heartbeat stuff
     */
    Uint32 hbFrequency; // Heartbeat frequence 
    Uint32 hbCounter;   // # milliseconds passed since last hb sent
    Uint32 hbMissed;    // # missed heartbeats
  };
  
  const trp_node & getNodeInfo(NodeId) const;
  Uint32        getNoOfConnectedNodes() const;
  void          hb_received(NodeId);

  int m_auto_reconnect;
  Uint32        m_connect_count;
private:
  Uint32        m_max_api_reg_req_interval;
  Uint32        noOfAliveNodes;
  Uint32        noOfConnectedNodes;
  Uint32        minDbVersion;
  Node          theNodes[MAX_NODES];
  NdbThread*    theClusterMgrThread;

  NodeBitmask   waitForHBFromNodes; // used in forcing HBs
  NdbCondition* waitForHBCond;
  bool          waitingForHB;

  enum Cluster_state m_cluster_state;
  /**
   * Used for controlling start/stop of the thread
   */
  NdbMutex*     clusterMgrThreadMutex;

  /**
   * Signals received
   */
  void execAPI_REGREQ    (const Uint32 * theData);
  void execAPI_REGCONF   (const NdbApiSignal*, const LinearSectionPtr ptr[]);
  void execAPI_REGREF    (const Uint32 * theData);
  void execCONNECT_REP   (const NdbApiSignal*, const LinearSectionPtr ptr[]);
  void execDISCONNECT_REP(const NdbApiSignal*, const LinearSectionPtr ptr[]);
  void execNODE_FAILREP  (const NdbApiSignal*, const LinearSectionPtr ptr[]);
  void execNF_COMPLETEREP(const NdbApiSignal*, const LinearSectionPtr ptr[]);

  void check_wait_for_hb(NodeId nodeId);

  inline void set_node_alive(trp_node& node, bool alive){

    // Only DB nodes can be "alive"
    assert(!alive ||
           (alive && node.m_info.getType() == NodeInfo::DB));

    if(node.m_alive && !alive)
    {
      assert(noOfAliveNodes);
      noOfAliveNodes--;
    }
    else if(!node.m_alive && alive)
    {
      noOfAliveNodes++;
    }
    node.m_alive = alive;
  }

  void set_node_dead(trp_node&);

  void print_nodes(const char* where, NdbOut& out = ndbout);
  void recalcMinDbVersion();

public:
  /**
   * trp_client interface
   */
  virtual void trp_deliver_signal(const NdbApiSignal*,
                                  const LinearSectionPtr p[3]);
};

inline
const trp_node &
ClusterMgr::getNodeInfo(NodeId nodeId) const {
  // Check array bounds
  assert(nodeId < MAX_NODES);
  return theNodes[nodeId];
}

inline
Uint32
ClusterMgr::getNoOfConnectedNodes() const {
  return noOfConnectedNodes;
}

inline
void
ClusterMgr::hb_received(NodeId nodeId) {
  // Check array bounds + don't allow node 0 to be touched
  assert(nodeId > 0 && nodeId < MAX_NODES);
  theNodes[nodeId].hbMissed = 0;
}

/*****************************************************************************/

/**
 * @class ArbitMgr
 * Arbitration manager.  Runs in separate thread.
 * Started only by a request from the kernel.
 */

extern "C" void* runArbitMgr_C(void* me);

class ArbitMgr
{
public:
  ArbitMgr(class ClusterMgr &);
  ~ArbitMgr();

  inline void setRank(unsigned n) { theRank = n; }
  inline void setDelay(unsigned n) { theDelay = n; }

  void doStart(const Uint32* theData);
  void doChoose(const Uint32* theData);
  void doStop(const Uint32* theData);

  friend void* runArbitMgr_C(void* me);

private:
  class ClusterMgr & m_clusterMgr;
  unsigned theRank;
  unsigned theDelay;

  void threadMain();
  NdbThread* theThread;
  NdbMutex* theThreadMutex;     // not really needed

  struct ArbitSignal {
    GlobalSignalNumber gsn;
    ArbitSignalData data;
    NDB_TICKS timestamp;

    ArbitSignal() {}

    inline void init(GlobalSignalNumber aGsn, const Uint32* aData) {
      gsn = aGsn;
      if (aData != NULL)
        memcpy(&data, aData, sizeof(data));
      else
        memset(&data, 0, sizeof(data));
    }

    inline void setTimestamp() {
      timestamp = NdbTick_CurrentMillisecond();
    }

    inline NDB_TICKS getTimediff() {
      NDB_TICKS now = NdbTick_CurrentMillisecond();
      return now < timestamp ? 0 : now - timestamp;
    }
  };

  NdbMutex* theInputMutex;
  NdbCondition* theInputCond;
  int theInputTimeout;
  bool theInputFull;            // the predicate
  ArbitSignal theInputBuffer;   // shared buffer

  void sendSignalToThread(ArbitSignal& aSignal);

  enum State {                  // thread states
    StateInit,
    StateStarted,               // thread started
    StateChoose1,               // received one valid REQ
    StateChoose2,               // received two valid REQs
    StateFinished               // finished one way or other
  };
  State theState;

  enum Stop {                   // stop code in ArbitSignal.data.code
    StopExit = 1,               // at API exit
    StopRequest = 2,            // request from kernel
    StopRestart = 3             // stop before restart
  };

  void threadStart(ArbitSignal& aSignal);       // handle thread events
  void threadChoose(ArbitSignal& aSignal);
  void threadTimeout();
  void threadStop(ArbitSignal& aSignal);

  ArbitSignal theStartReq;
  ArbitSignal theChooseReq1;
  ArbitSignal theChooseReq2;
  ArbitSignal theStopOrd;

  void sendStartConf(ArbitSignal& aSignal, Uint32);
  void sendChooseRef(ArbitSignal& aSignal, Uint32);
  void sendChooseConf(ArbitSignal& aSignal, Uint32);
  void sendStopRep(ArbitSignal& aSignal, Uint32);

  void sendSignalToQmgr(ArbitSignal& aSignal);
};

#endif
