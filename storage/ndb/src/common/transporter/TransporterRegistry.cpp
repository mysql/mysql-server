/*
   Copyright (c) 2003, 2019, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <ndb_global.h>

#include <TransporterRegistry.hpp>
#include "TransporterInternalDefinitions.hpp"

#include "Transporter.hpp"
#include <SocketAuthenticator.hpp>
#include "BlockNumbers.h"

#include "TCP_Transporter.hpp"
#include "Loopback_Transporter.hpp"

#ifndef WIN32
#include "SHM_Transporter.hpp"
#endif

#include "NdbOut.hpp"
#include <NdbSleep.h>
#include <NdbMutex.h>
#include <InputStream.hpp>
#include <OutputStream.hpp>
#include <socket_io.h>

#include <mgmapi/mgmapi.h>
#include <mgmapi_internal.h>
#include <mgmapi/mgmapi_debug.h>

#include <EventLogger.hpp>
extern EventLogger * g_eventLogger;

#if 0
#define DEBUG_FPRINTF(arglist) do { fprintf arglist ; } while (0)
#else
#define DEBUG_FPRINTF(a)
#endif

/**
 * There is a requirement in the Transporter design that
 * ::performReceive() and ::update_connections()
 * on the same 'TransporterReceiveHandle' should not be 
 * run concurrently. class TransporterReceiveWatchdog provides a
 * simple mechanism to assert that this rule is followed.
 * Does nothing if NDEBUG is defined (in production code)
 */
class TransporterReceiveWatchdog
{
public:
#ifdef NDEBUG
  TransporterReceiveWatchdog(TransporterReceiveHandle& recvdata)
  {}

#else
  TransporterReceiveWatchdog(TransporterReceiveHandle& recvdata)
    : m_recvdata(recvdata)
  {
    assert(m_recvdata.m_active == false);
    m_recvdata.m_active = true;
  }

  ~TransporterReceiveWatchdog()
  {
    assert(m_recvdata.m_active == true);
    m_recvdata.m_active = false;
  }

private:
  TransporterReceiveHandle& m_recvdata;
#endif
};


struct in_addr
TransporterRegistry::get_connect_address(NodeId node_id) const
{
  return theTransporters[node_id]->m_connect_address;
}

Uint64
TransporterRegistry::get_bytes_sent(NodeId node_id) const
{
  return theTransporters[node_id]->m_bytes_sent;
}

Uint64
TransporterRegistry::get_bytes_received(NodeId node_id) const
{
  return theTransporters[node_id]->m_bytes_received;
}

SocketServer::Session * TransporterService::newSession(NDB_SOCKET_TYPE sockfd)
{
  DBUG_ENTER("SocketServer::Session * TransporterService::newSession");
  if (m_auth && !m_auth->server_authenticate(sockfd))
  {
    ndb_socket_close_with_reset(sockfd, true); // Close with reset
    DBUG_RETURN(0);
  }

  BaseString msg;
  bool close_with_reset = true;
  if (!m_transporter_registry->connect_server(sockfd, msg, close_with_reset))
  {
    ndb_socket_close_with_reset(sockfd, close_with_reset);
    DBUG_RETURN(0);
  }

  DBUG_RETURN(0);
}

TransporterReceiveData::TransporterReceiveData()
  : m_transporters(),
    m_recv_transporters(),
    m_has_data_transporters(),
    m_handled_transporters(),
    m_bad_data_transporters(),
    m_last_nodeId(0)
{
  /**
   * With multi receiver threads
   *   an interface to reassign these is needed...
   */
  m_transporters.set();            // Handle all
  m_transporters.clear(Uint32(0)); // Except wakeup socket...

#if defined(HAVE_EPOLL_CREATE)
  m_epoll_fd = -1;
  m_epoll_events = 0;
#endif
}

bool
TransporterReceiveData::init(unsigned maxTransporters)
{
  maxTransporters += 1; /* wakeup socket */
  m_spintime = 0;
  m_total_spintime = 0;
#if defined(HAVE_EPOLL_CREATE)
  m_epoll_fd = epoll_create(maxTransporters);
  if (m_epoll_fd == -1)
  {
    perror("epoll_create failed... falling back to select!");
    goto fallback;
  }
  m_epoll_events = new struct epoll_event[maxTransporters];
  if (m_epoll_events == 0)
  {
    perror("Failed to alloc epoll-array... falling back to select!");
    close(m_epoll_fd);
    m_epoll_fd = -1;
    goto fallback;
  }
  memset(m_epoll_events, 0, maxTransporters * sizeof(struct epoll_event));
  return true;
fallback:
#endif
  return m_socket_poller.set_max_count(maxTransporters);
}

bool
TransporterReceiveData::epoll_add(Transporter *t)
{
  assert(m_transporters.get(t->getRemoteNodeId()));
#if defined(HAVE_EPOLL_CREATE)
  if (m_epoll_fd != -1)
  {
    bool add = true;
    struct epoll_event event_poll;
    memset(&event_poll, 0, sizeof(event_poll));
    NDB_SOCKET_TYPE sock_fd = t->getSocket();
    int node_id = t->getRemoteNodeId();
    int op = EPOLL_CTL_ADD;
    int ret_val, error;

    if (!ndb_socket_valid(sock_fd))
      return FALSE;

    event_poll.data.u32 = t->getRemoteNodeId();
    event_poll.events = EPOLLIN;
    ret_val = epoll_ctl(m_epoll_fd, op, sock_fd.fd, &event_poll);
    if (!ret_val)
      goto ok;
    error= errno;
    if (error == ENOENT && !add)
    {
      /*
       * Could be that socket was closed premature to this call.
       * Not a problem that this occurs.
       */
      goto ok;
    }
    if (!add || (add && (error != ENOMEM)))
    {
      /*
       * Serious problems, we are either using wrong parameters,
       * have permission problems or the socket doesn't support
       * epoll!!
       */
      ndbout_c("Failed to %s epollfd: %u fd " MY_SOCKET_FORMAT
               " node %u to epoll-set,"
               " errno: %u %s",
               add ? "ADD" : "DEL",
               m_epoll_fd,
               MY_SOCKET_FORMAT_VALUE(sock_fd),
               node_id,
               error,
               strerror(error));
      abort();
    }
    ndbout << "We lacked memory to add the socket for node id ";
    ndbout << node_id << endl;
    return false;
  }

ok:
#endif
  return true;
}

TransporterReceiveData::~TransporterReceiveData()
{
#if defined(HAVE_EPOLL_CREATE)
  if (m_epoll_fd != -1)
  {
    close(m_epoll_fd);
    m_epoll_fd = -1;
  }

  if (m_epoll_events)
  {
    delete [] m_epoll_events;
    m_epoll_events = 0;
  }
#endif
}

TransporterRegistry::TransporterRegistry(TransporterCallback *callback,
                                         TransporterReceiveHandle *recvHandle,
                                         unsigned _maxTransporters) :
  callbackObj(callback),
  receiveHandle(recvHandle),
  m_mgm_handle(0),
  sendCounter(1),
  localNodeId(0),
  maxTransporters(_maxTransporters),
  nTransporters(0),
  nTCPTransporters(0), nSHMTransporters(0),
  connectBackoffMaxTime(0),
  m_transp_count(0),
  m_total_max_send_buffer(0)
{
  DBUG_ENTER("TransporterRegistry::TransporterRegistry");

  allTransporters     = new Transporter*      [maxTransporters];
  theTCPTransporters  = new TCP_Transporter * [maxTransporters];
  theSHMTransporters  = new SHM_Transporter * [maxTransporters];
  theTransporterTypes = new TransporterType   [maxTransporters];
  theTransporters     = new Transporter     * [maxTransporters];
  performStates       = new PerformState      [maxTransporters];
  ioStates            = new IOState           [maxTransporters]; 
  peerUpIndicators    = new bool              [maxTransporters];
  connectingTime      = new Uint32            [maxTransporters];
  m_disconnect_errnum = new int               [maxTransporters];
  m_disconnect_enomem_error = new Uint32      [maxTransporters];
  m_error_states      = new ErrorState        [maxTransporters];

  m_has_extra_wakeup_socket = false;

#ifdef ERROR_INSERT
  m_blocked.clear();
  m_blocked_disconnected.clear();
  m_sendBlocked.clear();

  m_mixology_level = 0;
#endif

  // Initialize the transporter arrays
  ErrorState default_error_state = { TE_NO_ERROR, (const char *)~(UintPtr)0 };
  for (unsigned i=0; i<maxTransporters; i++) {
    allTransporters[i]    = NULL;
    theTCPTransporters[i] = NULL;
    theSHMTransporters[i] = NULL;
    theTransporters[i]    = NULL;
    performStates[i]      = DISCONNECTED;
    ioStates[i]           = NoHalt;
    peerUpIndicators[i]   = true; // Assume all nodes are up, will be
                                  // cleared at first connect attempt
    connectingTime[i]     = 0;
    m_disconnect_errnum[i]= 0;
    m_disconnect_enomem_error[i] = 0;
    m_error_states[i]     = default_error_state;
  }
  DBUG_VOID_RETURN;
}

void TransporterRegistry::set_mgm_handle(NdbMgmHandle h)
{
  DBUG_ENTER("TransporterRegistry::set_mgm_handle");
  if (m_mgm_handle)
    ndb_mgm_destroy_handle(&m_mgm_handle);
  m_mgm_handle= h;
  ndb_mgm_set_timeout(m_mgm_handle, 5000);
#ifndef DBUG_OFF
  if (h)
  {
    char buf[256];
    DBUG_PRINT("info",("handle set with connectstring: %s",
		       ndb_mgm_get_connectstring(h,buf, sizeof(buf))));
  }
  else
  {
    DBUG_PRINT("info",("handle set to NULL"));
  }
#endif
  DBUG_VOID_RETURN;
}

TransporterRegistry::~TransporterRegistry()
{
  DBUG_ENTER("TransporterRegistry::~TransporterRegistry");
 
  disconnectAll(); 
  removeAll();
  
  delete[] allTransporters;
  delete[] theTCPTransporters;
  delete[] theSHMTransporters;
  delete[] theTransporterTypes;
  delete[] theTransporters;
  delete[] performStates;
  delete[] ioStates;
  delete[] peerUpIndicators;
  delete[] connectingTime;
  delete[] m_disconnect_errnum;
  delete[] m_disconnect_enomem_error;
  delete[] m_error_states;

  if (m_mgm_handle)
    ndb_mgm_destroy_handle(&m_mgm_handle);

  if (m_has_extra_wakeup_socket)
  {
    ndb_socket_close(m_extra_wakeup_sockets[0]);
    ndb_socket_close(m_extra_wakeup_sockets[1]);
  }

  DBUG_VOID_RETURN;
}

void
TransporterRegistry::removeAll(){
  for(unsigned i = 0; i<maxTransporters; i++){
    if(theTransporters[i] != NULL)
      removeTransporter(theTransporters[i]->getRemoteNodeId());
  }
}

void
TransporterRegistry::disconnectAll(){
  DEBUG_FPRINTF((stderr, "(%u)doDisconnect(all), line: %d\n",
               localNodeId, __LINE__));
  for(unsigned i = 0; i<maxTransporters; i++){
    if(theTransporters[i] != NULL)
      theTransporters[i]->doDisconnect();
  }
}

