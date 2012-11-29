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
class SCI_Transporter;
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
 *   Moved into own class to enable multi receive threads
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
  bool epoll_add(TCP_Transporter*);

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
class TransporterRegistry : private TransporterSendBufferHandle {
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
                      bool use_default_send_buffer = true,
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
     Handle the handshaking with a new client connection
     on the server port.
     NOTE! Connection should be closed if function
     returns false
  */
  bool connect_server(NDB_SOCKET_TYPE sockfd, BaseString& errormsg) const;

  bool connect_client(NdbMgmHandle *h);

  /**
   * Given a SocketClient, creates a NdbMgmHandle, turns it into a transporter
   * and returns the socket.
   */
  NDB_SOCKET_TYPE connect_ndb_mgmd(SocketClient *sc);

  /**
   * Given a connected NdbMgmHandle, turns it into a transporter
   * and returns the socket.
   */
  NDB_SOCKET_TYPE connect_ndb_mgmd(NdbMgmHandle *h);

  /**
   * Remove all transporters
   */
  void removeAll();
  
  /**
   * Disconnect all transporters
   */
  void disconnectAll();

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
  bool is_connected(NodeId node_id) { return performStates[node_id] == CONNECTED; };
  void report_connect(TransporterReceiveHandle&, NodeId node_id);
  void report_disconnect(TransporterReceiveHandle&, NodeId node_id, int errnum);
  void report_error(NodeId nodeId, TransporterError errorCode,
                    const char *errorInfo = 0);
  
  /**
   * Get and set methods for IOState
   */
  IOState ioState(NodeId nodeId);
  void setIOState(NodeId nodeId, IOState state);

private:

  bool createTCPTransporter(TransporterConfiguration * config);
  bool createSCITransporter(TransporterConfiguration * config);
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
   * Allocate send buffer for default send buffer handling.
   *
   * Upper layer that implements their own TransporterSendBufferHandle do not
   * use this, instead they manage their own send buffers.
   *
   * Argument is the value of config parameter TotalSendBufferMemory. If 0,
   * a default will be used of sum(max send buffer) over all transporters.
   * The second is the config parameter ExtraSendBufferMemory
   */
  void allocate_send_buffers(Uint64 total_send_buffer,
                             Uint64 extra_send_buffer);

  /**
   * Get sum of max send buffer over all transporters, to be used as a default
   * for allocate_send_buffers eg.
   *
   * Must be called after creating all transporters for returned value to be
   * correct.
   */
  Uint64 get_total_max_send_buffer() { return m_total_max_send_buffer; }

  bool get_using_default_send_buffer() const{ return m_use_default_send_buffer;}

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
   * CMVMI block (blockno 252)
   * Perform prepareSend on the transporter. 
   *
   * NOTE signalHeader->xxxBlockRef should contain block numbers and 
   *                                not references
   */
  SendStatus prepareSend(TransporterSendBufferHandle *sendHandle,
                         const SignalHeader * const signalHeader, Uint8 prio,
			 const Uint32 * const signalData,
			 NodeId nodeId, 
			 const LinearSectionPtr ptr[3]);

  SendStatus prepareSend(TransporterSendBufferHandle *sendHandle,
                         const SignalHeader * const signalHeader, Uint8 prio,
			 const Uint32 * const signalData,
			 NodeId nodeId, 
			 class SectionSegmentPool & pool,
			 const SegmentedSectionPtr ptr[3]);
  SendStatus prepareSend(TransporterSendBufferHandle *sendHandle,
                         const SignalHeader * const signalHeader, Uint8 prio,
                         const Uint32 * const signalData,
                         NodeId nodeId,
                         const GenericSectionPtr ptr[3]);
  /**
   * Backwards compatiple methods with default send buffer handling.
   */
  SendStatus prepareSend(const SignalHeader * const signalHeader, Uint8 prio,
			 const Uint32 * const signalData,
			 NodeId nodeId,
			 const LinearSectionPtr ptr[3])
  {
    return prepareSend(this, signalHeader, prio, signalData, nodeId, ptr);
  }
  SendStatus prepareSend(const SignalHeader * const signalHeader, Uint8 prio,
			 const Uint32 * const signalData,
			 NodeId nodeId,
			 class SectionSegmentPool & pool,
			 const SegmentedSectionPtr ptr[3])
  {
    return prepareSend(this, signalHeader, prio, signalData, nodeId, pool, ptr);
  }
  SendStatus prepareSend(const SignalHeader * const signalHeader, Uint8 prio,
                         const Uint32 * const signalData,
                         NodeId nodeId,
                         const GenericSectionPtr ptr[3])
  {
    return prepareSend(this, signalHeader, prio, signalData, nodeId, ptr);
  }
  
  /**
   * external_IO
   *
   * Equal to: poll(...); perform_IO()
   *
   */
  void external_IO(Uint32 timeOutMillis);

  int performSend(NodeId nodeId);
  void performSend();

  /**
   * Force sending if more than or equal to sendLimit
   * number have asked for send. Returns 0 if not sending
   * and 1 if sending.
   */
  int forceSendCheck(int sendLimit);
  
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
  Transporter* get_transporter(NodeId nodeId);
  struct in_addr get_connect_address(NodeId node_id) const;

