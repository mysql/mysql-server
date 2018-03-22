/*
   Copyright (c) 2003, 2018, Oracle and/or its affiliates. All rights reserved.

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

//****************************************************************************
//
//  NAME
//      TransporterRegistry
//
//  DESCRIPTION
//      TransporterRegistry (singelton) is the interface to the 
//      transporter layer. It handles transporter states and 
//      holds the transporter arrays.
//
//***************************************************************************/
#ifndef TransporterRegistry_H
#define TransporterRegistry_H

#if defined(HAVE_EPOLL_CREATE)
#include <sys/epoll.h>
#endif
#include "TransporterDefinitions.hpp"
#include <SocketServer.hpp>
#include <SocketClient.hpp>

#include <NdbTCP.h>

#include <mgmapi/mgmapi.h>

#include <NodeBitmask.hpp>
#include <NdbMutex.h>

// A transporter is always in an IOState.
// NoHalt is used initially and as long as it is no restrictions on
// sending or receiving.
enum IOState {
  NoHalt     = 0,
  HaltInput  = 1,
  HaltOutput = 2,
  HaltIO     = 3
};


static const char *performStateString[] = 
  { "is connected",
    "is trying to connect",
    "does nothing",
    "is trying to disconnect" };

class Transporter;
class TCP_Transporter;
class SHM_Transporter;

class TransporterRegistry;
class SocketAuthenticator;

class TransporterService : public SocketServer::Service {
  SocketAuthenticator * m_auth;
  TransporterRegistry * m_transporter_registry;
public:
  TransporterService(SocketAuthenticator *auth= 0)
  {
    m_auth= auth;
    m_transporter_registry= 0;
  }
  void setTransporterRegistry(TransporterRegistry *t)
  {
    m_transporter_registry= t;
  }
  SocketServer::Session * newSession(NDB_SOCKET_TYPE socket);
};

/**
 * TransporterReceiveData
 *
 *   State for pollReceive/performReceive
 *   Moved into own class to enable multiple receive threads
 */
struct TransporterReceiveData
{
  TransporterReceiveData();
  ~TransporterReceiveData();

  bool init (unsigned maxTransporters);

  /**
   * Add a transporter to epoll_set
   *   does nothing if epoll not active
   */
  bool epoll_add(Transporter*);

  /**
   * Bitmask of transporters currently handled by this instance
   */
  NodeBitmask m_transporters;

  /**
   * Bitmask of transporters having data awaiting to be received
   * from its transporter.
   */
  NodeBitmask m_recv_transporters;

  /**
   * Bitmask of transporters that has already received data buffered
   * inside its transporter. Possibly "carried over" from last 
   * performReceive
   */
  NodeBitmask m_has_data_transporters;

  /**
   * Subset of m_has_data_transporters which we completed handling
   * of in previous ::performReceive before we was interrupted due
   * to lack of job buffers. Will skip these when we later retry 
   * ::performReceive in order to avoid starvation of non-handled
   * transporters.
   */
  NodeBitmask m_handled_transporters;

  /**
   * Bitmask of transporters having received corrupted or unsupported
   * message. No more unpacking and delivery of messages allowed.
   */
  NodeBitmask m_bad_data_transporters;

  /**
   * Last node received from if unable to complete all transporters
   * in previous ::performReceive(). Next ::performReceive will
   * resume from first transporter after this.
   */
  Uint32 m_last_nodeId;

  /**
   * Spintime calculated as maximum of currently connected transporters.
   * Only applies to shared memory transporters.
   */
  Uint32 m_spintime;

  /**
   * Total spintime
   */
  Uint32 m_total_spintime;

#if defined(HAVE_EPOLL_CREATE)
  int m_epoll_fd;
  struct epoll_event *m_epoll_events;
  bool change_epoll(TCP_Transporter *t, bool add);
#endif

  /**
   * Used in polling if exists TCP_Transporter
   */
  ndb_socket_poller m_socket_poller;
};