bool
TransporterRegistry::init(NodeId nodeId) {
  DBUG_ENTER("TransporterRegistry::init");
  assert(localNodeId == 0 ||
         localNodeId == nodeId);

  localNodeId = nodeId;

  DEBUG("TransporterRegistry started node: " << localNodeId);

  if (receiveHandle)
  {
    if (!init(* receiveHandle))
      DBUG_RETURN(false);
  }

  DBUG_RETURN(true);
}

bool
TransporterRegistry::init(TransporterReceiveHandle& recvhandle)
{
  return recvhandle.init(maxTransporters);
}

bool
TransporterRegistry::connect_server(NDB_SOCKET_TYPE sockfd,
                                    BaseString & msg,
                                    bool& close_with_reset) const
{
  DBUG_ENTER("TransporterRegistry::connect_server(sockfd)");

  // Read "hello" that consists of node id and transporter
  // type from client
  SocketInputStream s_input(sockfd);
  char buf[11+1+11+1]; // <int> <int>
  if (s_input.gets(buf, sizeof(buf)) == 0) {
    msg.assfmt("line: %u : Failed to get nodeid from client", __LINE__);
    DBUG_PRINT("error", ("Failed to read 'hello' from client"));
    DBUG_RETURN(false);
  }

  int nodeId, remote_transporter_type= -1;
  int r= sscanf(buf, "%d %d", &nodeId, &remote_transporter_type);
  switch (r) {
  case 2:
    break;
  case 1:
    // we're running version prior to 4.1.9
    // ok, but with no checks on transporter configuration compatability
    break;
  default:
    msg.assfmt("line: %u : Incorrect reply from client: >%s<", __LINE__, buf);
    DBUG_PRINT("error", ("Failed to parse 'hello' from client, buf: '%.*s'",
                         (int)sizeof(buf), buf));
    DBUG_RETURN(false);
  }

  DBUG_PRINT("info", ("Client hello, nodeId: %d transporter type: %d",
		      nodeId, remote_transporter_type));


  // Check that nodeid is in range before accessing the arrays
  if (nodeId < 0 ||
      nodeId >= (int)maxTransporters)
  {
    msg.assfmt("line: %u : Incorrect reply from client: >%s<", __LINE__, buf);
    DBUG_PRINT("error", ("Out of range nodeId: %d from client",
                         nodeId));
    DBUG_RETURN(false);
  }

  // Check that transporter is allocated
  Transporter *t= theTransporters[nodeId];
  if (t == 0)
  {
    msg.assfmt("line: %u : Incorrect reply from client: >%s<, node: %u",
               __LINE__, buf, nodeId);
    DBUG_PRINT("error", ("No transporter available for node id %d", nodeId));
    DBUG_RETURN(false);
  }

  // Check that the transporter should be connecting
  if (performStates[nodeId] != TransporterRegistry::CONNECTING)
  {
    msg.assfmt("line: %u : Incorrect state for node %u state: %s (%u)",
               __LINE__, nodeId,
               getPerformStateString(nodeId),
               performStates[nodeId]);

    DBUG_PRINT("error", ("Transporter for node id %d in wrong state",
                         nodeId));

    // Avoid TIME_WAIT on server by requesting client to close connection
    SocketOutputStream s_output(sockfd);
    if (s_output.println("BYE") < 0)
    {
      // Failed to request client close
      DBUG_PRINT("error", ("Failed to send client BYE"));
      DBUG_RETURN(false);
    }

    // Wait for to close connection by reading EOF(i.e read returns 0)
    const int read_eof_timeout = 1000; // Fairly short timeout
    if (read_socket(sockfd, read_eof_timeout,
                    buf, sizeof(buf)) == 0)
    {
      // Client gracefully closed connection, turn off close_with_reset
      close_with_reset = false;
      DBUG_RETURN(false);
    }

    // Failed to request client close
    DBUG_RETURN(false);
  }

  // Check transporter type
  if (remote_transporter_type != -1 &&
      remote_transporter_type != t->m_type)
  {
    g_eventLogger->error("Connection from node: %d uses different transporter "
                         "type: %d, expected type: %d",
                         nodeId, remote_transporter_type, t->m_type);
    DBUG_RETURN(false);
  }

  // Send reply to client
  SocketOutputStream s_output(sockfd);
  if (s_output.println("%d %d", t->getLocalNodeId(), t->m_type) < 0)
  {
    msg.assfmt("line: %u : Failed to reply to connecting socket (node: %u)",
               __LINE__, nodeId);
    DBUG_PRINT("error", ("Send of reply failed"));
    DBUG_RETURN(false);
  }

  // Setup transporter (transporter responsible for closing sockfd)
  DBUG_RETURN(t->connect_server(sockfd, msg));
}


bool
TransporterRegistry::configureTransporter(TransporterConfiguration *config)
{
  NodeId remoteNodeId = config->remoteNodeId;

  assert(localNodeId);
  assert(config->localNodeId == localNodeId);

  if (remoteNodeId >= maxTransporters)
    return false;

  Transporter* t = theTransporters[remoteNodeId];
  if(t != NULL)
  {
    // Transporter already exist, try to reconfigure it
    return t->configure(config);
  }

  DEBUG("Configuring transporter from " << localNodeId
	<< " to " << remoteNodeId);

  switch (config->type){
  case tt_TCP_TRANSPORTER:
    return createTCPTransporter(config);
  case tt_SHM_TRANSPORTER:
    return createSHMTransporter(config);
  default:
    abort();
    break;
  }
  return false;
}


bool
TransporterRegistry::createTCPTransporter(TransporterConfiguration *config) {

  TCP_Transporter * t = 0;
  if (config->remoteNodeId == config->localNodeId)
  {
    t = new Loopback_Transporter(* this, config);
  }
  else
  {
    t = new TCP_Transporter(*this, config);
  }

  if (t == NULL) 
    return false;
  else if (!t->initTransporter()) {
    delete t;
    return false;
  }

  // Put the transporter in the transporter arrays
  allTransporters[nTransporters]            = t;
  theTCPTransporters[nTCPTransporters]      = t;
  theTransporters[t->getRemoteNodeId()]     = t;
  theTransporterTypes[t->getRemoteNodeId()] = tt_TCP_TRANSPORTER;
  performStates[t->getRemoteNodeId()]       = DISCONNECTED;
  nTransporters++;
  nTCPTransporters++;
  m_total_max_send_buffer += t->get_max_send_buffer();

  return true;
}

bool
TransporterRegistry::createSHMTransporter(TransporterConfiguration *config)
{
#ifndef WIN32
  DBUG_ENTER("TransporterRegistry::createTransporter SHM");

  SHM_Transporter * t = new SHM_Transporter(*this,
					    config->localHostName,
					    config->remoteHostName,
					    config->s_port,
					    config->isMgmConnection,
					    localNodeId,
					    config->remoteNodeId,
					    config->serverNodeId,
					    config->checksum,
					    config->signalId,
					    config->shm.shmKey,
					    config->shm.shmSize,
					    config->preSendChecksum,
                                            config->shm.shmSpintime,
                                            config->shm.sendBufferSize);
  if (t == NULL)
    return false;
  // Put the transporter in the transporter arrays
  allTransporters[nTransporters]            = t;
  theSHMTransporters[nSHMTransporters]      = t;
  theTransporters[t->getRemoteNodeId()]     = t;
  theTransporterTypes[t->getRemoteNodeId()] = tt_SHM_TRANSPORTER;
  performStates[t->getRemoteNodeId()]       = DISCONNECTED;
  
  nTransporters++;
  nSHMTransporters++;
  m_total_max_send_buffer += t->get_max_send_buffer();

  DBUG_RETURN(true);
#else
  return false;
#endif
}


void
TransporterRegistry::removeTransporter(NodeId nodeId) {

  DEBUG("Removing transporter from " << localNodeId
	<< " to " << nodeId);
  
  if(theTransporters[nodeId] == NULL)
    return;
  
  DEBUG_FPRINTF((stderr, "(%u)doDisconnect(%u), line: %u\n",
                 localNodeId, nodeId, __LINE__));
  theTransporters[nodeId]->doDisconnect();
  
  const TransporterType type = theTransporterTypes[nodeId];

  Uint32 ind = 0;
  switch(type){
  case tt_TCP_TRANSPORTER:
    for(; ind < nTCPTransporters; ind++)
      if(theTCPTransporters[ind]->getRemoteNodeId() == nodeId)
	break;
    ind++;
    for(; ind<nTCPTransporters; ind++)
      theTCPTransporters[ind-1] = theTCPTransporters[ind];
    nTCPTransporters --;
    break;
#ifndef WIN32
  case tt_SHM_TRANSPORTER:
    for(; ind < nSHMTransporters; ind++)
      if(theSHMTransporters[ind]->getRemoteNodeId() == nodeId)
	break;
    ind++;
    for(; ind < nSHMTransporters; ind++)
      theSHMTransporters[ind-1] = theSHMTransporters[ind];
    nSHMTransporters--;
    break;
#endif
  }
  ind = 0;
  for(; ind < nTransporters; ind++)
  {
    if(allTransporters[ind]->getRemoteNodeId() == nodeId)
      break;
  }
  ind++;
  for(; ind < nTransporters; ind++)
  {
    allTransporters[ind-1] = allTransporters[ind];
  }
  nTransporters--;

  // Delete the transporter and remove it from theTransporters array
  delete theTransporters[nodeId];
  theTransporters[nodeId] = NULL;        
}


/**
 * prepareSend() - queue a signal for later asynchronous sending.
 *
 * A successfull prepareSend() only guarantee that the signal has been
 * stored in some send buffers. Normally it will later be sent, but could
 * also be discarded if the transporter *later* disconnects.
 *
 * Signal memory is allocated with the implementation dependent
 * ::getWritePtr(). On multithreaded implementations, allocation may
 * take place in thread-local buffer pools which is later 'flushed'
 * to a global send buffer.
 *
 * Asynchronous to prepareSend() there may be Transporters
 * (dis)connecting which are signaled to the upper layers by calling
 * disable_/enable_send_buffers().
 *
 * The 'sendHandle' interface has the method ::isSendEnabled() which
 * provides us with a way to check whether communication with a node
 * is possible. Depending on the sendHandle implementation, 
 * isSendEnabled() may either have a 'synchronized' or 'optimistic'
 * implementation:
 *  - A (thread-) 'synchronized' implementation guarantee that the
 *    send buffers really are enabled, both thread local and global,
 *    at the time of send buffer allocation. (May disconnect later though)
 *  - An 'optimistic' implementation does not really know whether the
 *    send buffers are (globally) enabled. Send buffers may always be
 *    allocated and possibly silently discarded later.
 *    (SEND_DISCONNECTED will never be returned)
 *
 * The trp_client implementation is 'synchronized', while the mt-/non-mt
 * data node implementation is not. Note that a 'SEND_DISCONNECTED'
 * and 'SEND_BLOCKED' return has always been handled as an 'OK' on 
 * the data nodes. So not being able to detect 'SEND_DISCONNECTED'
 * should not matter.
 *
 * Note that sending behaves differently wrt disconnect / reconnect
 * synching compared to 'receive'. Receiver side *is* synchroinized with
 * the receiver transporter disconnect / reconnect by both requiring the
 * 'poll-right'. Thus receiver logic may check Transporter::isConnected()
 * directly.
 *
 * See further comments as part of ::performReceive().
 */
