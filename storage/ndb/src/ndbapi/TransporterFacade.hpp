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

#ifndef TransporterFacade_H
#define TransporterFacade_H

#include <kernel_types.h>
#include <ndb_limits.h>
#include <NdbThread.h>
#include <TransporterRegistry.hpp>
#include <NdbMutex.h>
#include "DictCache.hpp"
#include <BlockNumbers.h>
#include <mgmapi.h>

class ClusterMgr;
class ArbitMgr;
struct ndb_mgm_configuration;

class Ndb;
class NdbApiSignal;
class NdbWaiter;
class trp_client;

extern "C" {
  void* runSendRequest_C(void*);
  void* runReceiveResponse_C(void*);
}

class TransporterFacade :
  public TransporterCallback,
  public TransporterReceiveHandle
{
public:
  /**
   * Max number of Ndb objects.  
   * (Ndb objects should not be shared by different threads.)
   */
  STATIC_CONST( MAX_NO_THREADS = 4711 );
  TransporterFacade(GlobalDictCache *cache);
  virtual ~TransporterFacade();

  int start_instance(NodeId, const ndb_mgm_configuration*);
  void stop_instance();

  /*
    (Re)configure the TransporterFacade
    to a specific configuration
  */
  bool configure(NodeId, const ndb_mgm_configuration *);

  /**
   * Register this block for sending/receiving signals
   * @blockNo block number to use, -1 => any blockNumber
   * @return BlockNumber or -1 for failure
   */
  Uint32 open_clnt(trp_client*, int blockNo = -1);
  int close_clnt(trp_client*);

  Uint32 get_active_ndb_objects() const;

  /** 
   * Get/Set wait time in the send thread.
   */
 void setSendThreadInterval(Uint32 ms);
 Uint32 getSendThreadInterval(void);

  // Only sends to nodes which are alive
private:
  int sendSignal(const NdbApiSignal * signal, NodeId nodeId);
  int sendSignal(const NdbApiSignal*, NodeId,
                 const LinearSectionPtr ptr[3], Uint32 secs);
  int sendSignal(const NdbApiSignal*, NodeId,
                 const GenericSectionPtr ptr[3], Uint32 secs);
  int sendFragmentedSignal(const NdbApiSignal*, NodeId,
                           const LinearSectionPtr ptr[3], Uint32 secs);
  int sendFragmentedSignal(const NdbApiSignal*, NodeId,
                           const GenericSectionPtr ptr[3], Uint32 secs);
public:

  /**
   * These are functions used by ndb_mgmd
   */
  void ext_set_max_api_reg_req_interval(Uint32 ms);
  void ext_update_connections();
  struct in_addr ext_get_connect_address(Uint32 nodeId);
  void ext_forceHB();
  bool ext_isConnected(NodeId aNodeId);
  void ext_doConnect(int aNodeId);

  // Is node available for running transactions
private:
  bool   get_node_alive(NodeId nodeId) const;
  bool   getIsNodeSendable(NodeId nodeId) const;

public:
  Uint32 getMinDbNodeVersion() const;

  // My own processor id
  NodeId ownId() const;

  void connected();

  void doConnect(int NodeId);
  void reportConnected(int NodeId);
  void doDisconnect(int NodeId);
  void reportDisconnected(int NodeId);

  NodeId get_an_alive_node();
  void trp_node_status(NodeId, Uint32 event);

  /**
   * Send signal to each registered object
   */
  void for_each(trp_client* clnt,
                const NdbApiSignal* aSignal, const LinearSectionPtr ptr[3]);
  
  void lock_mutex();
  void unlock_mutex();

  // Improving the API performance
  void forceSend(Uint32 block_number);
  int checkForceSend(Uint32 block_number);

  TransporterRegistry* get_registry() { return theTransporterRegistry;};

/*
  When a thread has sent its signals and is ready to wait for reception
  of these it does normally always wait on a conditional mutex and
  the actual reception is handled by the receiver thread in the NDB API.
  With the below new methods and variables each thread has the possibility
  of becoming owner of the "right" to poll for signals. Effectually this
  means that the thread acts temporarily as a receiver thread.
  For the thread that succeeds in grabbing this "ownership" it will avoid
  a number of expensive calls to conditional mutex and even more expensive
  context switches to wake up.
  When an owner of the poll "right" has completed its own task it is likely
  that there are others still waiting. In this case we pick one of the
  threads as new owner of the poll "right". Since we want to switch owner
  as seldom as possible we always pick the last thread which is likely to
  be the last to complete its reception.
*/
  void start_poll(trp_client*);
  void do_poll(trp_client* clnt, Uint32 wait_time);
  void complete_poll(trp_client*);
  void wakeup(trp_client*);

  void external_poll(Uint32 wait_time);

  trp_client* get_poll_owner(bool) const { return m_poll_owner;}
  trp_client* remove_last_from_poll_queue();
  void add_to_poll_queue(trp_client* clnt);
  void remove_from_poll_queue(trp_client* clnt);

  trp_client * m_poll_owner;
  trp_client * m_poll_queue_head; // First in queue
  trp_client * m_poll_queue_tail; // Last in queue
  /* End poll owner stuff */

  // heart beat received from a node (e.g. a signal came)
  void hb_received(NodeId n);
  void set_auto_reconnect(int val);
  int get_auto_reconnect() const;

  /* TransporterCallback interface. */
  void deliver_signal(SignalHeader * const header,
                      Uint8 prio,
                      Uint32 * const signalData,
                      LinearSectionPtr ptr[3]);
  int checkJobBuffer();
  void reportSendLen(NodeId nodeId, Uint32 count, Uint64 bytes);
  void reportReceiveLen(NodeId nodeId, Uint32 count, Uint64 bytes);
  void reportConnect(NodeId nodeId);
  void reportDisconnect(NodeId nodeId, Uint32 errNo);
  void reportError(NodeId nodeId, TransporterError errorCode,
                   const char *info = 0);
  void transporter_recv_from(NodeId node);
  Uint32 get_bytes_to_send_iovec(NodeId node, struct iovec *dst, Uint32 max)
  {
    return theTransporterRegistry->get_bytes_to_send_iovec(node, dst, max);
  }
  Uint32 bytes_sent(NodeId node, Uint32 bytes)
  {
    return theTransporterRegistry->bytes_sent(node, bytes);
  }
  bool has_data_to_send(NodeId node)
  {
    return theTransporterRegistry->has_data_to_send(node);
  }
  void reset_send_buffer(NodeId node, bool should_be_empty)
  {
    theTransporterRegistry->reset_send_buffer(node, should_be_empty);
  }
  /**
   * Wakeup
   *
   * Clients normally block waiting for a pattern of signals,
   * or until a timeout expires.
   * This Api allows them to be woken early.
   * To use it, a setupWakeup() call must be made once prior
   * to using the Apis in any client.
   *
   */
  bool setupWakeup();
  bool registerForWakeup(trp_client* dozer);
  bool unregisterForWakeup(trp_client* dozer);
  void requestWakeup();
  void reportWakeup();

private:

  friend class trp_client;
  friend class ClusterMgr;
  friend class ArbitMgr;
  friend class Ndb_cluster_connection;
  friend class Ndb_cluster_connection_impl;

  bool isConnected(NodeId aNodeId);
  void doStop();

  TransporterRegistry* theTransporterRegistry;
  SocketServer m_socket_server;
  int sendPerformedLastInterval;
  NodeId theOwnId;
  NodeId theStartNodeId;

  ClusterMgr* theClusterMgr;
  
  // Improving the API response time
  int checkCounter;
  Uint32 currentSendLimit;
  
  void calculateSendLimit();

  /* Single dozer supported currently.
   * In future, use a DLList to support > 1
   */
  trp_client * dozer;

  // Declarations for the receive and send thread
  int  theStopReceive;
  Uint32 sendThreadWaitMillisec;

  void threadMainSend(void);
  NdbThread* theSendThread;
  void threadMainReceive(void);
  NdbThread* theReceiveThread;

  friend void* runSendRequest_C(void*);
  friend void* runReceiveResponse_C(void*);

  bool do_connect_mgm(NodeId, const ndb_mgm_configuration*);

  /**
   * Block number handling
   */
private:

  struct ThreadData {
    STATIC_CONST( ACTIVE = (1 << 16) | 1 );
    STATIC_CONST( INACTIVE = (1 << 16) );
    STATIC_CONST( END_OF_LIST = MAX_NO_THREADS + 1 );
    
    ThreadData(Uint32 initialSize = 32);
    
    Uint32 m_use_cnt;
    Uint32 m_firstFree;
    Vector<Uint32> m_statusNext;
    Vector<trp_client*> m_objectExecute;
    
    int open(trp_client*);
    int close(int number);
    void expand(Uint32 size);

    inline trp_client* get(Uint16 blockNo) const {
      blockNo -= MIN_API_BLOCK_NO;
      if(likely (blockNo < m_objectExecute.size()))
      {
        return m_objectExecute.getBase()[blockNo];
      }
      return 0;
    }
  } m_threads;

  Uint32 m_fixed2dynamic[NO_API_FIXED_BLOCKS];
  Uint32 m_fragmented_signal_id;

public:
  NdbMutex* theMutexPtr;

public:
  GlobalDictCache *m_globalDictCache;
};

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
#include "ndb_cluster_connection_impl.hpp"