#include "TransporterCallback.hpp"

/**
 * @class TransporterRegistry
 * @brief ...
 */
class TransporterRegistry 
{
  friend class SHM_Transporter;
  friend class SHM_Writer;
  friend class Transporter;
  friend class TransporterService;
public:
 /**
  * Constructor
  */
  TransporterRegistry(TransporterCallback *callback,
                      TransporterReceiveHandle * receiveHandle,
		      unsigned maxTransporters = MAX_NTRANSPORTERS);

  /**
   * this handle will be used in the client connect thread
   * to fetch information on dynamic ports.  The old handle
   * (if set) is destroyed, and this is destroyed by the destructor
   */
  void set_mgm_handle(NdbMgmHandle h);
  NdbMgmHandle get_mgm_handle(void) { return m_mgm_handle; };

  bool init(NodeId localNodeId);

  /**
   * Iff using non-default TransporterReceiveHandle's
   *   they need to get initalized
   */
  bool init(TransporterReceiveHandle&);

  /**
     Perform handshaking of a client connection to accept it
     as transporter.

     @note Connection should be closed by caller if function
     returns false

     @param sockfd           the socket to handshake
     @param mgs              error message describing why handshake failed,
                             to be filled in when function return
     @param close_with_reset allows the function to indicate to the caller
                             how the socket should be closed when function
                             returns false

     @returns false on failure and true on success
  */
  bool connect_server(NDB_SOCKET_TYPE sockfd,
                      BaseString& msg,
                      bool& close_with_reset) const;

  bool connect_client(NdbMgmHandle *h);

  /**
   * Given a SocketClient, creates a NdbMgmHandle, turns it into a transporter
   * and returns the socket.
   */
  NDB_SOCKET_TYPE connect_ndb_mgmd(const char* server_name,
                                   unsigned short server_port);

  /**
   * Given a connected NdbMgmHandle, turns it into a transporter
   * and returns the socket.
   */
  NDB_SOCKET_TYPE connect_ndb_mgmd(NdbMgmHandle *h);

private:

  /**
   * Report the dynamically allocated ports to ndb_mgmd so that clients
   * which want to connect to ndbd can ask ndb_mgmd which port to use.
   */
  bool report_dynamic_ports(NdbMgmHandle h) const;

  /**
   * Remove all transporters
   */
  void removeAll();
  
  /**
   * Disconnect all transporters
   */
  void disconnectAll();

  /**
   * Reset awake state on shared memory transporters before sleep.
   */
  int reset_shm_awake_state(TransporterReceiveHandle& recvdata,
                            bool& sleep_state_set);

  /**
   * Set awake state on shared memory transporters after sleep.
   */
  void set_shm_awake_state(TransporterReceiveHandle& recvdata);

public:

  /**
   * Stops the server, disconnects all the transporter 
   * and deletes them and remove it from the transporter arrays
   */
  virtual ~TransporterRegistry();

  bool start_service(SocketServer& server);
  struct NdbThread* start_clients();
  bool stop_clients();
  void start_clients_thread();

  /**
   * Start/Stop receiving
   */
  void startReceiving();
  void stopReceiving();
  
  /**
   * Start/Stop sending
   */
  void startSending();
  void stopSending();

  // A transporter is always in a PerformState.
  // PerformIO is used initially and as long as any of the events 
  // PerformConnect, ... 
  enum PerformState {
    CONNECTED         = 0,
    CONNECTING        = 1,
    DISCONNECTED      = 2,
    DISCONNECTING     = 3
  };
  const char *getPerformStateString(NodeId nodeId) const
  { return performStateString[(unsigned)performStates[nodeId]]; };

  PerformState getPerformState(NodeId nodeId) const { return performStates[nodeId]; }