template <typename AnySectionArg>
SendStatus
TransporterRegistry::prepareSendTemplate(
                                 TransporterSendBufferHandle *sendHandle,
                                 const SignalHeader * signalHeader,
                                 Uint8 prio,
                                 const Uint32 * signalData,
                                 NodeId nodeId,
                                 AnySectionArg section)
{
  Transporter *t = theTransporters[nodeId];
  if (unlikely(t == NULL))
  {
    DEBUG("Discarding message to unknown node: " << nodeId);
    return SEND_UNKNOWN_NODE;
  }
  else if(
    likely((ioStates[nodeId] != HaltOutput) && (ioStates[nodeId] != HaltIO)) || 
           (signalHeader->theReceiversBlockNumber == QMGR) ||
           (signalHeader->theReceiversBlockNumber == API_CLUSTERMGR))
  {
    if (likely(sendHandle->isSendEnabled(nodeId)))
    {
      const Uint32 lenBytes = t->m_packer.getMessageLength(signalHeader, section.m_ptr);
      if (likely(lenBytes <= MAX_SEND_MESSAGE_BYTESIZE))
      {
        SendStatus error = SEND_OK;
        Uint32 *insertPtr = getWritePtr(sendHandle,
                                        nodeId,
                                        lenBytes,
                                        prio,
                                        &error);
        if (likely(insertPtr != nullptr))
	{
	  t->m_packer.pack(insertPtr, prio, signalHeader, signalData, section);
	  updateWritePtr(sendHandle, nodeId, lenBytes, prio);
	  return SEND_OK;
	}
        if (unlikely(error == SEND_MESSAGE_TOO_BIG))
        {
          g_eventLogger->info("Send message too big");
	  return SEND_MESSAGE_TOO_BIG;
        }
        set_status_overloaded(nodeId, true);
        const int sleepTime = 2;

	/**
	 * @note: on linux/i386 the granularity is 10ms
	 *        so sleepTime = 2 generates a 10 ms sleep.
	 */
	for (int i = 0; i < 100; i++)
	{
	  NdbSleep_MilliSleep(sleepTime);
          /* FC : Consider counting sleeps here */
	  insertPtr = getWritePtr(sendHandle, nodeId, lenBytes, prio, &error);
	  if (likely(insertPtr != nullptr))
	  {
	    t->m_packer.pack(insertPtr, prio, signalHeader, signalData, section);
	    updateWritePtr(sendHandle, nodeId, lenBytes, prio);
	    DEBUG_FPRINTF((stderr, "TE_SEND_BUFFER_FULL\n"));
	    /**
	     * Send buffer full, but resend works
	     */
	    report_error(nodeId, TE_SEND_BUFFER_FULL);
	    return SEND_OK;
	  }
          if (unlikely(error == SEND_MESSAGE_TOO_BIG))
          {
            g_eventLogger->info("Send message too big");
	    return SEND_MESSAGE_TOO_BIG;
          }
	}

	WARNING("Signal to " << nodeId << " lost(buffer)");
	DEBUG_FPRINTF((stderr, "TE_SIGNAL_LOST_SEND_BUFFER_FULL\n"));
	report_error(nodeId, TE_SIGNAL_LOST_SEND_BUFFER_FULL);
	return SEND_BUFFER_FULL;
      }
      else
      {
        g_eventLogger->info("Send message too big: length %u", lenBytes);
	return SEND_MESSAGE_TOO_BIG;
      }
    }
    else
    {
#ifdef ERROR_INSERT
      if (m_blocked.get(nodeId))
      {
        /* Looks like it disconnected while blocked.  We'll pretend
         * not to notice for now
         */
        WARNING("Signal to " << nodeId << " discarded as node blocked + disconnected");
        return SEND_OK;
      }
#endif
      DEBUG("Signal to " << nodeId << " lost(disconnect) ");
      return SEND_DISCONNECTED;
    }
  }
  else
  {
    DEBUG("Discarding message to block: " 
	  << signalHeader->theReceiversBlockNumber 
	  << " node: " << nodeId);
    
    return SEND_BLOCKED;
  }
}


SendStatus
TransporterRegistry::prepareSend(TransporterSendBufferHandle *sendHandle,
                                 const SignalHeader *signalHeader,
                                 Uint8 prio,
                                 const Uint32 *signalData,
                                 NodeId nodeId,
                                 const LinearSectionPtr ptr[3])
{
  const Packer::LinearSectionArg section(ptr);
  return prepareSendTemplate(sendHandle, signalHeader, prio, signalData, nodeId, section);
}


SendStatus
TransporterRegistry::prepareSend(TransporterSendBufferHandle *sendHandle,
                                 const SignalHeader *signalHeader,
                                 Uint8 prio,
                                 const Uint32 *signalData,
                                 NodeId nodeId,
                                 class SectionSegmentPool &thePool,
                                 const SegmentedSectionPtr ptr[3])
{
  const Packer::SegmentedSectionArg section(thePool,ptr);
  return prepareSendTemplate(sendHandle, signalHeader, prio, signalData, nodeId, section);
}


SendStatus
TransporterRegistry::prepareSend(TransporterSendBufferHandle *sendHandle,
                                 const SignalHeader *signalHeader,
                                 Uint8 prio,
                                 const Uint32 *signalData,
                                 NodeId nodeId,
                                 const GenericSectionPtr ptr[3])
{
  const Packer::GenericSectionArg section(ptr);
  return prepareSendTemplate(sendHandle, signalHeader, prio, signalData, nodeId, section);
}

void
TransporterRegistry::external_IO(Uint32 timeOutMillis) {
  //-----------------------------------------------------------
  // Most of the time we will send the buffers here and then wait
  // for new signals. Thus we start by sending without timeout
  // followed by the receive part where we expect to sleep for
  // a while.
  //-----------------------------------------------------------
  if(pollReceive(timeOutMillis, * receiveHandle)){
    performReceive(* receiveHandle);
  }
  performSend();
}

bool
TransporterRegistry::setup_wakeup_socket(TransporterReceiveHandle& recvdata)
{
  assert((receiveHandle == &recvdata) || (receiveHandle == 0));

  if (m_has_extra_wakeup_socket)
  {
    return true;
  }

  assert(!recvdata.m_transporters.get(0));

  if (ndb_socketpair(m_extra_wakeup_sockets))
  {
    perror("socketpair failed!");
    return false;
  }

  if (!TCP_Transporter::setSocketNonBlocking(m_extra_wakeup_sockets[0]) ||
      !TCP_Transporter::setSocketNonBlocking(m_extra_wakeup_sockets[1]))
  {
    goto err;
  }

#if defined(HAVE_EPOLL_CREATE)
  if (recvdata.m_epoll_fd != -1)
  {
    int sock = m_extra_wakeup_sockets[0].fd;
    struct epoll_event event_poll;
    memset(&event_poll, 0, sizeof(event_poll));
    event_poll.data.u32 = 0;
    event_poll.events = EPOLLIN;
    int ret_val = epoll_ctl(recvdata.m_epoll_fd, EPOLL_CTL_ADD, sock,
                            &event_poll);
    if (ret_val != 0)
    {
      int error= errno;
      fprintf(stderr, "Failed to add extra sock %u to epoll-set: %u\n",
              sock, error);
      fflush(stderr);
      goto err;
    }
  }
#endif
  m_has_extra_wakeup_socket = true;
  recvdata.m_transporters.set(Uint32(0));
  return true;

err:
  ndb_socket_close(m_extra_wakeup_sockets[0]);
  ndb_socket_close(m_extra_wakeup_sockets[1]);
  ndb_socket_invalidate(m_extra_wakeup_sockets+0);
  ndb_socket_invalidate(m_extra_wakeup_sockets+1);
  return false;
}

void
TransporterRegistry::wakeup()
{
  if (m_has_extra_wakeup_socket)
  {
    static char c = 37;
    ndb_send(m_extra_wakeup_sockets[1], &c, 1, 0);
  }
}

Uint32
TransporterRegistry::check_TCP(TransporterReceiveHandle& recvdata,
                               Uint32 timeOutMillis)
{
  Uint32 retVal = 0;
#if defined(HAVE_EPOLL_CREATE)
  if (likely(recvdata.m_epoll_fd != -1))
  {
    int tcpReadSelectReply = 0;
    Uint32 num_trps = nTCPTransporters + nSHMTransporters +
                      (m_has_extra_wakeup_socket ? 1 : 0);

    if (num_trps)
    {
      tcpReadSelectReply = epoll_wait(recvdata.m_epoll_fd,
                                      recvdata.m_epoll_events,
                                      num_trps, timeOutMillis);
      retVal = tcpReadSelectReply;
    }

    int num_socket_events = tcpReadSelectReply;
    if (num_socket_events > 0)
    {
      for (int i = 0; i < num_socket_events; i++)
      {
        const Uint32 trpid = recvdata.m_epoll_events[i].data.u32;
        /**
         * check that it's assigned to "us"
         */
        assert(recvdata.m_transporters.get(trpid));

        recvdata.m_recv_transporters.set(trpid);
      }
    }
    else if (num_socket_events < 0)
    {
      assert(errno == EINTR);
    }
  }
  else
#endif
  {
    retVal = poll_TCP(timeOutMillis, recvdata);
  }
  return retVal;
}

Uint32
TransporterRegistry::poll_SHM(TransporterReceiveHandle& recvdata,
                              NDB_TICKS start_poll,
                              Uint32 micros_to_poll)
{
  Uint32 res;
  Uint64 micros_passed;
  do
  {
    bool any_connected = false;
    res = poll_SHM(recvdata, any_connected);
    if (res || !any_connected)
    {
      /**
       * If data found or no SHM transporter connected there is no
       * reason to continue spinning.
       */
      break;
    }
    NDB_TICKS now = NdbTick_getCurrentTicks();
    micros_passed =
      NdbTick_Elapsed(start_poll, now).microSec();
  } while (micros_passed < Uint64(micros_to_poll));
  return res;
}

Uint32
TransporterRegistry::poll_SHM(TransporterReceiveHandle& recvdata,
                              bool &any_connected)
{
  assert((receiveHandle == &recvdata) || (receiveHandle == 0));

  Uint32 retVal = 0;
  any_connected = false;
#ifndef WIN32
  for (Uint32 i = 0; i < nSHMTransporters; i++)
  {
    SHM_Transporter * t = theSHMTransporters[i];
    Uint32 node_id = t->getRemoteNodeId();

    if (!recvdata.m_transporters.get(node_id))
      continue;

    if (t->isConnected() && is_connected(node_id))
    {
      any_connected = true;
      if (t->hasDataToRead())
      {
        recvdata.m_has_data_transporters.set(node_id);
        retVal = 1;
      }
    }
  }
#endif
  return retVal;
}