inline
unsigned Ndb_cluster_connection_impl::get_connect_count() const
{
  if (m_transporter_facade->theClusterMgr)
    return m_transporter_facade->theClusterMgr->m_connect_count;
  return 0;
}

inline
unsigned Ndb_cluster_connection_impl::get_min_db_version() const
{
  return m_transporter_facade->getMinDbNodeVersion();
}

inline
bool
TransporterFacade::get_node_alive(NodeId n) const {
  if (theClusterMgr)
  {
    return theClusterMgr->getNodeInfo(n).m_alive;
  }
  return 0;
}

inline
void
TransporterFacade::hb_received(NodeId n) {
  theClusterMgr->hb_received(n);
}

inline
Uint32
TransporterFacade::getMinDbNodeVersion() const
{
  if (theClusterMgr)
    return theClusterMgr->minDbVersion;
  else
    return 0;
}

inline
const trp_node &
trp_client::getNodeInfo(Uint32 nodeId) const
{
  return m_facade->theClusterMgr->getNodeInfo(nodeId);
}

/** 
 * LinearSectionIterator
 *
 * This is an implementation of GenericSectionIterator 
 * that iterates over one linear section of memory.
 * The iterator is used by the transporter at signal
 * send time to obtain all of the relevant words for the
 * signal section
 */
