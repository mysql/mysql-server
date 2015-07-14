/*
   Copyright (c) 2003, 2015, Oracle and/or its affiliates. All rights reserved.

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
#include <Vector.hpp>
#include "DictCache.hpp"
#include <BlockNumbers.h>
#include <mgmapi.h>
#include "trp_buffer.hpp"

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
  void perform_close_clnt(trp_client*);
  void expand_clnt();

  Uint32 get_active_ndb_objects() const;

  /** 
   * Get/Set wait time in the send thread.
   */
 void setSendThreadInterval(Uint32 ms);
 Uint32 getSendThreadInterval(void);

  // Only sends to nodes which are alive
private:
  int sendSignal(trp_client*, const NdbApiSignal *, NodeId nodeId);
  int sendSignal(trp_client*, const NdbApiSignal*, NodeId,
                 const LinearSectionPtr ptr[3], Uint32 secs);
  int sendSignal(trp_client*, const NdbApiSignal*, NodeId,
                 const GenericSectionPtr ptr[3], Uint32 secs);
  int sendFragmentedSignal(trp_client*, const NdbApiSignal*, NodeId,
                           const LinearSectionPtr ptr[3], Uint32 secs);
  int sendFragmentedSignal(trp_client*, const NdbApiSignal*, NodeId,
                           const GenericSectionPtr ptr[3], Uint32 secs);

  /* Support routine to configure */
  void set_up_node_active_in_send_buffers(Uint32 nodeId,
                           const ndb_mgm_configuration &conf);

public:

  /**
   * These are functions used by ndb_mgmd
   */
  void ext_set_max_api_reg_req_interval(Uint32 ms);
  struct in_addr ext_get_connect_address(Uint32 nodeId);
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

  bool is_cluster_completely_unavailable();

  /**
   * Send signal to each registered object
   */
  void for_each(trp_client* clnt,
                const NdbApiSignal* aSignal, const LinearSectionPtr ptr[3]);
  
  void lock_poll_mutex();
  void unlock_poll_mutex();

  TransporterRegistry* get_registry() { return theTransporterRegistry;};

/*
  When a thread has sent its signals and is ready to wait for reception
  of these it does normally always wait on a conditional mutex and
  the actual reception is handled by the receiver thread in the NDB API.
  With the below new methods and variables each thread has the possibility
  of becoming owner of the "right" to poll for signals. Effectually this
  means that the thread acts temporarily as a receiver thread.
  There is also a dedicated receiver thread (threadMainReceive) which will
  be activated to off load the client threads if the load is sufficient high.
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
  bool do_poll(trp_client* clnt,
               Uint32 wait_time,
               bool is_poll_owner = false,
               bool stay_poll_owner = false);
  void complete_poll(trp_client*);
  void wakeup(trp_client*);

  void external_poll(Uint32 wait_time);

  void remove_from_poll_queue(trp_client* const *, Uint32 cnt);
  void unlock_and_signal(trp_client* const *, Uint32 cnt);

  trp_client* get_poll_owner(bool) const { return m_poll_owner;}
  trp_client* remove_last_from_poll_queue();
  void add_to_poll_queue(trp_client* clnt);
  void remove_from_poll_queue(trp_client* clnt);

  /*
    Optimize detection of connection state changes by requesting 
    an ::update_connections() to be done in the next do_poll().
  */
  void request_connection_check()
  { m_check_connections = true; }

  /*
    Configuration handling of the receiver threads handling of polling
    These methods implement methods on the ndb_cluster_connection
    interface.
  */