Uint32
TransporterRegistry::spin_check_transporters(
                          TransporterReceiveHandle& recvdata)
{
  Uint32 res = 0;
#ifndef WIN32
  Uint64 micros_passed = 0;
  bool any_connected = false;

  NDB_TICKS start = NdbTick_getCurrentTicks();
  do
  {
    for (Uint32 i = 0; i < 3; i++)
    {
      res = poll_SHM(recvdata, any_connected);
      if (res || !any_connected)
        break;
      cpu_pause();
    }
    if (res || !any_connected)
      break;
    res = check_TCP(recvdata, 0);
    if (res)
      break;
    NDB_TICKS now = NdbTick_getCurrentTicks();
    micros_passed =
      NdbTick_Elapsed(start, now).microSec();
  } while (micros_passed < Uint64(recvdata.m_spintime));
  recvdata.m_total_spintime += micros_passed;
#endif
  return res;
}

Uint32
TransporterRegistry::pollReceive(Uint32 timeOutMillis,
                                 TransporterReceiveHandle& recvdata)
{
  bool sleep_state_set = false;
  assert((receiveHandle == &recvdata) || (receiveHandle == 0));

  Uint32 retVal = 0;
  recvdata.m_recv_transporters.clear();

  /**
   * If any transporters have left-over data that was not fully executed in
   * last loop, don't wait and return 'data available' even if nothing new
   */
  if (!recvdata.m_has_data_transporters.isclear())
  {
    timeOutMillis = 0;
    retVal = 1;
  }
#ifndef WIN32
  if (nSHMTransporters > 0)
  {
    /**
     * We start by checking shared memory transporters without
     * any mutexes or other protection. If we find something to
     * read we will set timeout to 0 and check the TCP transporters
     * before returning.
     */
    bool any_connected = false;
    Uint32 res = poll_SHM(recvdata, any_connected);
    if(res)
    {
      retVal |= res;
      timeOutMillis = 0;
    }
    else if (timeOutMillis > 0 && any_connected)
    {
      /**
       * We are preparing to wait for socket events. We will start by
       * polling for a configurable amount of microseconds before we
       * go to sleep. We will check both shared memory transporters and
       * TCP transporters in this period. We will check shared memory
       * transporter four times and then check TCP transporters in a
       * loop.
       *
       * After this polling period, if we are still waiting for data
       * we will prepare to go to sleep by informing the other side
       * that we are going to sleep.
       *
       * To do this we first grab the mutex used by the sender
       * to check for sleep/awake state, next we poll the shared
       * memory holding this mutex. If this check also returns
       * without finding any data we set the state to sleep,
       * release the mutex and go to sleep (on an epoll/poll)
       * that can be woken up by incoming data or a wakeup byte
       * sent to SHM transporter.
       */
      res = spin_check_transporters(recvdata);
      if (res)
      {
        retVal |= res;
        timeOutMillis = 0;
      }
      else
      {
        int res = reset_shm_awake_state(recvdata, sleep_state_set);
        if (res || !sleep_state_set)
        {
          /**
           * If sleep_state_set is false here it means that the
           * all SHM transporters were disconnected. Alternatively
           * there was data available on all the connected SHM
           * transporters. Read the data from TCP transporters and
           * return.
           */
          retVal |= 1;
          timeOutMillis = 0;
        }
      }
    }
  }
#endif
  retVal |= check_TCP(recvdata, timeOutMillis);
#ifndef WIN32
  if (nSHMTransporters > 0)
  {
    /**
     * If any SHM transporter was put to sleep above we will
     * set all connected SHM transporters to awake now.
     */
    if (sleep_state_set)
    {
      set_shm_awake_state(recvdata);
    }
    bool any_connected = false;
    int res = poll_SHM(recvdata, any_connected);
    retVal |= res;
  }
#endif
  return retVal;
}

/**
 * Every time a SHM transporter is sending data and wants the other side
 * to wake up to handle the data, it follows this procedure.
 * 1) Write the data in shared memory
 * 2) Acquire the mutex protecting the awake flag on the receive side.
 * 3) Read flag
 * 4) Release mutex
 * 5.1) If flag says that receiver is awake we're done
 * 5.2) If flag says that receiver is asleep we will send a byte on the
 *      transporter socket for the SHM transporter to wake up the other
 *      side.
 *
 * The reset_shm_awake_state is called right before we are going to go
 * to sleep. To ensure that we don't miss any signals from the other
 * side we will first check if there is data available on shared memory.
 * We first grab the mutex before checking this. If no data is available
 * we can proceed to go to sleep after setting the flag to indicate that
 * we are asleep. The above procedure used when sending means that we
 * know that the sender will always know the correct state. The only
 * error in this is that the sender might think that we are asleep when
 * we actually is still on our way to go to sleep. In this case no harm
 * has been done since the only thing that have happened is that one byte
 * is sent on the SHM socket that wasn't absolutely needed.
 */
int
TransporterRegistry::reset_shm_awake_state(TransporterReceiveHandle& recvdata,
                                       bool& sleep_state_set)
{
  int res = 0;
#ifndef WIN32
  for (Uint32 i = 0; i < nSHMTransporters; i++)
  {
    SHM_Transporter * t = theSHMTransporters[i];
    Uint32 node_id = t->getRemoteNodeId();

    if (!recvdata.m_transporters.get(node_id))
      continue;

    if (t->isConnected())
    {
      t->lock_mutex();
      if (is_connected(node_id))
      {
        if (t->hasDataToRead())
        {
          recvdata.m_has_data_transporters.set(node_id);
          res = 1;
        }
        else
        {
          sleep_state_set = true;
          t->set_awake_state(0);
        }
      }
      t->unlock_mutex();
    }
  }
#endif
  return res;
}

/**
 * We have been sleeping for a while, before proceeding we need to set
 * the flag to awake in the shared memory. This will flag to all other
 * nodes using shared memory to communicate that we're awake and don't
 * need any socket communication to wake up.
 */
void
TransporterRegistry::set_shm_awake_state(TransporterReceiveHandle& recvdata)
{
#ifndef WIN32
  for (Uint32 i = 0; i < nSHMTransporters; i++)
  {
    SHM_Transporter * t = theSHMTransporters[i];
    Uint32 node_id = t->getRemoteNodeId();

    if (!recvdata.m_transporters.get(node_id))
      continue;
    if (t->isConnected())
    {
      t->lock_mutex();
      t->set_awake_state(1);
      t->unlock_mutex();
    }
  }
#endif
}

/**
 * We do not want to hold any transporter locks during select(), so there
 * is no protection against a disconnect closing the socket during this call.
 *
 * That does not matter, at most we will get a spurious wakeup on the wrong
 * socket, which will be handled correctly in performReceive() (which _is_
 * protected by transporter locks on upper layer).
 */
Uint32
TransporterRegistry::poll_TCP(Uint32 timeOutMillis,
                              TransporterReceiveHandle& recvdata)
{
  assert((receiveHandle == &recvdata) || (receiveHandle == 0));

  recvdata.m_socket_poller.clear();

  const bool extra_socket = m_has_extra_wakeup_socket;
  if (extra_socket && recvdata.m_transporters.get(0))
  {
    const NDB_SOCKET_TYPE socket = m_extra_wakeup_sockets[0];
    assert(&recvdata == receiveHandle); // not used by ndbmtd...

    // Poll the wakup-socket for read
    recvdata.m_socket_poller.add(socket, true, false, false);
  }

  Uint16 idx[MAX_NODES];
  Uint32 i = 0;
  for (; i < nTCPTransporters; i++)
  {
    TCP_Transporter * t = theTCPTransporters[i];
    const NDB_SOCKET_TYPE socket = t->getSocket();
    Uint32 node_id = t->getRemoteNodeId();

    idx[i] = MAX_NODES + 1;
    if (!recvdata.m_transporters.get(node_id))
      continue;

    if (is_connected(node_id) && t->isConnected() && ndb_socket_valid(socket))
    {
      idx[i] = recvdata.m_socket_poller.add(socket, true, false, false);
    }
  }

#ifndef WIN32
  for (Uint32 j = 0; j < nSHMTransporters; j++)
  {
    /**
     * We need to listen to socket also for shared memory transporters.
     * These sockets are used as a wakeup mechanism, so we're not sending
     * any data in it. But we need a socket to be able to wake up things
     * when the receiver is not awake and we've only sent data on shared
     * memory transporter.
     */
    SHM_Transporter * t = theSHMTransporters[j];
    const NDB_SOCKET_TYPE socket = t->getSocket();
    Uint32 node_id = t->getRemoteNodeId();
    idx[i] = MAX_NODES + 1;
    if (!recvdata.m_transporters.get(node_id))
    {
      i++;
      continue;
    }
    if (is_connected(node_id) && t->isConnected() && ndb_socket_valid(socket))
    {
      idx[i] = recvdata.m_socket_poller.add(socket, true, false, false);
    }
    i++;
  }
#endif
  int tcpReadSelectReply = recvdata.m_socket_poller.poll_unsafe(timeOutMillis);

  if (tcpReadSelectReply > 0)
  {
    if (extra_socket)
    {
      if (recvdata.m_socket_poller.has_read(0))
      {
        assert(recvdata.m_transporters.get(0));
        recvdata.m_recv_transporters.set((Uint32)0);
      }
    }
    i = 0;
    for (; i < nTCPTransporters; i++)
    {
      TCP_Transporter * t = theTCPTransporters[i];
      if (idx[i] != MAX_NODES + 1)
      {
        Uint32 node_id = t->getRemoteNodeId();
        if (recvdata.m_socket_poller.has_read(idx[i]))
          recvdata.m_recv_transporters.set(node_id);
      }
    }
#ifndef WIN32
    for (Uint32 j = 0; j < nSHMTransporters; j++)
    {
      /**
       * If a shared memory transporter have data on its socket we
       * will get it now, the data is only an indication for us to
       * wake up, so we're not interested in the data as such.
       * But to integrate it with epoll handling we will read it
       * in performReceive still.
       */
      SHM_Transporter * t = theSHMTransporters[j];
      if (idx[i] != MAX_NODES + 1)
      {
        Uint32 node_id = t->getRemoteNodeId();
        if (recvdata.m_socket_poller.has_read(idx[i]))
          recvdata.m_recv_transporters.set(node_id);
      }
      i++;
    }
#endif
  }

  return tcpReadSelectReply;
}

/**
 * Receive from the set of transporters in the bitmask
 * 'recvdata.m_transporters'. These has been polled by 
 * ::pollReceive() which recorded transporters with 
 * available data in the subset 'recvdata.m_recv_transporters'.
 *
 * In multi-threaded datanodes, there might be multiple 
 * receiver threads, each serving a disjunct set of
 * 'm_transporters'.
 *
 * Single-threaded datanodes does all ::performReceive
 * from the scheduler main-loop, and thus it will handle
 * all 'm_transporters'.
 *
 * Clients has to aquire a 'poll right' (see TransporterFacade)
 * which gives it the right to temporarily acts as a receive 
 * thread with the right to poll *all* transporters.
 *
 * Reception takes place on a set of transporters knowing to be in a
 * 'CONNECTED' state. Transporters can (asynch) become 'DISCONNECTING' 
 * while we performReceive(). There is *no* mutex lock protecting
 * 'disconnecting' from being started while we are in the receive-loop!
 * However, the contents of the buffers++  should still be in a 
 * consistent state, such that the current receive can complete
 * without failures. 
 *
 * With regular intervals we have to ::update_connections()
 * in order to bring DISCONNECTING transporters into
 * a DISCONNECTED state. At earlies at this point, resources
 * used by performReceive() may be reset or released.
 * A transporter should be brought to the DISCONNECTED state
 * before it can reconnect again. (Note: There is a break of
 * this rule in ::do_connect, see own note here)
 *
 * To not interfere with ::poll- or ::performReceive(),
 * ::update_connections() has to be synched with with these
 * methods. Either by being run within the same
 * receive thread (dataNodes), or protected by the 'poll rights'.
 *
 * In case we were unable to receive due to job buffers being full.
 * Returns 0 when receive succeeded from all Transporters having data,
 * else 1.
 */
