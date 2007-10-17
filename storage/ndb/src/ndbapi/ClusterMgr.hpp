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

#ifndef ClusterMgr_H
#define ClusterMgr_H

#include "API.hpp"
#include <ndb_limits.h>
#include <NdbThread.h>
#include <NdbMutex.h>
#include <NdbCondition.h>
#include <signaldata/ArbitSignalData.hpp>
#include <signaldata/NodeStateSignalData.hpp>
#include <NodeInfo.hpp>
#include <NodeState.hpp>

extern "C" void* runClusterMgr_C(void * me);


/**
 * @class ClusterMgr
 */
class ClusterMgr {
  friend void* runClusterMgr_C(void * me);
  friend void  execute(void *, struct SignalHeader * const, 
		       Uint8, Uint32 * const, LinearSectionPtr ptr[3]);
public:
  ClusterMgr(class TransporterFacade &);
  ~ClusterMgr();
  void init(struct ndb_mgm_configuration_iterator & config);
  
  void reportConnected(NodeId nodeId);
  void reportDisconnected(NodeId nodeId);
  
  bool checkUpgradeCompatability(Uint32 nodeVersion);

  void doStop();
  void startThread();

  void forceHB();
  void set_max_api_reg_req_interval(unsigned int millisec) { m_max_api_reg_req_interval = millisec; }

private:
  void threadMain();
  
  int  theStop;
  class TransporterFacade & theFacade;
  
public:
  enum Cluster_state {
    CS_waiting_for_clean_cache = 0,
    CS_waiting_for_first_connect,
    CS_connected
  };
  struct Node {
    Node();
    bool defined;
    bool connected;     // Transporter connected
    bool compatible;    // Version is compatible
    bool nfCompleteRep; // NF Complete Rep has arrived
    bool m_alive;       // Node is alive
    bool m_api_reg_conf;// API_REGCONF has arrived
    
    NodeInfo  m_info;
    NodeState m_state;

    /**
     * Heartbeat stuff
     */
    Uint32 hbFrequency; // Heartbeat frequence 
    Uint32 hbCounter;   // # milliseconds passed since last hb sent
  };
  
  const Node &  getNodeInfo(NodeId) const;
  Uint32        getNoOfConnectedNodes() const;
  bool          isClusterAlive() const;
  void          hb_received(NodeId);

  Uint32        m_connect_count;
private:
  Uint32        m_max_api_reg_req_interval;
  Uint32        noOfAliveNodes;
  Uint32        noOfConnectedNodes;
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
  
  void showState(NodeId nodeId);
  void reportNodeFailed(NodeId nodeId, bool disconnect = false);
  
  /**
   * Signals received
   */
  void execAPI_REGREQ    (const Uint32 * theData);
  void execAPI_REGCONF   (const Uint32 * theData);
  void execAPI_REGREF    (const Uint32 * theData);
  void execNODE_FAILREP  (const Uint32 * theData);
  void execNF_COMPLETEREP(const Uint32 * theData);

  inline void set_node_alive(Node& node, bool alive){
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
};

inline
const ClusterMgr::Node &
ClusterMgr::getNodeInfo(NodeId nodeId) const {
  return theNodes[nodeId];
}

inline
Uint32
ClusterMgr::getNoOfConnectedNodes() const {
  return noOfConnectedNodes;
}

inline
bool
ClusterMgr::isClusterAlive() const {
  return noOfAliveNodes != 0;
}
inline
void
ClusterMgr::hb_received(NodeId nodeId) {
  theNodes[nodeId].m_info.m_heartbeat_cnt= 0;
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
  ArbitMgr(class TransporterFacade &);
  ~ArbitMgr();

  inline void setRank(unsigned n) { theRank = n; }
  inline void setDelay(unsigned n) { theDelay = n; }

  void doStart(const Uint32* theData);
  void doChoose(const Uint32* theData);
  void doStop(const Uint32* theData);

  friend void* runArbitMgr_C(void* me);

private:
  class TransporterFacade & theFacade;
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