#define NO_RECV_THREAD_CPU_ID 0xFFFF
  int unset_recv_thread_cpu(Uint32 recv_thread_id);
  int set_recv_thread_cpu(Uint16 *cpuid_array,
                          Uint32 array_len,
                          Uint32 recv_thread_id);
  int set_recv_thread_activation_threshold(Uint32 threshold);
  int get_recv_thread_activation_threshold() const;
  /* Variables to support configuration of receiver thread handling */
  Uint32 min_active_clients_recv_thread;
  Uint16 recv_thread_cpu_id;
  /* Support methods to lock/unlock the receiver thread to/from its CPU */
  void lock_recv_thread_cpu();
  void unlock_recv_thread_cpu();

  trp_client * m_poll_owner;
  trp_client * m_poll_queue_head; // First in queue
  trp_client * m_poll_queue_tail; // Last in queue
  /* End poll owner stuff */

  // heart beat received from a node (e.g. a signal came)
  void hb_received(NodeId n);
  void set_auto_reconnect(int val);
  int get_auto_reconnect() const;

  /* TransporterCallback interface. */
  bool deliver_signal(SignalHeader * const header,
                      Uint8 prio,
                      Uint32 * const signalData,
                      LinearSectionPtr ptr[3]);
  void handleMissingClnt(const SignalHeader * header,
                         const Uint32 * theData);

  int checkJobBuffer();
  void reportSendLen(NodeId nodeId, Uint32 count, Uint64 bytes);
  void reportReceiveLen(NodeId nodeId, Uint32 count, Uint64 bytes);
  void reportConnect(NodeId nodeId);
  void reportDisconnect(NodeId nodeId, Uint32 errNo);
  void reportError(NodeId nodeId, TransporterError errorCode,
                   const char *info = 0);
  void transporter_recv_from(NodeId node);

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

  bool try_become_poll_owner(trp_client* clnt, Uint32 wait_time);
  static void finish_poll(trp_client* clnt,
                          Uint32 cnt,
                          Uint32& cnt_woken,
                          trp_client** arr);
  void try_lock_last_client(trp_client* clnt,
                            bool &new_owner_locked,
                            trp_client** new_owner_ptr,
                            Uint32 first_check);

  Uint32 m_num_active_clients;
  volatile bool m_check_connections;

  TransporterRegistry* theTransporterRegistry;
  SocketServer m_socket_server;
  int sendPerformedLastInterval;
  NodeId theOwnId;
  NodeId theStartNodeId;

  ClusterMgr* theClusterMgr;
  
  /* Single dozer supported currently.
   * In future, use a DLList to support > 1
   */
  trp_client * dozer;

  // Declarations for the receive and send thread
  int  theStopReceive;
  int  theStopSend;
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

    struct Client {
      trp_client* m_clnt;
      Uint32 m_next;

      Client()
	: m_clnt(NULL), m_next(END_OF_LIST) {};

      Client(trp_client* clnt, Uint32 next)
	: m_clnt(clnt), m_next(next) {};
    };
    Vector<struct Client> m_clients;

    /**
     * open, close and expand need to hold the m_open_close_mutex.
     * In addition, close and expand need the poll right to 
     * serialize access with get (also need poll right)
     */
    int open(trp_client*);
    int close(int number);
    void expand(Uint32 size);

    inline trp_client* get(Uint16 blockNo) const {
      blockNo -= MIN_API_BLOCK_NO;
      if(likely (blockNo < m_clients.size()))
      {
        return m_clients[blockNo].m_clnt;
      }
      return 0;
    }

    Uint32 freeCnt() const {
      return m_clients.size() - m_use_cnt;
    }

  } m_threads;

  Uint32 m_fixed2dynamic[NO_API_FIXED_BLOCKS];
  Uint32 m_fragmented_signal_id;

public:
  NdbMutex* m_open_close_mutex;
  NdbMutex* thePollMutex;

public:
  GlobalDictCache *m_globalDictCache;