Uint32
TransporterRegistry::performReceive(TransporterReceiveHandle& recvdata)
{
  TransporterReceiveWatchdog guard(recvdata);
  assert((receiveHandle == &recvdata) || (receiveHandle == 0));
  bool stopReceiving = false;

  if (recvdata.m_recv_transporters.get(0))
  {
    assert(recvdata.m_transporters.get(0));
    assert(&recvdata == receiveHandle); // not used by ndbmtd
    recvdata.m_recv_transporters.clear(Uint32(0));
    consume_extra_sockets();
  }

#ifdef ERROR_INSERT
  if (!m_blocked.isclear())
  {
    /* Exclude receive from blocked sockets. */
    recvdata.m_recv_transporters.bitANDC(m_blocked);

    if (recvdata.m_recv_transporters.isclear()  &&
        recvdata.m_has_data_transporters.isclear())
    {
        /* poll sees data, but we want to ignore for now
         * sleep a little to avoid busy loop
         */
      NdbSleep_MilliSleep(1);
    }
  }
#endif

  /**
   * m_recv_transporters set indicates that there might be data
   * available on the socket used by the transporter. The
   * doReceive call will read the socket. For TCP transporters
   * the doReceive call will return an indication if there is
   * data to receive on socket. This will set m_has_data_transporters.
   * For SHM transporter the socket is only used to send wakeup
   * bytes. The m_has_data_transporters bitmap was set already in
   * pollReceive for SHM transporters.
   */
  for(Uint32 id = recvdata.m_recv_transporters.find_first();
      id != BitmaskImpl::NotFound;
      id = recvdata.m_recv_transporters.find_next(id + 1))
  {

    if (theTransporters[id]->getTransporterType() == tt_TCP_TRANSPORTER)
    {
      TCP_Transporter * t = (TCP_Transporter*)theTransporters[id];
      assert(recvdata.m_transporters.get(id));

      /**
       * First check connection 'is CONNECTED.
       * A connection can only be set into, or taken out of, is_connected'
       * state by ::update_connections(). See comment there about 
       * synchronication between ::update_connections() and 
       * performReceive()
       *
       * Transporter::isConnected() state my change asynch.
       * A mismatch between the TransporterRegistry::is_connected(),
       * and Transporter::isConnected() state is possible, and indicate 
       * that a change is underway. (Completed by update_connections())
       */
      if (is_connected(id))
      {
        if (t->isConnected())
        {
          int nBytes = t->doReceive(recvdata);
          if (nBytes > 0)
          {
            recvdata.transporter_recv_from(id);
            recvdata.m_has_data_transporters.set(id);
          }
        }
      }
    }
    else
    {
#ifndef WIN32
      require(theTransporters[id]->getTransporterType() == tt_SHM_TRANSPORTER);
      SHM_Transporter * t = (SHM_Transporter*)theTransporters[id];
      assert(recvdata.m_transporters.get(id));
      if (is_connected(id) && t->isConnected())
      {
        t->doReceive();
        /**
         * Ignore any data we read, the data wasn't collected by the
         * shared memory transporter, it was simply read and thrown
         * away, it is only a wakeup call to send data over the socket
         * for shared memory transporters.
         */
      }
#else
      require(false);
#endif
    }
  }
  recvdata.m_recv_transporters.clear();

  /**
   * Unpack data either received above or pending from prev rounds.
   * For the Shared memory transporter m_has_data_transporters can
   * be set in pollReceive as well.
   *
   * TCP Transporter
   * ---------------
   * Data to be processed at this stage is in the Transporter 
   * receivebuffer. The data *is received*, and will stay in
   * the  receiveBuffer even if a disconnect is started during
   * unpack. 
   * When ::update_connection() finaly completes the disconnect,
   * (synced with ::performReceive()), 'm_has_data_transporters'
   * will be cleared, which will terminate further unpacking.
   *
   * NOTE:
   *  Without reading inconsistent date, we could have removed
   *  the 'connected' checks below, However, there is a requirement
   *  in the CLOSE_COMREQ/CONF protocol between TRPMAN and QMGR
   *  that no signals arrives from disconnecting nodes after a
   *  CLOSE_COMCONF was sent. For the moment the risk of taking
   *  advantage of this small optimization is not worth the risk.
   */
  Uint32 id = recvdata.m_last_nodeId;
  while ((id = recvdata.m_has_data_transporters.find_next(id + 1)) !=
	 BitmaskImpl::NotFound)
  {
    bool hasdata = false;
    Transporter * t = (Transporter*)theTransporters[id];

    assert(recvdata.m_transporters.get(id));

    if (is_connected(id))
    {
      if (t->isConnected())
      {
        if (unlikely(recvdata.checkJobBuffer()))
        {
          return 1;     // Full, can't unpack more
        }
        if (unlikely(recvdata.m_handled_transporters.get(id)))
          continue;     // Skip now to avoid starvation
        if (t->getTransporterType() == tt_TCP_TRANSPORTER)
        {
          TCP_Transporter *t_tcp = (TCP_Transporter*)t;
          Uint32 * ptr;
          Uint32 sz = t_tcp->getReceiveData(&ptr);
          Uint32 szUsed = unpack(recvdata, ptr, sz, id, ioStates[id], stopReceiving);
          if (likely(szUsed))
          {
            t_tcp->updateReceiveDataPtr(szUsed);
            hasdata = t_tcp->hasReceiveData();
          }
        }
        else
        {
#ifndef WIN32
          require(t->getTransporterType() == tt_SHM_TRANSPORTER);
          SHM_Transporter *t_shm = (SHM_Transporter*)t;
          Uint32 * readPtr, * eodPtr, * endPtr;
          t_shm->getReceivePtr(&readPtr, &eodPtr, &endPtr);
          recvdata.transporter_recv_from(id);
          Uint32 *newPtr = unpack(recvdata,
                                  readPtr,
                                  eodPtr,
                                  endPtr,
                                  id,
                                  ioStates[id],
				  stopReceiving);
          t_shm->updateReceivePtr(recvdata, newPtr);
          /**
           * Set hasdata dependent on if data is still available in
           * transporter to ensure we follow rules about setting
           * m_has_data_transporters and m_handled_transporters
           * when returning from performReceive.
           */
          hasdata = t_shm->hasDataToRead();
#else
          require(false);
#endif
        }
        // else, we didn't unpack anything:
        //   Avail ReceiveData to short to be useful, need to
        //   receive more before we can resume this transporter.
      }
    }
    // If transporter still have data, make sure that it's remember to next time
    recvdata.m_has_data_transporters.set(id, hasdata);
    recvdata.m_handled_transporters.set(id, hasdata);

    if (unlikely(stopReceiving))
    {
      recvdata.m_last_nodeId = id;  //Resume from node after 'last_node'
      return 1;
    }
  }
  recvdata.m_handled_transporters.clear();
  recvdata.m_last_nodeId = 0;
  return 0;
}

void
TransporterRegistry::consume_extra_sockets()
{
  char buf[4096];
  ssize_t ret;
  int err;
  NDB_SOCKET_TYPE sock = m_extra_wakeup_sockets[0];
  do
  {
    ret = ndb_recv(sock, buf, sizeof(buf), 0);
    err = ndb_socket_errno();
  } while (ret == sizeof(buf) || (ret == -1 && err == EINTR));

  /* Notify upper layer of explicit wakeup */
  callbackObj->reportWakeup();
}

/**
 * performSend() - Call physical transporters to 'doSend'
 * of previously prepareSend() signals.
 *
 * The doSend() implementations will call
 * TransporterCallback::get_bytes_to_send_iovec() to fetch
 * any available data from the send buffer.
 *
 * *This* ^^ is the synch point where we under mutex protection
 * may check for specific nodes being disconnected/disabled.
 * For disabled nodes we may drain the send buffers instead of
 * returning anything from get_bytes_to_send_iovec().
 * Also see comments for prepareSend() above.
 *
 * Note that since disconnection may happen asynch from other
 * threads, we can not reliably check the 'connected' state
 * before doSend(). Instead we must require that the 
 * TransporterCallback implementation provide necessary locking
 * of get_bytes_to_send() vs enable/disable of send buffers.
 *
 * Returns:
 *   true if anything still remains to be sent.
 *   Will require another ::performSend()
 *
 *   false: if nothing more remains, either due to
 *   the send buffers being empty, we succeeded
 *   sending everything, or we found the node to be
 *   disconnected and thus discarded the contents.
 */
bool
TransporterRegistry::performSend(NodeId nodeId, bool need_wakeup)
{
  Transporter *t = get_transporter(nodeId);
  if (t != NULL)
  {
#ifdef ERROR_INSERT
    if (m_sendBlocked.get(nodeId))
    {
      return true;
    }
#endif
    return t->doSend(need_wakeup);
  }
  return false;
}

void
TransporterRegistry::performSend()
{
  Uint32 i; 
  sendCounter = 1;

  for (i = m_transp_count; i < nTransporters; i++) 
  {
    Transporter *t = allTransporters[i];
    if (t != NULL
#ifdef ERROR_INSERT
        && !m_sendBlocked.get(t->getRemoteNodeId())
#endif
        )
    {
      t->doSend();
    }
  }
  for (i = 0; i < m_transp_count && i < nTransporters; i++) 
  {
    Transporter *t = allTransporters[i];
    if (t != NULL
#ifdef ERROR_INSERT
        && !m_sendBlocked.get(t->getRemoteNodeId())
#endif
        )
    {
      t->doSend();
    }
  }
  m_transp_count++;
  if (m_transp_count == nTransporters)
    m_transp_count = 0;
}

#ifdef DEBUG_TRANSPORTER
void
TransporterRegistry::printState(){
  ndbout << "-- TransporterRegistry -- " << endl << endl
         << "Transporters = " << nTransporters << endl;
  for(Uint32 i = 0; i < maxTransporters; i++)
  {
    if (theTransporters[i] != NULL)
    {
      const NodeId remoteNodeId = theTransporters[i]->getRemoteNodeId();
      ndbout << "Transporter: " << remoteNodeId 
             << " PerformState: " << performStates[remoteNodeId]
             << " IOState: " << ioStates[remoteNodeId] << endl;
    }
  }
}
#endif

#ifdef ERROR_INSERT
bool
TransporterRegistry::isBlocked(NodeId nodeId)
{
  return m_blocked.get(nodeId);
}