  /**
   * Get and set methods for PerformState
   */
  void do_connect(NodeId node_id);
  void do_disconnect(NodeId node_id, int errnum = 0);
  bool is_connected(NodeId node_id) const {
    return performStates[node_id] == CONNECTED;
  };
private:
  void report_connect(TransporterReceiveHandle&, NodeId node_id);
  void report_disconnect(TransporterReceiveHandle&, NodeId node_id, int errnum);
  void report_error(NodeId nodeId, TransporterError errorCode,
                    const char *errorInfo = 0);
  void dump_and_report_bad_message(const char file[], unsigned line,
                    TransporterReceiveHandle & recvHandle,
                    Uint32 * readPtr,
                    size_t sizeOfData,
                    NodeId remoteNodeId,
                    IOState state,
                    TransporterError errorCode);
public:
  
  /**
   * Get and set methods for IOState
   */
  IOState ioState(NodeId nodeId) const;
  void setIOState(NodeId nodeId, IOState state);

  /**
   * Methods to handle backoff of connection attempts when attempt fails
   */
public:
  void indicate_node_up(NodeId nodeId);
  void set_connect_backoff_max_time_in_ms(Uint32 max_time_in_ms);
private:
  Uint32 get_connect_backoff_max_time_in_laps() const;
  bool get_and_clear_node_up_indicator(NodeId nodeId);
  void backoff_reset_connecting_time(NodeId nodeId);
  bool backoff_update_and_check_time_for_connect(NodeId nodeId);

private:

  bool createTCPTransporter(TransporterConfiguration * config);
  bool createSHMTransporter(TransporterConfiguration * config);

public:
  /**
   *   configureTransporter
   *
   *   Configure a transporter, ie. create new if it
   *   does not exist otherwise try to reconfigure it
   *
   */
  bool configureTransporter(TransporterConfiguration * config);

  /**
   * Get sum of max send buffer over all transporters, to be used as a default
   * for allocate_send_buffers eg.
   *
   * Must be called after creating all transporters for returned value to be
   * correct.
   */
  Uint64 get_total_max_send_buffer() {
    DBUG_ASSERT(m_total_max_send_buffer > 0);
    return m_total_max_send_buffer;
  } 

  /**
   * Get transporter's connect count
   */
  Uint32 get_connect_count(Uint32 nodeId);

  /**
   * Set or clear overloaded bit.
   * Query if any overloaded bit is set.
   */
  void set_status_overloaded(Uint32 nodeId, bool val);
  const NodeBitmask& get_status_overloaded() const;
  
  /**
   * Get transporter's overload count since connect
   */
  Uint32 get_overload_count(Uint32 nodeId);

  /**
   * Set or clear slowdown bit.
   * Query if any slowdown bit is set.
   */
  void set_status_slowdown(Uint32 nodeId, bool val);
  const NodeBitmask& get_status_slowdown() const;
 
  /** 
   * Get transporter's slowdown count since connect
   */
  Uint32 get_slowdown_count(Uint32 nodeId);

  /**
   * prepareSend
   *
   * When IOState is HaltOutput or HaltIO do not send or insert any 
   * signals in the SendBuffer, unless it is intended for the remote 
   * QMGR block (blockno 252)
   * Perform prepareSend on the transporter. 
   *
   * NOTE signalHeader->xxxBlockRef should contain block numbers and 
   *                                not references
   */

private:
  template <typename AnySectionArg>
  SendStatus prepareSendTemplate(
                         TransporterSendBufferHandle *sendHandle,
                         const SignalHeader *signalHeader,
                         Uint8 prio,
                         const Uint32 *signalData,
                         NodeId nodeId,
                         AnySectionArg section);


public:
  SendStatus prepareSend(TransporterSendBufferHandle *sendHandle,
                         const SignalHeader *signalHeader,
                         Uint8 prio,
                         const Uint32 *signalData,
                         NodeId nodeId,
                         const LinearSectionPtr ptr[3]);

  SendStatus prepareSend(TransporterSendBufferHandle *sendHandle,
                         const SignalHeader *signalHeader,
                         Uint8 prio,
                         const Uint32 *signalData,
                         NodeId nodeId,
                         class SectionSegmentPool & pool,
                         const SegmentedSectionPtr ptr[3]);