public:
  /**
   * Add a send buffer to out-buffer
   */
  void flush_send_buffer(Uint32 node, const TFBuffer* buffer);
  void flush_and_send_buffer(Uint32 node, const TFBuffer* buffer);

  /**
   * Allocate a send buffer
   */
  TFPage *alloc_sb_page() { return m_send_buffer.try_alloc(1);}

  Uint32 get_bytes_to_send_iovec(NodeId node, struct iovec *dst, Uint32 max);
  Uint32 bytes_sent(NodeId node, Uint32 bytes);
  bool has_data_to_send(NodeId node);
  void reset_send_buffer(NodeId node, bool should_be_empty);

private:
  TFMTPool m_send_buffer;
  struct TFSendBuffer
  {
    TFSendBuffer()
    {
      m_sending = false;
      m_reset = false;
      m_node_active = false;
    }

    /**
     * Protection of struct members:
     * - boolean flags and 'm_buffer' is protected directly
     *   by holding mutex lock.
     * - 'm_out_buffer' is protected by setting 'm_sending==true'
     *   as a signal to other threads to keep away. 'm_sending'
     *   itself is protected by 'm_mutex', but we don't have to
     *   keep that mutex lock after 'm_sending' has been granted.
     *   This locking mechanism is implemented by try_lock_send()
     *   and unlock_send().
     *
     * Thus, appending buffers to m_buffer are allowed without
     * being blocked by another thread sending from m_out_buffers.
     */
    NdbMutex m_mutex;

    bool m_sending;     // Send is ongoing, keep away from 'm_out_buffer'
    bool m_reset;       // Reset pending, await 'm_sending' to complete
    bool m_node_active;

    /**
     * A protected view of the current send buffer size of the node.
     * This is to support getSendBufferLevel.
     */
    Uint32 m_current_send_buffer_size;

    /**
     * This is data that have been "scheduled" to be sent
     */
    TFBuffer m_buffer;

    /**
     * This is data that is being sent
     */
    TFBuffer m_out_buffer;

    /**
     *  Implements the 'm_out_buffer' locking as described above.
     */
    bool try_lock_send();
    void unlock_send();
  } m_send_buffers[MAX_NODES];

  void do_send_buffer(Uint32 node, TFSendBuffer *b);

  Uint32 get_current_send_buffer_size(NodeId node)
  {
    return m_send_buffers[node].m_current_send_buffer_size;
  }
  void wakeup_send_thread(void);
  NdbMutex * m_send_thread_mutex;
  NdbCondition * m_send_thread_cond;
  NodeBitmask m_send_thread_nodes;
};

inline
void 
TransporterFacade::lock_poll_mutex()
{
  NdbMutex_Lock(thePollMutex);
}

inline
void 
TransporterFacade::unlock_poll_mutex()
{
  NdbMutex_Unlock(thePollMutex);
}

inline
bool
TransporterFacade::TFSendBuffer::try_lock_send()
{
  //assert(NdbMutex_Trylock(&m_mutex) != 0); //Lock should be held
  if (!m_sending)
  {
    m_sending = true;
    return true;
  }
  return false;
}

inline
void
TransporterFacade::TFSendBuffer::unlock_send()
{
  //assert(NdbMutex_Trylock(&m_mutex) != 0); //Lock should be held
  assert(m_sending);
  m_sending = false;
}


#include "ClusterMgr.hpp"
#include "ndb_cluster_connection_impl.hpp"

inline
bool
TransporterFacade::is_cluster_completely_unavailable()
{
  return theClusterMgr->is_cluster_completely_unavailable();
}

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
        memmove(dest, chunkPtr, wordsToCopy << 2);
        chunkPtr += wordsToCopy;
        chunkRemain -= wordsToCopy;

        dest += wordsToCopy;
        n -= wordsToCopy;
      }
    }
  }
};

class ReceiveThreadClient : public trp_client
{
  public :
  explicit ReceiveThreadClient(TransporterFacade *facade);
  ~ReceiveThreadClient();
  void trp_deliver_signal(const NdbApiSignal *,
                          const LinearSectionPtr ptr[3]);
};
#endif // TransporterFacade_H