void
TransporterRegistry::blockReceive(TransporterReceiveHandle& recvdata,
                                  NodeId nodeId)
{
  assert((receiveHandle == &recvdata) || (receiveHandle == 0));
  assert(recvdata.m_transporters.get(nodeId));

  /* Check that node is not already blocked?
   * Stop pulling from its socket (but track received data etc)
   */
  /* Shouldn't already be blocked with data */
  assert(!m_blocked.get(nodeId));

  m_blocked.set(nodeId);
}

void
TransporterRegistry::unblockReceive(TransporterReceiveHandle& recvdata,
                                    NodeId nodeId)
{
  assert((receiveHandle == &recvdata) || (receiveHandle == 0));
  assert(recvdata.m_transporters.get(nodeId));

  /* Check that node is blocked?
   * Resume pulling from its socket
   * Ensure in-flight data is processed if there was some
   */
  assert(m_blocked.get(nodeId));
  assert(!recvdata.m_has_data_transporters.get(nodeId));

  m_blocked.clear(nodeId);

  if (m_blocked_disconnected.get(nodeId))
  {
    /* Process disconnect notification/handling now */
    m_blocked_disconnected.clear(nodeId);

    report_disconnect(recvdata, nodeId, m_disconnect_errors[nodeId]);
  }
}

bool
TransporterRegistry::isSendBlocked(NodeId nodeId) const
{
  return m_sendBlocked.get(nodeId);
}

void
TransporterRegistry::blockSend(TransporterReceiveHandle& recvdata,
                               NodeId nodeId)
{
  assert((receiveHandle == &recvdata) || (receiveHandle == 0));
  assert(recvdata.m_transporters.get(nodeId));

  m_sendBlocked.set(nodeId);
}

void
TransporterRegistry::unblockSend(TransporterReceiveHandle& recvdata,
                                 NodeId nodeId)
{
  assert((receiveHandle == &recvdata) || (receiveHandle == 0));
  assert(recvdata.m_transporters.get(nodeId));

  m_sendBlocked.clear(nodeId);
}

#endif

#ifdef ERROR_INSERT
Uint32
TransporterRegistry::getMixologyLevel() const
{
  return m_mixology_level;
}

extern Uint32 MAX_RECEIVED_SIGNALS;  /* Packer.cpp */

#define MIXOLOGY_MIX_INCOMING_SIGNALS 4

void
TransporterRegistry::setMixologyLevel(Uint32 l)
{
  m_mixology_level = l;
  
  if (m_mixology_level & MIXOLOGY_MIX_INCOMING_SIGNALS)
  {
    ndbout_c("MIXOLOGY_MIX_INCOMING_SIGNALS on");
    /* Max one signal per transporter */
    MAX_RECEIVED_SIGNALS = 1;
  }

  /* TODO : Add mixing of Send from NdbApi / MGMD */
}
#endif

IOState
TransporterRegistry::ioState(NodeId nodeId) const { 
  return ioStates[nodeId]; 
}

void
TransporterRegistry::setIOState(NodeId nodeId, IOState state) {
  if (ioStates[nodeId] == state)
    return;

  DEBUG("TransporterRegistry::setIOState("
        << nodeId << ", " << state << ")");

  ioStates[nodeId] = state;
}

extern "C" void *
run_start_clients_C(void * me)
{
  ((TransporterRegistry*) me)->start_clients_thread();
  return 0;
}

/**
 * This method is used to initiate connection, called from the TRPMAN block.
 *
 * This works asynchronously, no actions are taken directly in the calling
 * thread.
 */
void
TransporterRegistry::do_connect(NodeId node_id)
{
  PerformState &curr_state = performStates[node_id];
  switch(curr_state){
  case DISCONNECTED:
    break;
  case CONNECTED:
    return;
  case CONNECTING:
    return;
  case DISCONNECTING:
    /**
     * NOTE (Need future work)
     * Going directly from DISCONNECTING to CONNECTING creates
     * a possible race with ::update_connections(): It will
     * see either of the *ING states, and bring the connection
     * into CONNECTED or *DISCONNECTED* state. Furthermore, the
     * state may be overwritten to CONNECTING by this method.
     * We should probably have waited for DISCONNECTED state,
     * before allowing reCONNECTING ....
     */
    assert(false);
    break;
  }
  DEBUG_FPRINTF((stderr, "(%u)REG:do_connect(%u)\n", localNodeId, node_id));
  DBUG_ENTER("TransporterRegistry::do_connect");
  DBUG_PRINT("info",("performStates[%d]=CONNECTING",node_id));

  Transporter * t = theTransporters[node_id];
  if (t != NULL)
  {
    DEBUG_FPRINTF((stderr, "(%u)REG:resetBuffers(%u)\n",
                           localNodeId, node_id));
    t->resetBuffers();
  }

  curr_state= CONNECTING;
  DBUG_VOID_RETURN;
}

/**
 * This method is used to initiate disconnect from TRPMAN. It is also called
 * from the TCP/SHM transporter in case of an I/O error on the socket.
 *
 * This works asynchronously, similar to do_connect().
 */
bool
TransporterRegistry::do_disconnect(NodeId node_id,
                                   int errnum,
                                   bool send_source)
{
  DEBUG_FPRINTF((stderr, "(%u)REG:do_disconnect(%u, %d)\n",
                         localNodeId, node_id, errnum));
  PerformState &curr_state = performStates[node_id];
  switch(curr_state){
  case DISCONNECTED:
  {
    return true;
  }
  case CONNECTED:
  {
    break;
  }
  case CONNECTING:
    /**
     * This is a correct transition. But it should only occur for nodes
     * that lack resources, e.g. lack of shared memory resources to
     * setup the transporter. Therefore we assert here to get a simple
     * handling of test failures such that we can fix the test config.
     */
    //DBUG_ASSERT(false);
    break;
  case DISCONNECTING:
  {
    return true;
  }
  }
  if (errnum == ENOENT)
  {
    m_disconnect_enomem_error[node_id]++;
    if (m_disconnect_enomem_error[node_id] < 10)
    {
      NdbSleep_MilliSleep(40);
      g_eventLogger->info("Socket error %d on nodeId: %u in state: %u",
                          errnum, node_id, (Uint32)curr_state);
      return false;
    }
  }
  if (errnum == 0)
  {
    g_eventLogger->info("Node %u disconnected in state: %d",
                        node_id, (int)curr_state);
  }
  else
  {
    g_eventLogger->info("Node %u disconnected in %s with errnum: %d"
                        " in state: %d",
                        node_id,
                        send_source ? "send" : "recv",
                        errnum,
                        (int)curr_state);
  }
  DBUG_ENTER("TransporterRegistry::do_disconnect");
  DBUG_PRINT("info",("performStates[%d]=DISCONNECTING",node_id));
  curr_state= DISCONNECTING;
  m_disconnect_errnum[node_id] = errnum;
  DBUG_RETURN(false);
}

/**
 * report_connect() / report_disconnect()
 *
 * Connect or disconnect the 'TransporterReceiveHandle' and 
 * enable/disable the send buffers.
 *
 * To prevent races wrt poll/receive of data, these methods must
 * either be called from the same (receive-)thread as performReceive(),
 * or by the (API) client holding the poll-right.
 *
 * The send buffers needs similar protection against concurent
 * enable/disable of the same send buffers. Thus the sender
 * side is also handled here.
 */
void
TransporterRegistry::report_connect(TransporterReceiveHandle& recvdata,
                                    NodeId node_id)
{
  DEBUG_FPRINTF((stderr, "(%u)REG:report_connect(%u)\n",
                         localNodeId, node_id));
  assert((receiveHandle == &recvdata) || (receiveHandle == 0));
  assert(recvdata.m_transporters.get(node_id));

  DBUG_ENTER("TransporterRegistry::report_connect");
  DBUG_PRINT("info",("performStates[%d]=CONNECTED",node_id));

  if (recvdata.epoll_add(theTransporters[node_id]))
  {
    callbackObj->enable_send_buffer(node_id);
    performStates[node_id] = CONNECTED;
    recvdata.reportConnect(node_id);
    DBUG_VOID_RETURN;
  }

  /**
   * Failed to add to epoll_set...
   *   disconnect it (this is really really bad)
   */
  performStates[node_id] = DISCONNECTING;
  DBUG_VOID_RETURN;
}

void
TransporterRegistry::report_disconnect(TransporterReceiveHandle& recvdata,
                                       NodeId node_id, int errnum)
{
  DEBUG_FPRINTF((stderr, "(%u)REG:report_disconnect(%u, %d)\n",
                         localNodeId, node_id, errnum));
  assert((receiveHandle == &recvdata) || (receiveHandle == 0));
  assert(recvdata.m_transporters.get(node_id));

  DBUG_ENTER("TransporterRegistry::report_disconnect");
  DBUG_PRINT("info",("performStates[%d]=DISCONNECTED",node_id));

#ifdef ERROR_INSERT
  if (m_blocked.get(node_id))
  {
    /* We are simulating real latency, so control events experience
     * it too
     */
    m_blocked_disconnected.set(node_id);
    m_disconnect_errors[node_id] = errnum;
    DBUG_VOID_RETURN;
  }
#endif

  /**
   * No one else should be using the transporter now,
   * reset its send buffer and recvdata.
   *
   * Note that we may 'do_disconnect' due to transporter failure,
   * while trying to 'CONNECTING'. This cause a transition
   * from CONNECTING to DISCONNECTING without first being CONNECTED.
   * Thus there can be multiple reset & disable of the buffers (below)
   * without being 'enabled' inbetween.
   */
  callbackObj->disable_send_buffer(node_id);
  performStates[node_id] = DISCONNECTED;
  recvdata.m_recv_transporters.clear(node_id);
  recvdata.m_has_data_transporters.clear(node_id);
  recvdata.m_handled_transporters.clear(node_id);
  recvdata.m_bad_data_transporters.clear(node_id);
  recvdata.m_last_nodeId = 0;
  recvdata.reportDisconnect(node_id, errnum);
  DBUG_VOID_RETURN;
}

/**
 * We only call TransporterCallback::reportError() from
 * TransporterRegistry::update_connections().
 *
 * In other places we call this method to enqueue the error that will later be
 * picked up by update_connections().
 */
void
TransporterRegistry::report_error(NodeId nodeId, TransporterError errorCode,
                                  const char *errorInfo)
{
  DEBUG_FPRINTF((stderr, "(%u)REG:report_error(%u, %d, %s\n",
                         localNodeId, nodeId, (int)errorCode, errorInfo));
  if (m_error_states[nodeId].m_code == TE_NO_ERROR &&
      m_error_states[nodeId].m_info == (const char *)~(UintPtr)0)
  {
    m_error_states[nodeId].m_code = errorCode;
    m_error_states[nodeId].m_info = errorInfo;
  }
}

/**
 * update_connections(), together with the thread running in
 * start_clients_thread(), handle the state changes for transporters as they
 * connect and disconnect.
 *
 * update_connections on a specific set of recvdata *must not* be run
 * concurrently with :performReceive() on the same recvdata. Thus,
 * it must either be called from the same (receive-)thread as
 * performReceive(), or protected by aquiring the (client) poll rights.
 */