  SendStatus prepareSend(TransporterSendBufferHandle *sendHandle,
                         const SignalHeader *signalHeader,
                         Uint8 prio,
                         const Uint32 *signalData,
                         NodeId nodeId,
                         const GenericSectionPtr ptr[3]);
  
  /**
   * external_IO
   *
   * Equal to: poll(...); perform_IO()
   *
   */
  void external_IO(Uint32 timeOutMillis);

  bool performSend(NodeId nodeId, bool need_wakeup = true);
  void performSend();
  
#ifdef DEBUG_TRANSPORTER
  void printState();
#endif

  class Transporter_interface {
  public:
    NodeId m_remote_nodeId;
    int m_s_service_port;			// signed port number
    const char *m_interface;
  };
  Vector<Transporter_interface> m_transporter_interface;
  void add_transporter_interface(NodeId remoteNodeId, const char *interf,
		  		 int s_port);	// signed port. <0 is dynamic

  int get_transporter_count() const;
  Transporter* get_transporter(NodeId nodeId) const;
  bool is_shm_transporter(NodeId nodeId);
  struct in_addr get_connect_address(NodeId node_id) const;

  Uint64 get_bytes_sent(NodeId nodeId) const;
  Uint64 get_bytes_received(NodeId nodeId) const;
  
private:
  TransporterCallback *const callbackObj;
  TransporterReceiveHandle *const receiveHandle;

  NdbMgmHandle m_mgm_handle;

  struct NdbThread   *m_start_clients_thread;
  bool                m_run_start_clients_thread;

  int sendCounter;
  NodeId localNodeId;
  unsigned maxTransporters;
  Uint32 nTransporters;
  Uint32 nTCPTransporters;
  Uint32 nSHMTransporters;

#ifdef ERROR_INSERT
  Bitmask<MAX_NTRANSPORTERS/32> m_blocked;
  Bitmask<MAX_NTRANSPORTERS/32> m_blocked_disconnected;
  int m_disconnect_errors[MAX_NTRANSPORTERS];

  Bitmask<MAX_NTRANSPORTERS/32> m_sendBlocked;

  Uint32 m_mixology_level;
#endif

  /**
   * Arrays holding all transporters in the order they are created
   */
  Transporter**     allTransporters;
  TCP_Transporter** theTCPTransporters;
  SHM_Transporter** theSHMTransporters;
  
  /**
   * Array, indexed by nodeId, holding all transporters
   */
  TransporterType* theTransporterTypes;
  Transporter**    theTransporters;

  /** 
   * State arrays, index by host id
   */
  PerformState* performStates;
  int*          m_disconnect_errnum;
  IOState*      ioStates;
  struct ErrorState {
    TransporterError m_code;
    const char *m_info;
  };
  struct ErrorState *m_error_states;

  /**
   * peerUpIndicators[nodeId] is set by receiver thread
   * to indicate that node is probable up.
   * It is read and cleared by start clients thread.
   */
  volatile bool* peerUpIndicators;

  /**
   * Count of how long time one have been attempting to
   * connect to node nodeId, in units of 100ms.
   */
  Uint32*       connectingTime;

  /**
   * The current maximal time between connection attempts to a
   * node in units of 100ms.
   * Updated by receive thread, read by start clients thread
   */
  volatile Uint32 connectBackoffMaxTime;

  /**
   * Overloaded bits, for fast check.
   * Similarly slowdown bits for fast check.
   */
  NodeBitmask m_status_overloaded;
  NodeBitmask m_status_slowdown;

  /**
   * Unpack signal data.
   *
   * Defined in Packer.cpp.
   */
  Uint32 unpack(TransporterReceiveHandle&,
                Uint32 * readPtr,
                Uint32 bufferSize,
                NodeId remoteNodeId,
                IOState state,
		bool & stopReceiving);