class LinearSectionIterator: public GenericSectionIterator
{
private :
  const Uint32* data;
  Uint32 len;
  bool read;
public :
  LinearSectionIterator(const Uint32* _data, Uint32 _len)
  {
    data= (_len == 0)? NULL:_data;
    len= _len;
    read= false;
  }

  ~LinearSectionIterator()
  {};
  
  void reset()
  {
    /* Reset iterator */
    read= false;
  }

  const Uint32* getNextWords(Uint32& sz)
  {
    if (likely(!read))
    {
      read= true;
      sz= len;
      return data;
    }
    sz= 0;
    return NULL;
  }
};


/** 
 * SignalSectionIterator
 *
 * This is an implementation of GenericSectionIterator 
 * that uses chained NdbApiSignal objects to store a 
 * signal section.
 * The iterator is used by the transporter at signal
 * send time to obtain all of the relevant words for the
 * signal section
 */
class SignalSectionIterator: public GenericSectionIterator
{
private :
  NdbApiSignal* firstSignal;
  NdbApiSignal* currentSignal;
public :
  SignalSectionIterator(NdbApiSignal* signal)
  {
    firstSignal= currentSignal= signal;
  }

  ~SignalSectionIterator()
  {};
  
  void reset()
  {
    /* Reset iterator */
    currentSignal= firstSignal;
  }

  const Uint32* getNextWords(Uint32& sz);
};

/*
 * GenericSectionIteratorReader
 * Helper class to simplify reading data from 
 * GenericSectionIterator implementations
 */

class GSIReader
{
private :
  GenericSectionIterator* gsi;
  const Uint32* chunkPtr;
  Uint32 chunkRemain;
public :
  GSIReader(GenericSectionIterator* _gsi)
  {
    gsi = _gsi;
    chunkPtr = NULL;
    chunkRemain = 0;
  }

  void copyNWords(Uint32* dest, Uint32 n)
  {
    while (n)
    {
      if (chunkRemain == 0)
      {
        /* Get next contiguous stretch of words from
         * the iterator
         */
        chunkPtr = gsi->getNextWords(chunkRemain);
        if (!chunkRemain)
          abort(); // Must have the words the caller asks for
      }
      else
      {
        /* Have some words from the iterator, copy some/
         * all of them
         */
        Uint32 wordsToCopy = MIN(chunkRemain, n);
        memcpy(dest, chunkPtr, wordsToCopy << 2);
        chunkPtr += wordsToCopy;
        chunkRemain -= wordsToCopy;

        dest += wordsToCopy;
        n -= wordsToCopy;
      }
    }
  }
};

  


#endif // TransporterFacade_H