void
TransporterRegistry::update_connections(TransporterReceiveHandle& recvdata)
{
  Uint32 spintime = 0;
  TransporterReceiveWatchdog guard(recvdata);
  assert((receiveHandle == &recvdata) || (receiveHandle == 0));

  for (Uint32 i= 0, n= 0; n < nTransporters; i++){
    Transporter * t = theTransporters[i];
    if (!t)
      continue;
    n++;

    const NodeId nodeId = t->getRemoteNodeId();
    if (!recvdata.m_transporters.get(nodeId))
      continue;

    TransporterError code = m_error_states[nodeId].m_code;
    const char *info = m_error_states[nodeId].m_info;
    if (code != TE_NO_ERROR && info != (const char *)~(UintPtr)0)
    {
      if (performStates[nodeId] == CONNECTING)
      {
        fprintf(stderr, "update_connections while CONNECTING, nodeId:%d, error:%d\n", nodeId, code);
        /* Failed during CONNECTING -> we are still DISCONNECTED */
        assert(!t->isConnected());
        assert(false);
        performStates[nodeId] = DISCONNECTED;
      }

      recvdata.reportError(nodeId, code, info);
      m_error_states[nodeId].m_code = TE_NO_ERROR;
      m_error_states[nodeId].m_info = (const char *)~(UintPtr)0;
    }

    switch(performStates[nodeId]){
    case CONNECTED:
#ifndef WIN32
      if (t->getTransporterType() == tt_SHM_TRANSPORTER)
      {
        SHM_Transporter *shm_trp = (SHM_Transporter*)t;
        spintime = MAX(spintime, shm_trp->get_spintime());
      }
#endif
      break;
    case DISCONNECTED:
      break;
    case CONNECTING:
      if(t->isConnected())
	report_connect(recvdata, nodeId);
      break;
    case DISCONNECTING:
      if(!t->isConnected())
	report_disconnect(recvdata, nodeId, m_disconnect_errnum[nodeId]);
      break;
    }
  }
  recvdata.m_spintime = spintime;
}

/**
 * Run as own thread
 * Possible blocking parts of transporter connect and diconnect
 * is supposed to be handled here.
 */
void
TransporterRegistry::start_clients_thread()
{
  int persist_mgm_count= 0;
  DBUG_ENTER("TransporterRegistry::start_clients_thread");
  while (m_run_start_clients_thread) {
    NdbSleep_MilliSleep(100);
    persist_mgm_count++;
    if(persist_mgm_count==50)
    {
      ndb_mgm_check_connection(m_mgm_handle);
      persist_mgm_count= 0;
    }
    for (Uint32 i= 0, n= 0; n < nTransporters && m_run_start_clients_thread; i++){
      Transporter * t = theTransporters[i];
      if (!t)
	continue;
      n++;

      const NodeId nodeId = t->getRemoteNodeId();
      switch(performStates[nodeId]){
      case CONNECTING:
	if(!t->isConnected() && !t->isServer) {
          if (get_and_clear_node_up_indicator(nodeId))
          {
            // Other node have indicated that node nodeId is up, try connect
            // now and restart backoff sequence
            backoff_reset_connecting_time(nodeId);
          }
          if (!backoff_update_and_check_time_for_connect(nodeId))
          {
            // Skip connect this time
            continue;
          }

	  bool connected= false;
	  /**
	   * First, we try to connect (if we have a port number).
	   */

	  if (t->get_s_port())
          {
            DBUG_PRINT("info", ("connecting to node %d using port %d",
                                nodeId, t->get_s_port()));
            connected= t->connect_client();
          }

	  /**
	   * If dynamic, get the port for connecting from the management server
	   */
	  if( !connected && t->get_s_port() <= 0) {	// Port is dynamic
	    int server_port= 0;
	    struct ndb_mgm_reply mgm_reply;

            DBUG_PRINT("info", ("connection to node %d should use "
                                "dynamic port",
                                nodeId));

	    if(!ndb_mgm_is_connected(m_mgm_handle))
	      ndb_mgm_connect(m_mgm_handle, 0, 0, 0);

	    if(ndb_mgm_is_connected(m_mgm_handle))
	    {
              DBUG_PRINT("info", ("asking mgmd which port to use for node %d",
                                  nodeId));

              const int res=
		ndb_mgm_get_connection_int_parameter(m_mgm_handle,
						     t->getRemoteNodeId(),
						     t->getLocalNodeId(),
						     CFG_CONNECTION_SERVER_PORT,
						     &server_port,
						     &mgm_reply);
	      DBUG_PRINT("info",("Got dynamic port %d for %d -> %d (ret: %d)",
				 server_port,t->getRemoteNodeId(),
				 t->getLocalNodeId(),res));
	      if( res >= 0 )
	      {
                DBUG_PRINT("info", ("got port %d to use for connection to %d",
                                    server_port, nodeId));

		if (server_port != 0)
                {
                  if (t->get_s_port() != server_port)
                  {
                    // Got a different port number, reset backoff
                    backoff_reset_connecting_time(nodeId);
                  }
                  // Save the new port number
		  t->set_s_port(server_port);
                }
                else
                {
                  // Got port number 0, port is not known.  Keep the old.
                }
	      }
	      else if(ndb_mgm_is_connected(m_mgm_handle))
	      {
                DBUG_PRINT("info", ("Failed to get dynamic port, res: %d",
                                    res));
                g_eventLogger->info("Failed to get dynamic port, res: %d",
                                    res);
		ndb_mgm_disconnect(m_mgm_handle);
	      }
	      else
	      {
                DBUG_PRINT("info", ("mgmd close connection early"));
                g_eventLogger->info
                  ("Management server closed connection early. "
                   "It is probably being shut down (or has problems). "
                   "We will retry the connection. %d %s %s line: %d",
                   ndb_mgm_get_latest_error(m_mgm_handle),
                   ndb_mgm_get_latest_error_desc(m_mgm_handle),
                   ndb_mgm_get_latest_error_msg(m_mgm_handle),
                   ndb_mgm_get_latest_error_line(m_mgm_handle)
                   );
	      }
	    }
	    /** else
	     * We will not be able to get a new port unless
	     * the m_mgm_handle is connected. Note that not
	     * being connected is an ok state, just continue
	     * until it is able to connect. Continue using the
	     * old port until we can connect again and get a
	     * new port.
	     */
	  }
	}
	break;
      case DISCONNECTING:
	if(t->isConnected())
        {
          DEBUG_FPRINTF((stderr, "(%u)doDisconnect(%u), line: %u\n",
                         localNodeId, t->getRemoteNodeId(), __LINE__));
	  t->doDisconnect();
        }
	break;
      case DISCONNECTED:
      {
        if (t->isConnected())
        {
          g_eventLogger->warning("Found connection to %u in state DISCONNECTED "
                                 " while being connected, disconnecting!",
                                 t->getRemoteNodeId());
          DEBUG_FPRINTF((stderr, "(%u)doDisconnect(%u), line: %u\n",
                         localNodeId, t->getRemoteNodeId(), __LINE__));
          t->doDisconnect();
        }
        break;
      }
      case CONNECTED:
      default:
	break;
      }
    }
  }
  DBUG_VOID_RETURN;
}

struct NdbThread*
TransporterRegistry::start_clients()
{
  m_run_start_clients_thread= true;
  m_start_clients_thread= NdbThread_Create(run_start_clients_C,
					   (void**)this,
                                           0, // default stack size
					   "ndb_start_clients",
					   NDB_THREAD_PRIO_LOW);
  if (m_start_clients_thread == 0)
  {
    m_run_start_clients_thread= false;
  }
  return m_start_clients_thread;
}

bool
TransporterRegistry::stop_clients()
{
  if (m_start_clients_thread) {
    m_run_start_clients_thread= false;
    void* status;
    NdbThread_WaitFor(m_start_clients_thread, &status);
    NdbThread_Destroy(&m_start_clients_thread);
  }
  return true;
}

void
TransporterRegistry::add_transporter_interface(NodeId remoteNodeId,
					       const char *interf, 
					       int s_port)
{
  DBUG_ENTER("TransporterRegistry::add_transporter_interface");
  DBUG_PRINT("enter",("interface=%s, s_port= %d", interf, s_port));
  if (interf && strlen(interf) == 0)
    interf= 0;

  for (unsigned i= 0; i < m_transporter_interface.size(); i++)
  {
    Transporter_interface &tmp= m_transporter_interface[i];
    if (s_port != tmp.m_s_service_port || tmp.m_s_service_port==0)
      continue;
    if (interf != 0 && tmp.m_interface != 0 &&
	strcmp(interf, tmp.m_interface) == 0)
    {
      DBUG_VOID_RETURN; // found match, no need to insert
    }
    if (interf == 0 && tmp.m_interface == 0)
    {
      DBUG_VOID_RETURN; // found match, no need to insert
    }
  }
  Transporter_interface t;
  t.m_remote_nodeId= remoteNodeId;
  t.m_s_service_port= s_port;
  t.m_interface= interf;
  m_transporter_interface.push_back(t);
  DBUG_PRINT("exit",("interface and port added"));
  DBUG_VOID_RETURN;
}

bool
TransporterRegistry::start_service(SocketServer& socket_server)
{
  DBUG_ENTER("TransporterRegistry::start_service");
  if (m_transporter_interface.size() > 0 &&
      localNodeId == 0)
  {
    g_eventLogger->error("INTERNAL ERROR: not initialized");
    DBUG_RETURN(false);
  }

  for (unsigned i= 0; i < m_transporter_interface.size(); i++)
  {
    Transporter_interface &t= m_transporter_interface[i];

    unsigned short port= (unsigned short)t.m_s_service_port;
    if(t.m_s_service_port<0)
      port= -t.m_s_service_port; // is a dynamic port
    TransporterService *transporter_service =
      new TransporterService(new SocketAuthSimple("ndbd", "ndbd passwd"));
    if(!socket_server.setup(transporter_service,
			    &port, t.m_interface))
    {
      DBUG_PRINT("info", ("Trying new port"));
      port= 0;
      if(t.m_s_service_port>0
	 || !socket_server.setup(transporter_service,
				 &port, t.m_interface))
      {
	/*
	 * If it wasn't a dynamically allocated port, or
	 * our attempts at getting a new dynamic port failed
	 */
        g_eventLogger->error("Unable to setup transporter service port: %s:%d!\n"
                             "Please check if the port is already used,\n"
                             "(perhaps the node is already running)",
                             t.m_interface ? t.m_interface : "*", t.m_s_service_port);
	delete transporter_service;
	DBUG_RETURN(false);
      }
    }
    t.m_s_service_port= (t.m_s_service_port<=0)?-port:port; // -`ve if dynamic
    DBUG_PRINT("info", ("t.m_s_service_port = %d",t.m_s_service_port));
    transporter_service->setTransporterRegistry(this);
  }
  DBUG_RETURN(true);
}

void
TransporterRegistry::startReceiving()
{
  DBUG_ENTER("TransporterRegistry::startReceiving");

#ifndef WIN32
  m_shm_own_pid = getpid();
#endif
  DBUG_VOID_RETURN;
}

void
TransporterRegistry::stopReceiving(){
}