  Uint32 * unpack(TransporterReceiveHandle&,
                  Uint32 * readPtr,
                  Uint32 * eodPtr,
                  Uint32 * endPtr,
                  NodeId remoteNodeId,
                  IOState state,
		  bool & stopReceiving);

  static Uint32 unpack_length_words(const Uint32 *readPtr,
                                    Uint32 maxWords,
                                    bool extra_signal);
  /** 
   * Disconnect the transporter and remove it from 
   * theTransporters array. Do not allow any holes 
   * in theTransporters. Delete the transporter 
   * and remove it from theIndexedTransporters array
   */
  void removeTransporter(NodeId nodeId);

  Uint32 poll_TCP(Uint32 timeOutMillis, TransporterReceiveHandle&);
  Uint32 poll_SHM(TransporterReceiveHandle&, bool &any_connected);
  Uint32 poll_SHM(TransporterReceiveHandle&,
                  NDB_TICKS start_time,
                  Uint32 micros_to_poll);
  Uint32 check_TCP(TransporterReceiveHandle&, Uint32 timeoutMillis);
  Uint32 spin_check_transporters(TransporterReceiveHandle&);

  int m_shm_own_pid;
  Uint32 m_transp_count;

public:
  bool setup_wakeup_socket(TransporterReceiveHandle&);
  void wakeup();

  inline bool setup_wakeup_socket() {
    assert(receiveHandle != 0);
    return setup_wakeup_socket(* receiveHandle);
  }
private:
  bool m_has_extra_wakeup_socket;
  NDB_SOCKET_TYPE m_extra_wakeup_sockets[2];
  void consume_extra_sockets();

  Uint32 *getWritePtr(TransporterSendBufferHandle *handle,
                      NodeId node, Uint32 lenBytes, Uint32 prio);
  void updateWritePtr(TransporterSendBufferHandle *handle,
                      NodeId node, Uint32 lenBytes, Uint32 prio);

public:
  /* Various internal */
  void inc_overload_count(Uint32 nodeId);
  void inc_slowdown_count(Uint32 nodeId);

private:
  /**
   * Sum of max transporter memory for each transporter.
   * Used to compute default send buffer size.
   */
  Uint64 m_total_max_send_buffer;

public:
  /**
   * Receiving
   */
  Uint32 pollReceive(Uint32 timeOutMillis, TransporterReceiveHandle& mask);
  Uint32 performReceive(TransporterReceiveHandle&);
  void update_connections(TransporterReceiveHandle&);

  inline Uint32 pollReceive(Uint32 timeOutMillis) {
    assert(receiveHandle != 0);
    return pollReceive(timeOutMillis, * receiveHandle);
  }

  inline Uint32 performReceive() {
    assert(receiveHandle != 0);
    return performReceive(* receiveHandle);
  }

  inline void update_connections() {
    assert(receiveHandle != 0);
    update_connections(* receiveHandle);
  }
  inline Uint32 get_total_spintime()
  {
    assert(receiveHandle != 0);
    return receiveHandle->m_total_spintime;
  }
  inline void reset_total_spintime()
  {
    assert(receiveHandle != 0);
    receiveHandle->m_total_spintime = 0;
  }

#ifdef ERROR_INSERT
  /* Utils for testing latency issues */
  bool isBlocked(NodeId nodeId);
  void blockReceive(TransporterReceiveHandle&, NodeId nodeId);
  void unblockReceive(TransporterReceiveHandle&, NodeId nodeId);
  bool isSendBlocked(NodeId nodeId) const;
  void blockSend(TransporterReceiveHandle& recvdata,
                 NodeId nodeId);
  void unblockSend(TransporterReceiveHandle& recvdata,
                   NodeId nodeId);

  /* Testing interleaving of signal processing */
  Uint32 getMixologyLevel() const;
  void setMixologyLevel(Uint32 l);
#endif
};