  Uint64 get_bytes_sent(NodeId nodeId) const;
  Uint64 get_bytes_received(NodeId nodeId) const;
protected:
  
private:
  TransporterCallback *callbackObj;
  TransporterReceiveHandle * receiveHandle;

  NdbMgmHandle m_mgm_handle;

  struct NdbThread   *m_start_clients_thread;
  bool                m_run_start_clients_thread;

  int sendCounter;
  NodeId localNodeId;
  unsigned maxTransporters;
  int nTransporters;
  int nTCPTransporters;
  int nSCITransporters;
  int nSHMTransporters;

#ifdef ERROR_INSERT
  Bitmask<MAX_NTRANSPORTERS/32> m_blocked;
  Bitmask<MAX_NTRANSPORTERS/32> m_blocked_disconnected;
  int m_disconnect_errors[MAX_NTRANSPORTERS];
#endif

  /**
   * Arrays holding all transporters in the order they are created
   */
  TCP_Transporter** theTCPTransporters;
  SCI_Transporter** theSCITransporters;
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
                IOState state);

  Uint32 * unpack(TransporterReceiveHandle&,
                  Uint32 * readPtr,
                  Uint32 * eodPtr,
                  NodeId remoteNodeId,
                  IOState state);

  static Uint32 unpack_length_words(const Uint32 *readPtr, Uint32 maxWords);
  /** 
   * Disconnect the transporter and remove it from 
   * theTransporters array. Do not allow any holes 
   * in theTransporters. Delete the transporter 
   * and remove it from theIndexedTransporters array
   */
  void removeTransporter(NodeId nodeId);

  Uint32 poll_TCP(Uint32 timeOutMillis, TransporterReceiveHandle&);
  Uint32 poll_SCI(Uint32 timeOutMillis, TransporterReceiveHandle&);
  Uint32 poll_SHM(Uint32 timeOutMillis, TransporterReceiveHandle&);

  int m_shm_own_pid;
  int m_transp_count;

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

  /**
   * TransporterSendBufferHandle implementation.
   *
   * Used for default send buffer handling, when the upper layer does not
   * want to do special buffer handling itself.
   */
  virtual Uint32 *getWritePtr(NodeId node, Uint32 lenBytes, Uint32 prio,
                              Uint32 max_use);
  virtual Uint32 updateWritePtr(NodeId node, Uint32 lenBytes, Uint32 prio);
  virtual bool forceSend(NodeId node);


  /* Various internal */
  void inc_overload_count(Uint32 nodeId);
  void inc_slowdown_count(Uint32 nodeId);
private:
  /* Send buffer pages. */
  struct SendBufferPage {
    /* This is the number of words that will fit in one page of send buffer. */
    static const Uint32 PGSIZE = 32768;
    static Uint32 max_data_bytes()
    {
      return PGSIZE - offsetof(SendBufferPage, m_data);
    }

    /* Send buffer for one transporter is kept in a single-linked list. */
    struct SendBufferPage *m_next;

    /* Bytes of send data available in this page. */
    Uint16 m_bytes;
    /* Start of unsent data */
    Uint16 m_start;

    /* Data; real size is to the end of one page. */
    char m_data[2];
  };

  /* Send buffer for one transporter. */
  struct SendBuffer {
    /* Total size of data in buffer, from m_offset_start_data to end. */
    Uint32 m_used_bytes;
    /* Linked list of active buffer pages with first and last pointer. */
    SendBufferPage *m_first_page;
    SendBufferPage *m_last_page;
  };

  SendBufferPage *alloc_page();
  void release_page(SendBufferPage *page);

private:
  /* True if we are using the default send buffer implementation. */
  bool m_use_default_send_buffer;
  /* Send buffers. */
  SendBuffer *m_send_buffers;
  /* Linked list of free pages. */
  SendBufferPage *m_page_freelist;
  /* Original block of memory for pages (so we can free it at exit). */
  unsigned char *m_send_buffer_memory;
  /**
   * Sum of max transporter memory for each transporter.
   * Used to compute default send buffer size.
   */
  Uint64 m_total_max_send_buffer;

public:
  Uint32 get_bytes_to_send_iovec(NodeId node, struct iovec *dst, Uint32 max);
  Uint32 bytes_sent(NodeId node, Uint32 bytes);
  bool has_data_to_send(NodeId node);

  void reset_send_buffer(NodeId node, bool should_be_empty);

  void print_transporters(const char* where, NdbOut& out = ndbout);

  /**
   * Receiving
   */
  Uint32 pollReceive(Uint32 timeOutMillis, TransporterReceiveHandle& mask);
  void performReceive(TransporterReceiveHandle&);
  void update_connections(TransporterReceiveHandle&);

  inline Uint32 pollReceive(Uint32 timeOutMillis) {
    assert(receiveHandle != 0);
    return pollReceive(timeOutMillis, * receiveHandle);
  }

  inline void performReceive() {
    assert(receiveHandle != 0);
    performReceive(* receiveHandle);
  }

  inline void update_connections() {
    assert(receiveHandle != 0);
    update_connections(* receiveHandle);
  }

#ifdef ERROR_INSERT
  /* Utils for testing latency issues */
  bool isBlocked(NodeId nodeId);
  void blockReceive(TransporterReceiveHandle&, NodeId nodeId);
  void unblockReceive(TransporterReceiveHandle&, NodeId nodeId);
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

#endif // Define of TransporterRegistry_H