void
TransporterRegistry::startSending(){
}

void
TransporterRegistry::stopSending(){
}

NdbOut & operator <<(NdbOut & out, SignalHeader & sh){
  out << "-- Signal Header --" << endl;
  out << "theLength:    " << sh.theLength << endl;
  out << "gsn:          " << sh.theVerId_signalNumber << endl;
  out << "recBlockNo:   " << sh.theReceiversBlockNumber << endl;
  out << "sendBlockRef: " << sh.theSendersBlockRef << endl;
  out << "sendersSig:   " << sh.theSendersSignalId << endl;
  out << "theSignalId:  " << sh.theSignalId << endl;
  out << "trace:        " << (int)sh.theTrace << endl;
  return out;
} 

int
TransporterRegistry::get_transporter_count() const
{
  assert(nTransporters > 0);
  return nTransporters;
}

bool
TransporterRegistry::is_shm_transporter(NodeId nodeId)
{
  assert(nodeId < maxTransporters);
  Transporter *trp = theTransporters[nodeId];
  if (trp->getTransporterType() == tt_SHM_TRANSPORTER)
    return true;
  else
    return false;
}

Transporter*
TransporterRegistry::get_transporter(NodeId nodeId) const
{
  assert(nodeId < maxTransporters);
  return theTransporters[nodeId];
}


bool TransporterRegistry::connect_client(NdbMgmHandle *h)
{
  DBUG_ENTER("TransporterRegistry::connect_client(NdbMgmHandle)");

  Uint32 mgm_nodeid= ndb_mgm_get_mgmd_nodeid(*h);

  if(!mgm_nodeid)
  {
    g_eventLogger->error("%s: %d", __FILE__, __LINE__);
    return false;
  }
  Transporter * t = theTransporters[mgm_nodeid];
  if (!t)
  {
    g_eventLogger->error("%s: %d", __FILE__, __LINE__);
    return false;
  }

  bool res = t->connect_client(connect_ndb_mgmd(h));
  if (res == true)
  {
    performStates[mgm_nodeid] = TransporterRegistry::CONNECTING;
  }
  DBUG_RETURN(res);
}


bool TransporterRegistry::report_dynamic_ports(NdbMgmHandle h) const
{
  // Fill array of nodeid/port pairs for those ports which are dynamic
  unsigned num_ports = 0;
  ndb_mgm_dynamic_port ports[MAX_NODES];
  for(unsigned i = 0; i < m_transporter_interface.size(); i++)
  {
    const Transporter_interface& ti = m_transporter_interface[i];
    if (ti.m_s_service_port >= 0)
      continue; // Not a dynamic port

    assert(num_ports < NDB_ARRAY_SIZE(ports));
    ports[num_ports].nodeid = ti.m_remote_nodeId;
    ports[num_ports].port = ti.m_s_service_port;
    num_ports++;
  }

  if (num_ports == 0)
  {
    // No dynamic ports in use, nothing to report
    return true;
  }

  // Send array of nodeid/port pairs to mgmd
  if (ndb_mgm_set_dynamic_ports(h, localNodeId,
                                ports, num_ports) < 0)
  {
    g_eventLogger->error("Failed to register dynamic ports, error: %d  - '%s'",
                         ndb_mgm_get_latest_error(h),
                         ndb_mgm_get_latest_error_desc(h));
    return false;
  }

  return true;
}


/**
 * Given a connected NdbMgmHandle, turns it into a transporter
 * and returns the socket.
 */
NDB_SOCKET_TYPE TransporterRegistry::connect_ndb_mgmd(NdbMgmHandle *h)
{
  NDB_SOCKET_TYPE sockfd;
  ndb_socket_invalidate(&sockfd);

  DBUG_ENTER("TransporterRegistry::connect_ndb_mgmd(NdbMgmHandle)");

  if ( h==NULL || *h == NULL )
  {
    g_eventLogger->error("Mgm handle is NULL (%s:%d)", __FILE__, __LINE__);
    DBUG_RETURN(sockfd);
  }

  if (!report_dynamic_ports(*h))
  {
    ndb_mgm_destroy_handle(h);
    DBUG_RETURN(sockfd);
  }

  /**
   * convert_to_transporter also disposes of the handle (i.e. we don't leak
   * memory here.
   */
  DBUG_PRINT("info", ("Converting handle to transporter"));
  sockfd= ndb_mgm_convert_to_transporter(h);
  if (!ndb_socket_valid(sockfd))
  {
    g_eventLogger->error("Failed to convert to transporter (%s: %d)",
                         __FILE__, __LINE__);
    ndb_mgm_destroy_handle(h);
  }
  DBUG_RETURN(sockfd);
}

/**
 * Given a SocketClient, creates a NdbMgmHandle, turns it into a transporter
 * and returns the socket.
 */
NDB_SOCKET_TYPE
TransporterRegistry::connect_ndb_mgmd(const char* server_name,
                                      unsigned short server_port)
{
  NdbMgmHandle h= ndb_mgm_create_handle();
  NDB_SOCKET_TYPE s;
  ndb_socket_invalidate(&s);

  DBUG_ENTER("TransporterRegistry::connect_ndb_mgmd(SocketClient)");

  if ( h == NULL )
  {
    DBUG_RETURN(s);
  }

  /**
   * Set connectstring
   */
  {
    BaseString cs;
    cs.assfmt("%s:%u", server_name, server_port);
    ndb_mgm_set_connectstring(h, cs.c_str());
  }

  if(ndb_mgm_connect(h, 0, 0, 0)<0)
  {
    DBUG_PRINT("info", ("connection to mgmd failed"));
    ndb_mgm_destroy_handle(&h);
    DBUG_RETURN(s);
  }

  DBUG_RETURN(connect_ndb_mgmd(&h));
}

/**
 * The calls below are used by all implementations: NDB API, ndbd and
 * ndbmtd. The calls to handle->getWritePtr, handle->updateWritePtr
 * are handled by special implementations for NDB API, ndbd and
 * ndbmtd.
 */

Uint32 *
TransporterRegistry::getWritePtr(TransporterSendBufferHandle *handle,
                                 NodeId node,
                                 Uint32 lenBytes,
                                 Uint32 prio,
                                 SendStatus *error)
{
  Transporter *t = theTransporters[node];
  Uint32 *insertPtr = handle->getWritePtr(node,
                                          lenBytes,
                                          prio,
                                          t->get_max_send_buffer(),
                                          error);

  if (unlikely(insertPtr == nullptr && *error != SEND_MESSAGE_TOO_BIG))
  {
    //-------------------------------------------------
    // Buffer was completely full. We have severe problems.
    // We will attempt to wait for a small time
    //-------------------------------------------------
    if(t->send_is_possible(10)) {
      //-------------------------------------------------
      // Send is possible after the small timeout.
      //-------------------------------------------------
      if(!handle->forceSend(node)){
	return 0;
      } else {
	//-------------------------------------------------
	// Since send was successful we will make a renewed
	// attempt at inserting the signal into the buffer.
	//-------------------------------------------------
        insertPtr = handle->getWritePtr(node,
                                        lenBytes,
                                        prio,
                                        t->get_max_send_buffer(),
                                        error);
      }//if
    } else {
      return 0;
    }//if
  }
  return insertPtr;
}

void
TransporterRegistry::updateWritePtr(TransporterSendBufferHandle *handle,
                                    NodeId node, Uint32 lenBytes, Uint32 prio)
{
  Transporter *t = theTransporters[node];

  Uint32 used = handle->updateWritePtr(node, lenBytes, prio);
  t->update_status_overloaded(used);

  if(t->send_limit_reached(used)) {
    //-------------------------------------------------
    // Buffer is full and we are ready to send. We will
    // not wait since the signal is already in the buffer.
    // Force flag set has the same indication that we
    // should always send. If it is not possible to send
    // we will not worry since we will soon be back for
    // a renewed trial.
    //-------------------------------------------------
    if(t->send_is_possible(0)) {
      //-------------------------------------------------
      // Send was possible, attempt at a send.
      //-------------------------------------------------
      handle->forceSend(node);
    }//if
  }
}
	   
void 
TransporterRegistry::inc_overload_count(Uint32 nodeId)
{
  assert(nodeId < MAX_NODES);
  assert(theTransporters[nodeId] != NULL);
  theTransporters[nodeId]->inc_overload_count();
}

void 
TransporterRegistry::inc_slowdown_count(Uint32 nodeId)
{
  assert(nodeId < MAX_NODES);
  assert(theTransporters[nodeId] != NULL);
  theTransporters[nodeId]->inc_slowdown_count();
}

Uint32
TransporterRegistry::get_overload_count(Uint32 nodeId)
{
  assert(nodeId < MAX_NODES);
  assert(theTransporters[nodeId] != NULL);
  return theTransporters[nodeId]->get_overload_count();
}

Uint32
TransporterRegistry::get_slowdown_count(Uint32 nodeId)
{
  assert(nodeId < MAX_NODES);
  assert(theTransporters[nodeId] != NULL);
  return theTransporters[nodeId]->get_slowdown_count();
}

Uint32
TransporterRegistry::get_connect_count(Uint32 nodeId)
{
  assert(nodeId < MAX_NODES);
  assert(theTransporters[nodeId] != NULL);
  return theTransporters[nodeId]->get_connect_count();
}

/**
 * We calculate the risk level for a send buffer.
 * The primary instrument is the current size of
 * the node send buffer. However if the total
 * buffer for all send buffers is also close to
 * empty, then we will adjust the node send
 * buffer size for this. In this manner a very
 * contested total buffer will also slow down
 * the entire node operation.
 */
void
calculate_send_buffer_level(Uint64 node_send_buffer_size,
                            Uint64 total_send_buffer_size,
                            Uint64 total_used_send_buffer_size,
                            Uint32 num_threads,
                            SB_LevelType &level)
{
  Uint64 percentage =
    (total_used_send_buffer_size * 100) / total_send_buffer_size;

  if (percentage < 90)
  {
    ;
  }
  else if (percentage < 95)
  {
    node_send_buffer_size *= 2;
  }
  else if (percentage < 97)
  {
    node_send_buffer_size *= 4;
  }
  else if (percentage < 98)
  {
    node_send_buffer_size *= 8;
  }
  else if (percentage < 99)
  {
    node_send_buffer_size *= 16;
  }
  else
  {
    level = SB_CRITICAL_LEVEL;
    return;
  }
  
  if (node_send_buffer_size < 128 * 1024)
  {
    level = SB_NO_RISK_LEVEL;
    return;
  }
  else if (node_send_buffer_size < 256 * 1024)
  {
    level = SB_LOW_LEVEL;
    return;
  }
  else if (node_send_buffer_size < 384 * 1024)
  {
    level = SB_MEDIUM_LEVEL;
    return;
  }
  else if (node_send_buffer_size < 1024 * 1024)
  {
    level = SB_HIGH_LEVEL;
    return;
  }
  else if (node_send_buffer_size < 2 * 1024 * 1024)
  {
    level = SB_RISK_LEVEL;
    return;
  }
  else
  {
    level = SB_CRITICAL_LEVEL;
    return;
  }
}

template class Vector<TransporterRegistry::Transporter_interface>;
