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

#ifndef TransporterFacade_H
#define TransporterFacade_H

#include <kernel_types.h>
#include <ndb_limits.h>
#include <NdbThread.h>
#include <TransporterRegistry.hpp>
#include <NdbMutex.h>
#include "DictCache.hpp"
#include <BlockNumbers.h>

class ClusterMgr;
class ArbitMgr;
class IPCConfig;
struct ndb_mgm_configuration;
class ConfigRetriever;

class Ndb;
class NdbApiSignal;

typedef void (* ExecuteFunction)(void *, NdbApiSignal *, LinearSectionPtr ptr[3]);
typedef void (* NodeStatusFunction)(void *, Uint32, bool nodeAlive, bool nfComplete);

extern "C" {
  void* runSendRequest_C(void*);
  void* runReceiveResponse_C(void*);
  void atexit_stop_instance();
}

/**
 * Max number of Ndb objects in different threads.  
 * (Ndb objects should not be shared by different threads.)
 */
class TransporterFacade
{
public:
  TransporterFacade();
  virtual ~TransporterFacade();
  bool init(Uint32, const ndb_mgm_configuration *);

  static TransporterFacade* instance();
  int start_instance(int, const ndb_mgm_configuration*);
  static void stop_instance();
  
  /**
   * Register this block for sending/receiving signals
   * @return BlockNumber or -1 for failure
   */
  int open(void* objRef, ExecuteFunction, NodeStatusFunction);
  
  // Close this block number
  int close(BlockNumber blockNumber, Uint64 trans_id);

  // Only sends to nodes which are alive
  int sendSignal(NdbApiSignal * signal, NodeId nodeId);
  int sendFragmentedSignal(NdbApiSignal*, NodeId, 
			   LinearSectionPtr ptr[3], Uint32 secs);

  //Dirrrrty
  int sendFragmentedSignalUnCond(NdbApiSignal*, NodeId, 
				 LinearSectionPtr ptr[3], Uint32 secs);

  
  // Is node available for running transactions
  bool   get_node_alive(NodeId nodeId) const;
  bool   get_node_stopping(NodeId nodeId) const;
  bool   getIsDbNode(NodeId nodeId) const;
  bool   getIsNodeSendable(NodeId nodeId) const;
  Uint32 getNodeGrp(NodeId nodeId) const;
  Uint32 getNodeSequence(NodeId nodeId) const;

  // Is there space in sendBuffer to send messages
  bool   check_send_size(Uint32 node_id, Uint32 send_size);

  // My own processor id
  NodeId ownId() const;

  void connected();

  void doConnect(int NodeId);
  void reportConnected(int NodeId);
  void doDisconnect(int NodeId);
  void reportDisconnected(int NodeId);

  NodeId get_an_alive_node();
  void ReportNodeAlive(NodeId nodeId);
  void ReportNodeDead(NodeId nodeId);
  void ReportNodeFailureComplete(NodeId nodeId);
  
  void lock_mutex();
  void unlock_mutex();

  // Improving the API performance
  void forceSend(Uint32 block_number);
  void checkForceSend(Uint32 block_number);

  // Close this block number
  int close_local(BlockNumber blockNumber);

  // Scan batch configuration parameters
  Uint32 get_scan_batch_size();
  Uint32 get_batch_byte_size();
  Uint32 get_batch_size();

private:
  /**
   * Send a signal unconditional of node status (used by ClusterMgr)
   */
  friend class ClusterMgr;
  friend class ArbitMgr;
  friend class MgmtSrvr;
  friend class SignalSender;
  friend class GrepPS;
  friend class ExtSender; ///< @todo Hack to be able to sendSignalUnCond
  friend class GrepSS;
  friend class Ndb;
  friend class Ndb_cluster_connection;

  int sendSignalUnCond(NdbApiSignal *, NodeId nodeId);

  bool isConnected(NodeId aNodeId);
  void doStop();
  
  TransporterRegistry* theTransporterRegistry;
  int sendPerformedLastInterval;
  int theOwnId;

  NodeId theStartNodeId;

  ClusterMgr* theClusterMgr;
  ArbitMgr* theArbitMgr;
  
  // Improving the API response time
  int checkCounter;
  Uint32 currentSendLimit;
  
  void calculateSendLimit();

  // Scan batch configuration parameters
  Uint32 m_scan_batch_size;
  Uint32 m_batch_byte_size;
  Uint32 m_batch_size;

  // Declarations for the receive and send thread
  int  theStopReceive;

  void threadMainSend(void);
  NdbThread* theSendThread;
  void threadMainReceive(void);
  NdbThread* theReceiveThread;

  friend void* runSendRequest_C(void*);
  friend void* runReceiveResponse_C(void*);
  friend void atexit_stop_instance();

  /**
   * Block number handling
   */
public:
  static const unsigned MAX_NO_THREADS = 4711;
private:

  struct ThreadData {
    static const Uint32 ACTIVE = (1 << 16) | 1;
    static const Uint32 INACTIVE = (1 << 16);
    static const Uint32 END_OF_LIST = MAX_NO_THREADS + 1;
    
    ThreadData(Uint32 initialSize = 32);
    
    /**
     * Split "object" into 3 list
     *   This to improve locality
     *   when iterating over lists
     */
    struct Object_Execute {
      void * m_object;
      ExecuteFunction m_executeFunction;
    };
    struct NodeStatus_NextFree {
      NodeStatusFunction m_statusFunction;
    };

    Uint32 m_firstFree;
    Vector<Uint32> m_statusNext;
    Vector<Object_Execute> m_objectExecute;
    Vector<NodeStatusFunction> m_statusFunction;
    
    int open(void* objRef, ExecuteFunction, NodeStatusFunction);
    int close(int number);
    void expand(Uint32 size);

    inline Object_Execute get(Uint16 blockNo) const {
      blockNo -= MIN_API_BLOCK_NO;
      if(blockNo < m_objectExecute.size()){
	return m_objectExecute[blockNo];
      }
      Object_Execute oe = { 0, 0 };
      return oe;
    }

    /**
     * Is the block number used currently
     */
    inline bool getInUse(Uint16 index) const {
      return (m_statusNext[index] & (1 << 16)) != 0;
    }
  } m_threads;
  
  Uint32 m_max_trans_id;
  
  /**
   * execute function
   */
  friend void execute(void * callbackObj, SignalHeader * const header, 
                      Uint8 prio, 
                      Uint32 * const theData, LinearSectionPtr ptr[3]);
  
public:
  NdbMutex* theMutexPtr;
private:
  static TransporterFacade* theFacadeInstance;
  static ConfigRetriever *s_config_retriever;

public:
  GlobalDictCache m_globalDictCache;
};

inline
TransporterFacade*
TransporterFacade::instance()
{
  return theFacadeInstance;
}

inline
void 
TransporterFacade::lock_mutex()
{
  NdbMutex_Lock(theMutexPtr);
}

inline
void 
TransporterFacade::unlock_mutex()
{
  NdbMutex_Unlock(theMutexPtr);
}

#include "ClusterMgr.hpp"

inline
bool
TransporterFacade::check_send_size(Uint32 node_id, Uint32 send_size)
{
  return true;
}

inline
bool
TransporterFacade::getIsDbNode(NodeId n) const {
  return 
    theClusterMgr->getNodeInfo(n).defined && 
    theClusterMgr->getNodeInfo(n).m_info.m_type == NodeInfo::DB;
}

inline
Uint32
TransporterFacade::getNodeGrp(NodeId n) const {
  return theClusterMgr->getNodeInfo(n).m_state.nodeGroup;
}


inline
bool
TransporterFacade::get_node_alive(NodeId n) const {

  const ClusterMgr::Node & node = theClusterMgr->getNodeInfo(n);
  return node.m_alive;
}

inline
bool
TransporterFacade::get_node_stopping(NodeId n) const {
  const ClusterMgr::Node & node = theClusterMgr->getNodeInfo(n);
  return ((node.m_state.startLevel == NodeState::SL_STOPPING_1) ||
          (node.m_state.startLevel == NodeState::SL_STOPPING_2));
}

inline
bool
TransporterFacade::getIsNodeSendable(NodeId n) const {
  const ClusterMgr::Node & node = theClusterMgr->getNodeInfo(n);
  const Uint32 startLevel = node.m_state.startLevel;

  if (node.m_info.m_type == NodeInfo::DB) {
    if(node.m_state.singleUserMode && 
       ownId() == node.m_state.singleUserApi) {
      return (node.compatible && 
              (node.m_state.startLevel == NodeState::SL_STOPPING_1 ||
               node.m_state.startLevel == NodeState::SL_STARTED ||
               node.m_state.startLevel == NodeState::SL_SINGLEUSER));
      }
      else
        return node.compatible && (startLevel == NodeState::SL_STARTED ||
                                 startLevel == NodeState::SL_STOPPING_1);
  } else if (node.m_info.m_type == NodeInfo::REP) {
    /**
     * @todo Check that REP node actually has received API_REG_REQ
     */
    return node.compatible;
  } else {
    ndbout_c("TransporterFacade::getIsNodeSendable: Illegal node type: "
             "%d of node: %d", 
             node.m_info.m_type, n);
    abort();
    return false; // to remove compiler warning
  }
}

inline
Uint32
TransporterFacade::getNodeSequence(NodeId n) const {
  return theClusterMgr->getNodeInfo(n).m_info.m_connectCount;
}

inline
Uint32
TransporterFacade::get_scan_batch_size() {
  return m_scan_batch_size;
}

inline
Uint32
TransporterFacade::get_batch_byte_size() {
  return m_batch_byte_size;
}

inline
Uint32
TransporterFacade::get_batch_size() {
  return m_batch_size;
}



#endif // TransporterFacade_H