inline void
TransporterRegistry::set_status_overloaded(Uint32 nodeId, bool val)
{
  assert(nodeId < MAX_NODES);
  if (val != m_status_overloaded.get(nodeId))
  {
    m_status_overloaded.set(nodeId, val);
    if (val)
      inc_overload_count(nodeId);
  }
  if (val)
    set_status_slowdown(nodeId, val);
}

inline const NodeBitmask&
TransporterRegistry::get_status_overloaded() const
{
  return m_status_overloaded;
}

inline void
TransporterRegistry::set_status_slowdown(Uint32 nodeId, bool val)
{
  assert(nodeId < MAX_NODES);
  if (val != m_status_slowdown.get(nodeId))
  {
    m_status_slowdown.set(nodeId, val);
    if (val)
      inc_slowdown_count(nodeId);
  }
}

inline const NodeBitmask&
TransporterRegistry::get_status_slowdown() const
{
  return m_status_slowdown;
}

inline void
TransporterRegistry::indicate_node_up(NodeId nodeId) // Called from receive thread
{
  assert(nodeId < MAX_NODES);

  if (!peerUpIndicators[nodeId])
  {
    peerUpIndicators[nodeId] = true;
  }
}

inline bool
TransporterRegistry::get_and_clear_node_up_indicator(NodeId nodeId) // Called from start client thread
{
  assert(nodeId < MAX_NODES);

  bool indicator = peerUpIndicators[nodeId];
  if (indicator)
  {
    peerUpIndicators[nodeId] = false;
  }
  return indicator;
}

inline Uint32
TransporterRegistry::get_connect_backoff_max_time_in_laps() const
{ /* one lap, 100 ms */
  return connectBackoffMaxTime;
}

inline void
TransporterRegistry::set_connect_backoff_max_time_in_ms(Uint32 backoff_max_time_in_ms)
{
  /**
   * Round up backoff_max_time to nearest higher 100ms, since that is lap time
   * in start_client_threads using this function.
   */
  connectBackoffMaxTime = (backoff_max_time_in_ms + 99) / 100;
}

inline void
TransporterRegistry::backoff_reset_connecting_time(NodeId nodeId)
{
  assert(nodeId < MAX_NODES);

  connectingTime[nodeId] = 0;
}

inline bool
TransporterRegistry::backoff_update_and_check_time_for_connect(NodeId nodeId)
{
  assert(nodeId < MAX_NODES);

  Uint32 backoff_max_time = get_connect_backoff_max_time_in_laps();

  if (backoff_max_time == 0)
  {
    // Backoff disabled
    return true;
  }

  connectingTime[nodeId] ++;

  if (connectingTime[nodeId] >= backoff_max_time)
  {
    return (connectingTime[nodeId] % backoff_max_time == 0);
  }

  /**
   * Attempt moments from start of connecting.
   * This function is called from start_clients_thread
   * roughly every 100ms for each node it is connecting
   * to.
   */
  static const Uint16 attempt_moments[] = {1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024};
  static const int attempt_moments_count = sizeof(attempt_moments) / sizeof(attempt_moments[0]);
  for(int i = 0; i < attempt_moments_count; i ++)
  {
    if (connectingTime[nodeId] == attempt_moments[i])
    {
      return true;
    }
    else if (connectingTime[nodeId] < attempt_moments[i])
    {
      return false;
    }
  }
  return (connectingTime[nodeId] % attempt_moments[attempt_moments_count - 1] == 0);
}

/**
 * A function used to calculate a send buffer level given the size of the node
 * send buffer and the total send buffer size for all nodes and the total send
 * buffer used for all nodes. There is also a thread parameter that specifies
 * the number of threads used (this is 0 except for ndbmtd).
 */
void calculate_send_buffer_level(Uint64 node_send_buffer_size,
                                 Uint64 total_send_buffer_size,
                                 Uint64 total_used_send_buffer_size,
                                 Uint32 num_threads,
                                 SB_LevelType &level);
#endif // Define of TransporterRegistry_H
