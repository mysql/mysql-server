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

#include "TransporterDefinitions.hpp"
#include <SocketServer.hpp>
#include <SocketClient.hpp>

#include <NdbTCP.h>

#include <mgmapi/mgmapi.h>

// A transporter is always in an IOState.
// NoHalt is used initially and as long as it is no restrictions on
// sending or receiving.
enum IOState {
  NoHalt     = 0,
  HaltInput  = 1,
  HaltOutput = 2,
  HaltIO     = 3
};

enum TransporterType {
  tt_TCP_TRANSPORTER = 1,
  tt_SCI_TRANSPORTER = 2,
  tt_SHM_TRANSPORTER = 3
  // ID 4 was OSE Transporter which has been removed. Don't use ID 4.
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
 * @class TransporterRegistry
 * @brief ...
 */
class TransporterRegistry {
  friend class SHM_Transporter;
  friend class Transporter;
  friend class TransporterService;
public:
 /**
  * Constructor
  */
  TransporterRegistry(void * callback = 0 , 
		      unsigned maxTransporters = MAX_NTRANSPORTERS, 
		      unsigned sizeOfLongSignalMemory = 100);

  /**
   * this handle will be used in the client connect thread
   * to fetch information on dynamic ports.  The old handle
   * (if set) is destroyed, and this is destroyed by the destructor
   */
  void set_mgm_handle(NdbMgmHandle h);
  NdbMgmHandle get_mgm_handle(void) { return m_mgm_handle; };

  bool init(NodeId localNodeId);

  /**
   * after a connect from client, perform connection using correct transporter
   */
  bool connect_server(NDB_SOCKET_TYPE sockfd);

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
  ~TransporterRegistry();

  bool start_service(SocketServer& server);
  bool start_clients();
  bool stop_clients();
  void start_clients_thread();
  void update_connections();

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

  /**
   * Get and set methods for PerformState
   */
  void do_connect(NodeId node_id);
  void do_disconnect(NodeId node_id);
  bool is_connected(NodeId node_id) { return performStates[node_id] == CONNECTED; };
  void report_connect(NodeId node_id);
  void report_disconnect(NodeId node_id, int errnum);
  
  /**
   * Get and set methods for IOState
   */
  IOState ioState(NodeId nodeId);
  void setIOState(NodeId nodeId, IOState state);

  /** 
   * createTransporter
   *
   * If the config object indicates that the transporter
   * to be created will act as a server and no server is
   * started, startServer is called. A transporter of the selected kind
   * is created and it is put in the transporter arrays.
   */
  bool createTCPTransporter(struct TransporterConfiguration * config);
  bool createSCITransporter(struct TransporterConfiguration * config);
  bool createSHMTransporter(struct TransporterConfiguration * config);

  /**
   * Get free buffer space
   *
   *   Get #free bytes in send buffer for <em>node</node>
   */
  Uint32 get_free_buffer(Uint32 node) const ;
  
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
  SendStatus prepareSend(const SignalHeader * const signalHeader, Uint8 prio,
			 const Uint32 * const signalData,
			 NodeId nodeId, 
			 const LinearSectionPtr ptr[3]);

  SendStatus prepareSend(const SignalHeader * const signalHeader, Uint8 prio,
			 const Uint32 * const signalData,
			 NodeId nodeId, 
			 class SectionSegmentPool & pool,
			 const SegmentedSectionPtr ptr[3]);
  
  /**
   * external_IO
   *
   * Equal to: poll(...); perform_IO()
   *
   */
  void external_IO(Uint32 timeOutMillis);
  
  Uint32 pollReceive(Uint32 timeOutMillis);
  void performReceive();
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
  NodeId get_localNodeId() { return localNodeId; };


  struct in_addr get_connect_address(NodeId node_id) const;
protected:
  
private:
  void * callbackObj;

  NdbMgmHandle m_mgm_handle;

  struct NdbThread   *m_start_clients_thread;
  bool                m_run_start_clients_thread;

  int sendCounter;
  NodeId localNodeId;
  bool nodeIdSpecified;
  unsigned maxTransporters;
  int nTransporters;
  int nTCPTransporters;
  int nSCITransporters;
  int nSHMTransporters;

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
  IOState*      ioStates;
 
  /**
   * Unpack signal data
   */
  Uint32 unpack(Uint32 * readPtr,
		Uint32 bufferSize,
		NodeId remoteNodeId, 
		IOState state);
  
  Uint32 * unpack(Uint32 * readPtr,
		  Uint32 * eodPtr,
		  NodeId remoteNodeId,
		  IOState state);

  /** 
   * Disconnect the transporter and remove it from 
   * theTransporters array. Do not allow any holes 
   * in theTransporters. Delete the transporter 
   * and remove it from theIndexedTransporters array
   */
  void removeTransporter(NodeId nodeId);
  
  /**
   * Used in polling if exists TCP_Transporter
   */
  int tcpReadSelectReply;
  fd_set tcpReadset;
  
  Uint32 poll_TCP(Uint32 timeOutMillis);
  Uint32 poll_SCI(Uint32 timeOutMillis);
  Uint32 poll_SHM(Uint32 timeOutMillis);

  int m_shm_own_pid;
  int m_transp_count;
};

#endif // Define of TransporterRegistry_H
