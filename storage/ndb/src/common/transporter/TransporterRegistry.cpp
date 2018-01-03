/*
   Copyright (c) 2003, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifdef NDB_TCP_TRANSPORTER
#include "TCP_Transporter.hpp"
#include "Loopback_Transporter.hpp"
#endif

#ifdef NDB_SCI_TRANSPORTER
#include "SCI_Transporter.hpp"
#endif

#ifdef NDB_SHM_TRANSPORTER
#include "SHM_Transporter.hpp"
extern int g_ndb_shm_signum;
#endif

#include "NdbOut.hpp"
#include <NdbSleep.h>
#include <InputStream.hpp>
#include <OutputStream.hpp>
#include <socket_io.h>

#include <mgmapi/mgmapi.h>
#include <mgmapi_internal.h>
#include <mgmapi/mgmapi_debug.h>

#include <EventLogger.hpp>
extern EventLogger * g_eventLogger;

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
  bzero(m_epoll_events, maxTransporters * sizeof(struct epoll_event));
  return true;
fallback:
#endif
  return m_socket_poller.set_max_count(maxTransporters);
}

bool
TransporterReceiveData::epoll_add(TCP_Transporter *t)
{
  assert(m_transporters.get(t->getRemoteNodeId()));
#if defined(HAVE_EPOLL_CREATE)
  if (m_epoll_fd != -1)
  {
    bool add = true;
    struct epoll_event event_poll;
    bzero(&event_poll, sizeof(event_poll));
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
                                         TransporterReceiveHandle * recvHandle,
                                         bool use_default_send_buffer,
                                         unsigned _maxTransporters) :
  m_mgm_handle(0),
  localNodeId(0),
  connectBackoffMaxTime(0),
  m_transp_count(0),
  m_use_default_send_buffer(use_default_send_buffer),
  m_send_buffers(0), m_page_freelist(0), m_send_buffer_memory(0),
  m_total_max_send_buffer(0)
{
  DBUG_ENTER("TransporterRegistry::TransporterRegistry");

  receiveHandle = recvHandle;
  maxTransporters = _maxTransporters;
  sendCounter = 1;
  
  callbackObj=callback;

  theTCPTransporters  = new TCP_Transporter * [maxTransporters];
  theSCITransporters  = new SCI_Transporter * [maxTransporters];
  theSHMTransporters  = new SHM_Transporter * [maxTransporters];
  theTransporterTypes = new TransporterType   [maxTransporters];
  theTransporters     = new Transporter     * [maxTransporters];
  performStates       = new PerformState      [maxTransporters];
  ioStates            = new IOState           [maxTransporters]; 
  peerUpIndicators    = new bool              [maxTransporters];
  connectingTime      = new Uint32            [maxTransporters];
  m_disconnect_errnum = new int               [maxTransporters];
  m_error_states      = new ErrorState        [maxTransporters];

  m_has_extra_wakeup_socket = false;

#ifdef ERROR_INSERT
  m_blocked.clear();
  m_blocked_disconnected.clear();
  m_sendBlocked.clear();

  m_mixology_level = 0;
#endif

  // Initialize member variables
  nTransporters    = 0;
  nTCPTransporters = 0;
  nSCITransporters = 0;
  nSHMTransporters = 0;
  
  // Initialize the transporter arrays
  ErrorState default_error_state = { TE_NO_ERROR, (const char *)~(UintPtr)0 };
  for (unsigned i=0; i<maxTransporters; i++) {
    theTCPTransporters[i] = NULL;
    theSCITransporters[i] = NULL;
    theSHMTransporters[i] = NULL;
    theTransporters[i]    = NULL;
    performStates[i]      = DISCONNECTED;
    ioStates[i]           = NoHalt;
    peerUpIndicators[i]   = true; // Assume all nodes are up, will be
                                  // cleared at first connect attempt
    connectingTime[i]     = 0;
    m_disconnect_errnum[i]= 0;
    m_error_states[i]     = default_error_state;
  }

  DBUG_VOID_RETURN;
}

#define MIN_SEND_BUFFER_SIZE (4 * 1024 * 1024)

void
TransporterRegistry::allocate_send_buffers(Uint64 total_send_buffer,
                                           Uint64 extra_send_buffer)
{
  if (!m_use_default_send_buffer)
    return;

  if (total_send_buffer == 0)
    total_send_buffer = get_total_max_send_buffer();

  total_send_buffer += extra_send_buffer;

  if (!extra_send_buffer)
  {
    /**
     * If extra send buffer memory is 0 it means we can decide on an
     * appropriate value for it. We select to always ensure that the
     * minimum send buffer memory is 4M, otherwise we simply don't
     * add any extra send buffer memory at all.
     */
    if (total_send_buffer < MIN_SEND_BUFFER_SIZE)
    {
      total_send_buffer = (Uint64)MIN_SEND_BUFFER_SIZE;
    }
  }

  if (m_send_buffers)
  {
    /* Send buffers already allocated -> resize the buffer pages */
    assert(m_send_buffer_memory);

    // TODO resize send buffer pages

    return;
  }

  /* Initialize transporter send buffers (initially empty). */
  m_send_buffers = new SendBuffer[maxTransporters];
  for (unsigned i = 0; i < maxTransporters; i++)
  {
    SendBuffer &b = m_send_buffers[i];
    b.m_first_page = NULL;
    b.m_last_page = NULL;
    b.m_used_bytes = 0;
  }

  /* Initialize the page freelist. */
  Uint64 send_buffer_pages =
    (total_send_buffer + SendBufferPage::PGSIZE - 1)/SendBufferPage::PGSIZE;
  /* Add one extra page of internal fragmentation overhead per transporter. */
  send_buffer_pages += nTransporters;

  m_send_buffer_memory =
    new unsigned char[UintPtr(send_buffer_pages * SendBufferPage::PGSIZE)];
  if (m_send_buffer_memory == NULL)
  {
    ndbout << "Unable to allocate "
           << send_buffer_pages * SendBufferPage::PGSIZE
           << " bytes of memory for send buffers, aborting." << endl;
    abort();
  }

  m_page_freelist = NULL;
  for (unsigned i = 0; i < send_buffer_pages; i++)
  {
    SendBufferPage *page =
      (SendBufferPage *)(m_send_buffer_memory + i * SendBufferPage::PGSIZE);
    page->m_bytes = 0;
    page->m_next = m_page_freelist;
    m_page_freelist = page;
  }
  m_tot_send_buffer_memory = SendBufferPage::PGSIZE * send_buffer_pages;
  m_tot_used_buffer_memory = 0;
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
  
  delete[] theTCPTransporters;
  delete[] theSCITransporters;
  delete[] theSHMTransporters;
  delete[] theTransporterTypes;
  delete[] theTransporters;
  delete[] performStates;
  delete[] ioStates;
  delete[] peerUpIndicators;
  delete[] connectingTime;
  delete[] m_disconnect_errnum;
  delete[] m_error_states;

  if (m_send_buffers)
    delete[] m_send_buffers;
  m_page_freelist = NULL;
  if (m_send_buffer_memory)
    delete[] m_send_buffer_memory;

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
  bool res = t->connect_server(sockfd, msg);

  if (res && performStates[nodeId] != TransporterRegistry::CONNECTING)
  {
    msg.assfmt("line: %u : Incorrect state for node %u state: %s (%u)",
               __LINE__, nodeId,
               getPerformStateString(nodeId),
               performStates[nodeId]);
    // Connection suceeded, but not connecting anymore, return
    // false to close the connection
    DBUG_RETURN(false);
  }

  DBUG_RETURN(res);
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
  case tt_SCI_TRANSPORTER:
    return createSCITransporter(config);
  default:
    abort();
    break;
  }
  return false;
}


bool
TransporterRegistry::createTCPTransporter(TransporterConfiguration *config) {
#ifdef NDB_TCP_TRANSPORTER

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
  theTCPTransporters[nTCPTransporters]      = t;
  theTransporters[t->getRemoteNodeId()]     = t;
  theTransporterTypes[t->getRemoteNodeId()] = tt_TCP_TRANSPORTER;
  performStates[t->getRemoteNodeId()]       = DISCONNECTED;
  nTransporters++;
  nTCPTransporters++;
  m_total_max_send_buffer += t->get_max_send_buffer();

  return true;
#else
  return false;
#endif
}

bool
TransporterRegistry::createSCITransporter(TransporterConfiguration *config) {
#ifdef NDB_SCI_TRANSPORTER

  if(!SCI_Transporter::initSCI())
    abort();

  SCI_Transporter * t = new SCI_Transporter(*this,
                                            config->localHostName,
                                            config->remoteHostName,
                                            config->s_port,
					    config->isMgmConnection,
                                            config->sci.sendLimit, 
					    config->sci.bufferSize,
					    config->sci.nLocalAdapters,
					    config->sci.remoteSciNodeId0,
					    config->sci.remoteSciNodeId1,
					    localNodeId,
					    config->remoteNodeId,
					    config->serverNodeId,
					    config->checksum,
					    config->signalId,
					    config->preSendChecksum);
  
  if (t == NULL) 
    return false;
  else if (!t->initTransporter()) {
    delete t;
    return false;
  }
  // Put the transporter in the transporter arrays
  theSCITransporters[nSCITransporters]      = t;
  theTransporters[t->getRemoteNodeId()]     = t;
  theTransporterTypes[t->getRemoteNodeId()] = tt_SCI_TRANSPORTER;
  performStates[t->getRemoteNodeId()]       = DISCONNECTED;
  nTransporters++;
  nSCITransporters++;
  m_total_max_send_buffer += t->get_max_send_buffer();
  
  return true;
#else
  return false;
#endif
}

bool
TransporterRegistry::createSHMTransporter(TransporterConfiguration *config) {
  DBUG_ENTER("TransporterRegistry::createTransporter SHM");
#ifdef NDB_SHM_TRANSPORTER

  if (!g_ndb_shm_signum) {
    g_ndb_shm_signum= config->shm.signum;
    DBUG_PRINT("info",("Block signum %d",g_ndb_shm_signum));
    /**
     * Make sure to block g_ndb_shm_signum
     *   TransporterRegistry::init is run from "main" thread
     */
    NdbThread_set_shm_sigmask(TRUE);
  }

  if(config->shm.signum != g_ndb_shm_signum)
    return false;

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
					    config->preSendChecksum
					    );
  if (t == NULL)
    return false;
  else if (!t->initTransporter()) {
    delete t;
    return false;
  }
  // Put the transporter in the transporter arrays
  theSHMTransporters[nSHMTransporters]      = t;
  theTransporters[t->getRemoteNodeId()]     = t;
  theTransporterTypes[t->getRemoteNodeId()] = tt_SHM_TRANSPORTER;
  performStates[t->getRemoteNodeId()]       = DISCONNECTED;
  
  nTransporters++;
  nSHMTransporters++;
  m_total_max_send_buffer += t->get_max_send_buffer();

  DBUG_RETURN(true);
#else
  DBUG_RETURN(false);
#endif
}


void
TransporterRegistry::removeTransporter(NodeId nodeId) {

  DEBUG("Removing transporter from " << localNodeId
	<< " to " << nodeId);
  
  if(theTransporters[nodeId] == NULL)
    return;
  
  theTransporters[nodeId]->doDisconnect();
  
  const TransporterType type = theTransporterTypes[nodeId];

  int ind = 0;
  switch(type){
  case tt_TCP_TRANSPORTER:
#ifdef NDB_TCP_TRANSPORTER
    for(; ind < nTCPTransporters; ind++)
      if(theTCPTransporters[ind]->getRemoteNodeId() == nodeId)
	break;
    ind++;
    for(; ind<nTCPTransporters; ind++)
      theTCPTransporters[ind-1] = theTCPTransporters[ind];
    nTCPTransporters --;
#endif
    break;
  case tt_SCI_TRANSPORTER:
#ifdef NDB_SCI_TRANSPORTER
    for(; ind < nSCITransporters; ind++)
      if(theSCITransporters[ind]->getRemoteNodeId() == nodeId)
	break;
    ind++;
    for(; ind<nSCITransporters; ind++)
      theSCITransporters[ind-1] = theSCITransporters[ind];
    nSCITransporters --;
#endif
    break;
  case tt_SHM_TRANSPORTER:
#ifdef NDB_SHM_TRANSPORTER
    for(; ind < nSHMTransporters; ind++)
      if(theSHMTransporters[ind]->getRemoteNodeId() == nodeId)
	break;
    ind++;
    for(; ind<nSHMTransporters; ind++)
      theSHMTransporters[ind-1] = theSHMTransporters[ind];
    nSHMTransporters --;
#endif
    break;
  }

  nTransporters--;

  // Delete the transporter and remove it from theTransporters array
  delete theTransporters[nodeId];
  theTransporters[nodeId] = NULL;        
}


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
  if(t != NULL && 
     (((ioStates[nodeId] != HaltOutput) && (ioStates[nodeId] != HaltIO)) || 
      ((signalHeader->theReceiversBlockNumber == 252) ||
       (signalHeader->theReceiversBlockNumber == 4002)))) {
	 
    if(t->isConnected()){
      Uint32 lenBytes = t->m_packer.getMessageLength(signalHeader, section.m_ptr);
      if(lenBytes <= MAX_SEND_MESSAGE_BYTESIZE){
	Uint32 * insertPtr = getWritePtr(sendHandle, nodeId, lenBytes, prio);
	if(insertPtr != 0){
	  t->m_packer.pack(insertPtr, prio, signalHeader, signalData, section);
	  updateWritePtr(sendHandle, nodeId, lenBytes, prio);
	  return SEND_OK;
	}

        set_status_overloaded(nodeId, true);
        int sleepTime = 2;

	/**
	 * @note: on linux/i386 the granularity is 10ms
	 *        so sleepTime = 2 generates a 10 ms sleep.
	 */
	for(int i = 0; i<50; i++){
	  if((nSHMTransporters+nSCITransporters) == 0)
	    NdbSleep_MilliSleep(sleepTime);
          /* FC : Consider counting sleeps here */
	  insertPtr = getWritePtr(sendHandle, nodeId, lenBytes, prio);
	  if(insertPtr != 0){
	    t->m_packer.pack(insertPtr, prio, signalHeader, signalData, section);
	    updateWritePtr(sendHandle, nodeId, lenBytes, prio);
	    break;
	  }
	}
	
	if(insertPtr != 0){
	  /**
	   * Send buffer full, but resend works
	   */
	  report_error(nodeId, TE_SEND_BUFFER_FULL);
	  return SEND_OK;
	}
	
	WARNING("Signal to " << nodeId << " lost(buffer)");
	report_error(nodeId, TE_SIGNAL_LOST_SEND_BUFFER_FULL);
	return SEND_BUFFER_FULL;
      } else {
  g_eventLogger->info("Send message too big");
	return SEND_MESSAGE_TOO_BIG;
      }
    } else {
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
  } else {
    DEBUG("Discarding message to block: " 
	  << signalHeader->theReceiversBlockNumber 
	  << " node: " << nodeId);
    
    if(t == NULL)
      return SEND_UNKNOWN_NODE;
    
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
    bzero(&event_poll, sizeof(event_poll));
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
TransporterRegistry::pollReceive(Uint32 timeOutMillis,
                                 TransporterReceiveHandle& recvdata)
{
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

  if (nSCITransporters > 0)
  {
    timeOutMillis=0;
  }

#ifdef NDB_SHM_TRANSPORTER
  if (nSHMTransporters > 0)
  {
    Uint32 res = poll_SHM(0, recvdata);
    if(res)
    {
      retVal |= res;
      timeOutMillis = 0;
    }
  }
#endif

#ifdef NDB_TCP_TRANSPORTER
#if defined(HAVE_EPOLL_CREATE)
  if (likely(recvdata.m_epoll_fd != -1))
  {
    int tcpReadSelectReply = 0;
    Uint32 num_trps = nTCPTransporters + (m_has_extra_wakeup_socket ? 1 : 0);

    if (num_trps)
    {
      tcpReadSelectReply = epoll_wait(recvdata.m_epoll_fd,
                                      recvdata.m_epoll_events,
                                      num_trps, timeOutMillis);
      retVal |= tcpReadSelectReply;
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
    if (nTCPTransporters > 0 || m_has_extra_wakeup_socket)
    {
      retVal |= poll_TCP(timeOutMillis, recvdata);
    }
  }
#endif
#ifdef NDB_SCI_TRANSPORTER
  if (nSCITransporters > 0)
    retVal |= poll_SCI(timeOutMillis, recvdata);
#endif
#ifdef NDB_SHM_TRANSPORTER
  if (nSHMTransporters > 0)
  {
    int res = poll_SHM(0, recvdata);
    retVal |= res;
  }
#endif
  return retVal;
}


#ifdef NDB_SCI_TRANSPORTER
Uint32
TransporterRegistry::poll_SCI(Uint32 timeOutMillis,
                              TransporterReceiveHandle& recvdata)
{
  assert((receiveHandle == &recvdata) || (receiveHandle == 0));

  Uint32 retVal = 0;
  for (int i = 0; i < nSCITransporters; i++)
  {
    SCI_Transporter * t = theSCITransporters[i];
    Uint32 node_id = t->getRemoteNodeId();

    if (!recvdata.m_transporters.get(nodeId))
      continue;

    if (t->isConnected() && is_connected(node_id))
    {
      if (t->hasDataToRead())
      {
        recvdata.m_has_data_transporters.set(node_id);
	retVal = 1;
      }
    }
  }
  return retVal;
}
#endif


#ifdef NDB_SHM_TRANSPORTER
static int g_shm_counter = 0;
Uint32
TransporterRegistry::poll_SHM(Uint32 timeOutMillis,
                              TransporterReceiveHandle& recvdata)
{
  assert((receiveHandle == &recvdata) || (receiveHandle == 0));

  Uint32 retVal = 0;
  for (int j = 0; j < 100; j++)
  {
    for (int i = 0; i<nSHMTransporters; i++)
    {
      SHM_Transporter * t = theSHMTransporters[i];
      Uint32 node_id = t->getRemoteNodeId();

      if (!recvdata.m_transporters.get(node_id))
        continue;

      if (t->isConnected() && is_connected(node_id))
      {
	if (t->hasDataToRead())
        {
          j = 100;
          recvdata.m_has_data_transporters.set(node_id);
          retVal = 1;
	}
      }
    }
  }
  return retVal;
}
#endif

#ifdef NDB_TCP_TRANSPORTER
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
  for (int i = 0; i < nTCPTransporters; i++)
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

    for (int i = 0; i < nTCPTransporters; i++)
    {
      TCP_Transporter * t = theTCPTransporters[i];
      if (idx[i] != MAX_NODES + 1)
      {
        Uint32 node_id = t->getRemoteNodeId();
        if (recvdata.m_socket_poller.has_read(idx[i]))
          recvdata.m_recv_transporters.set(node_id);
      }
    }
  }

  return tcpReadSelectReply;
}
#endif

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

#ifdef NDB_TCP_TRANSPORTER
  /**
   * Receive data from transporters polled to have data.
   * Add to set of transporters having pending data.
   */
  for(Uint32 id = recvdata.m_recv_transporters.find_first();
      id != BitmaskImpl::NotFound;
      id = recvdata.m_recv_transporters.find_next(id + 1))
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
  recvdata.m_recv_transporters.clear();

  /**
   * Unpack data either received above or pending from prev rounds.
   *
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
    TCP_Transporter * t = (TCP_Transporter*)theTransporters[id];

    assert(recvdata.m_transporters.get(id));

    if (is_connected(id))
    {
      if (t->isConnected())
      {
        if (unlikely(recvdata.checkJobBuffer()))
          return 1;     // Full, can't unpack more
        if (unlikely(recvdata.m_handled_transporters.get(id)))
          continue;     // Skip now to avoid starvation
        Uint32 * ptr;
        Uint32 sz = t->getReceiveData(&ptr);
        Uint32 szUsed = unpack(recvdata, ptr, sz, id, ioStates[id], stopReceiving);
        if (likely(szUsed))
        {
          t->updateReceiveDataPtr(szUsed);
          hasdata = t->hasReceiveData();
        }
        // else, we didn't unpack anything:
        //   Avail ReceiveData to short to be usefull, need to
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
#endif
  
#ifdef NDB_SCI_TRANSPORTER
  //performReceive
  //do prepareReceive on the SCI transporters  (prepareReceive(t,,,,))
  for (int i=0; i<nSCITransporters && !stopReceiving; i++)
  {
    SCI_Transporter  *t = theSCITransporters[i];
    const NodeId nodeId = t->getRemoteNodeId();
    assert(recvdata.m_transporters.get(nodeId));
    if(is_connected(nodeId))
    {
      if(t->isConnected() && t->checkConnected())
      {
        if (unlikely(recvdata.checkJobBuffer()))
          return 1;      // Full, can't unpack more
        if (unlikely(recvdata.m_handled_transporters.get(nodeId)))
          continue;      // Skip now to avoid starvation

        Uint32 * readPtr, * eodPtr;
        t->getReceivePtr(&readPtr, &eodPtr);
        callbackObj->transporter_recv_from(nodeId);
        Uint32 *newPtr = unpack(recvdata, readPtr, eodPtr, nodeId, ioStates[nodeId], stopReceiving);
        t->updateReceivePtr(newPtr);
      }
    } 
    recvdata.m_handled_transporters.set(nodeId);
  }
#endif
#ifdef NDB_SHM_TRANSPORTER
  for (int i=0; i<nSHMTransporters && !stopReceiving; i++)
  {
    SHM_Transporter *t = theSHMTransporters[i];
    const NodeId nodeId = t->getRemoteNodeId();
    assert(recvdata.m_transporters.get(nodeId));
    if(is_connected(nodeId)){
      if(t->isConnected() && t->checkConnected())
      {
        if (unlikely(recvdata.checkJobBuffer()))
          return 1;      // Full, can't unpack more
        if (unlikely(recvdata.m_handled_transporters.get(nodeId)))
          continue;      // Previously handled, skip to avoid starvation

        Uint32 * readPtr, * eodPtr;
        t->getReceivePtr(&readPtr, &eodPtr);
        recvdata.transporter_recv_from(nodeId);
        Uint32 *newPtr = unpack(recvdata,
                                readPtr, eodPtr, nodeId, ioStates[nodeId],
				stopReceiving);
        t->updateReceivePtr(newPtr);
      }
    } 
    recvdata.m_handled_transporters.set(nodeId);
  }
#endif
  recvdata.m_handled_transporters.clear();
  recvdata.m_last_nodeId = 0;
  return 0;
}

/**
 * In multi-threaded cases, this must be protected by send lock (can use
 * different locks for each node).
 */
bool
TransporterRegistry::performSend(NodeId nodeId)
{
  Transporter *t = get_transporter(nodeId);
  if (t && t->isConnected() && is_connected(nodeId))
  {
#ifdef ERROR_INSERT
    if (m_sendBlocked.get(nodeId))
    {
      return true;
    }
#endif

    return t->doSend();
  }

  return false;
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

void
TransporterRegistry::performSend()
{
  int i; 
  sendCounter = 1;

#ifdef NDB_TCP_TRANSPORTER
  for (i = m_transp_count; i < nTCPTransporters; i++) 
  {
    TCP_Transporter *t = theTCPTransporters[i];
    if (t && t->has_data_to_send() &&
        t->isConnected() && is_connected(t->getRemoteNodeId())
#ifdef ERROR_INSERT
        && !m_sendBlocked.get(t->getRemoteNodeId())
#endif
        )
    {
      t->doSend();
    }
  }
  for (i = 0; i < m_transp_count && i < nTCPTransporters; i++) 
  {
    TCP_Transporter *t = theTCPTransporters[i];
    if (t && t->has_data_to_send() &&
        t->isConnected() && is_connected(t->getRemoteNodeId())
#ifdef ERROR_INSERT
        && !m_sendBlocked.get(t->getRemoteNodeId())
#endif
        )
    {
      t->doSend();
    }
  }
  m_transp_count++;
  if (m_transp_count == nTCPTransporters) m_transp_count = 0;
#endif
#ifdef NDB_SCI_TRANSPORTER
  //scroll through the SCI transporters, 
  // get each transporter, check if connected, send data
  for (i=0; i<nSCITransporters; i++) {
    SCI_Transporter  *t = theSCITransporters[i];
    const NodeId nodeId = t->getRemoteNodeId();
    
    if(is_connected(nodeId))
    {
      if(t->isConnected() && t->has_data_to_send()
#ifdef ERROR_INSERT
        && !m_sendBlocked.get(t->getRemoteNodeId())
#endif
        )
      {
	t->doSend();
      } //if
    } //if
  }
#endif
  
#ifdef NDB_SHM_TRANSPORTER
  for (i=0; i<nSHMTransporters; i++) 
  {
    SHM_Transporter  *t = theSHMTransporters[i];
    const NodeId nodeId = t->getRemoteNodeId();
    if(is_connected(nodeId))
    {
      if(t->isConnected()
#ifdef ERROR_INSERT
        && !m_sendBlocked.get(t->getRemoteNodeId())
#endif
         )
      {
	t->doSend();
      }
    }
  }
#endif
}

int
TransporterRegistry::forceSendCheck(int sendLimit){
  int tSendCounter = sendCounter;
  sendCounter = tSendCounter + 1;
  if (tSendCounter >= sendLimit) {
    performSend();
    sendCounter = 1;
    return 1;
  }//if
  return 0;
}//TransporterRegistry::forceSendCheck()

#ifdef DEBUG_TRANSPORTER
void
TransporterRegistry::printState(){
  ndbout << "-- TransporterRegistry -- " << endl << endl
	 << "Transporters = " << nTransporters << endl;
  for(int i = 0; i<maxTransporters; i++)
    if(theTransporters[i] != NULL){
      const NodeId remoteNodeId = theTransporters[i]->getRemoteNodeId();
      ndbout << "Transporter: " << remoteNodeId 
	     << " PerformState: " << performStates[remoteNodeId]
	     << " IOState: " << ioStates[remoteNodeId] << endl;
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
     * a possile race with ::update_connections(): It will
     * see either of the *ING states, and bring the connection
     * into CONNECTED or *DISCONNECTED* state. Furthermore, the
     * state may be overwritten to CONNECTING by this method.
     * We should probably have waited for DISCONNECTED state,
     * before allowing reCONNECTING ....
     */
    break;
  }
  DBUG_ENTER("TransporterRegistry::do_connect");
  DBUG_PRINT("info",("performStates[%d]=CONNECTING",node_id));

  Transporter * t = theTransporters[node_id];
  if (t != NULL)
    t->resetBuffers();

  curr_state= CONNECTING;
  DBUG_VOID_RETURN;
}

/**
 * This method is used to initiate disconnect from TRPMAN. It is also called
 * from the TCP transporter in case of an I/O error on the socket.
 *
 * This works asynchronously, similar to do_connect().
 */
void
TransporterRegistry::do_disconnect(NodeId node_id, int errnum)
{
  PerformState &curr_state = performStates[node_id];
  switch(curr_state){
  case DISCONNECTED:
    return;
  case CONNECTED:
    break;
  case CONNECTING:
    break;
  case DISCONNECTING:
    return;
  }
  DBUG_ENTER("TransporterRegistry::do_disconnect");
  DBUG_PRINT("info",("performStates[%d]=DISCONNECTING",node_id));
  curr_state= DISCONNECTING;
  m_disconnect_errnum[node_id] = errnum;
  DBUG_VOID_RETURN;
}

void
TransporterRegistry::report_connect(TransporterReceiveHandle& recvdata,
                                    NodeId node_id)
{
  assert((receiveHandle == &recvdata) || (receiveHandle == 0));
  assert(recvdata.m_transporters.get(node_id));

  DBUG_ENTER("TransporterRegistry::report_connect");
  DBUG_PRINT("info",("performStates[%d]=CONNECTED",node_id));

  /*
    The send buffers was reset when this connection
    was set to DISCONNECTED. In order to make sure no stray
    signals has been written to the send buffer since then
    check that the send buffers still are empty.
  */
  /*
    OJA: Bug#24444908 has been reported related to this
    assert being hit. That should indeed be further
    investigated, but as assert created a problem for testing,
    we decided to turn it of for now.
  */
  //assert(!callbackObj->has_data_to_send(node_id));

  if (recvdata.epoll_add((TCP_Transporter*)theTransporters[node_id]))
  {
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

  /*
    No one else should be using the transporter now,
    reset its send buffer and recvdata
   */
  callbackObj->reset_send_buffer(node_id);
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
  TransporterReceiveWatchdog guard(recvdata);
  assert((receiveHandle == &recvdata) || (receiveHandle == 0));

  for (int i= 0, n= 0; n < nTransporters; i++){
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
      recvdata.reportError(nodeId, code, info);
      m_error_states[nodeId].m_code = TE_NO_ERROR;
      m_error_states[nodeId].m_info = (const char *)~(UintPtr)0;
    }

    switch(performStates[nodeId]){
    case CONNECTED:
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
    for (int i= 0, n= 0; n < nTransporters && m_run_start_clients_thread; i++){
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
	  t->doDisconnect();
	break;
      case DISCONNECTED:
      {
        if (t->isConnected())
        {
          g_eventLogger->warning("Found connection to %u in state DISCONNECTED "
                                 " while being connected, disconnecting!",
                                 t->getRemoteNodeId());
          t->doDisconnect();
        }
        break;
      }
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

#ifdef NDB_SHM_TRANSPORTER
extern "C"
void
shm_sig_handler(int signo)
{
  g_shm_counter++;
}
#endif

void
TransporterRegistry::startReceiving()
{
  DBUG_ENTER("TransporterRegistry::startReceiving");

#ifdef NDB_SHM_TRANSPORTER
  m_shm_own_pid = getpid();
  if (g_ndb_shm_signum)
  {
    DBUG_PRINT("info",("Install signal handler for signum %d",
		       g_ndb_shm_signum));
    struct sigaction sa;
    NdbThread_set_shm_sigmask(FALSE);
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = shm_sig_handler;
    sa.sa_flags = 0;
    int ret;
    while((ret = sigaction(g_ndb_shm_signum, &sa, 0)) == -1 && errno == EINTR)
      ;
    if(ret != 0)
    {
      DBUG_PRINT("error",("Install failed"));
      g_eventLogger->error("Failed to install signal handler for"
                           " SHM transporter, signum %d, errno: %d (%s)",
                           g_ndb_shm_signum, errno, strerror(errno));
    }
  }
#endif // NDB_SHM_TRANSPORTER
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

Transporter*
TransporterRegistry::get_transporter(NodeId nodeId) {
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
                                 NodeId node, Uint32 lenBytes, Uint32 prio)
{
  Transporter *t = theTransporters[node];
  Uint32 *insertPtr = handle->getWritePtr(node, lenBytes, prio,
                                          t->get_max_send_buffer());

  if (insertPtr == 0) {
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
        insertPtr = handle->getWritePtr(node, lenBytes, prio,
                                        t->get_max_send_buffer());
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

Uint32
TransporterRegistry::get_bytes_to_send_iovec(NodeId node, struct iovec *dst,
                                             Uint32 max)
{
  assert(m_use_default_send_buffer);

  if (max == 0)
    return 0;

  Uint32 count = 0;
  SendBuffer *b = m_send_buffers + node;
  SendBufferPage *page = b->m_first_page;
  while (page != NULL && count < max)
  {
    dst[count].iov_base = page->m_data+page->m_start;
    dst[count].iov_len = page->m_bytes;
    assert(page->m_start + page->m_bytes <= page->max_data_bytes());
    page = page->m_next;
    count++;
  }

  return count;
}

Uint32
TransporterRegistry::bytes_sent(NodeId node, Uint32 bytes)
{
  assert(m_use_default_send_buffer);

  SendBuffer *b = m_send_buffers + node;
  Uint32 used_bytes = b->m_used_bytes;

  if (bytes == 0)
    return used_bytes;

  used_bytes -= bytes;
  b->m_used_bytes = used_bytes;

  SendBufferPage *page = b->m_first_page;
  while (bytes && bytes >= page->m_bytes)
  {
    SendBufferPage * tmp = page;
    bytes -= page->m_bytes;
    page = page->m_next;
    release_page(tmp);
  }

  if (used_bytes == 0)
  {
    b->m_first_page = 0;
    b->m_last_page = 0;
  }
  else
  {
    page->m_start += bytes;
    page->m_bytes -= bytes;
    assert(page->m_start + page->m_bytes <= page->max_data_bytes());
    b->m_first_page = page;
  }

  return used_bytes;
}

bool
TransporterRegistry::has_data_to_send(NodeId node)
{
  assert(m_use_default_send_buffer);

  SendBuffer *b = m_send_buffers + node;
  return (b->m_first_page != NULL && b->m_first_page->m_bytes);
}

void
TransporterRegistry::reset_send_buffer(NodeId node)
{
  assert(m_use_default_send_buffer);
  if (!has_data_to_send(node))
    return;

  SendBuffer *b = m_send_buffers + node;
  SendBufferPage *page = b->m_first_page;
  while (page != NULL)
  {
    SendBufferPage *next = page->m_next;
    release_page(page);
    page = next;
  }
  b->m_first_page = NULL;
  b->m_last_page = NULL;
  b->m_used_bytes = 0;
}

TransporterRegistry::SendBufferPage *
TransporterRegistry::alloc_page()
{
  SendBufferPage *page = m_page_freelist;
  if (page != NULL)
  {
    m_tot_used_buffer_memory += SendBufferPage::PGSIZE;
    m_page_freelist = page->m_next;
    return page;
  }

  ndbout << "ERROR: out of send buffers in kernel." << endl;
  return NULL;
}

void
TransporterRegistry::release_page(SendBufferPage *page)
{
  assert(page != NULL);
  page->m_next = m_page_freelist;
  m_tot_used_buffer_memory -= SendBufferPage::PGSIZE;
  m_page_freelist = page;
}

/**
 * These are the TransporterSendBufferHandle methods used by the
 * single-threaded ndbd.
 */
Uint32 *
TransporterRegistry::getWritePtr(NodeId node, Uint32 lenBytes, Uint32 prio,
                                 Uint32 max_use)
{
  assert(m_use_default_send_buffer);

  SendBuffer *b = m_send_buffers + node;

  /* First check if we have room in already allocated page. */
  SendBufferPage *page = b->m_last_page;
  if (page != NULL && page->m_bytes + page->m_start + lenBytes <= page->max_data_bytes())
  {
    return (Uint32 *)(page->m_data + page->m_start + page->m_bytes);
  }

  if (b->m_used_bytes + lenBytes > max_use)
    return NULL;

  /* Allocate a new page. */
  page = alloc_page();
  if (page == NULL)
    return NULL;
  page->m_next = NULL;
  page->m_bytes = 0;
  page->m_start = 0;

  if (b->m_last_page == NULL)
  {
    b->m_first_page = page;
    b->m_last_page = page;
  }
  else
  {
    assert(b->m_first_page != NULL);
    b->m_last_page->m_next = page;
    b->m_last_page = page;
  }
  return (Uint32 *)(page->m_data);
}

/**
 * This is used by the ndbd, so here only one thread is using this, so
 * values will always be consistent.
 */
void
TransporterRegistry::getSendBufferLevel(NodeId node, SB_LevelType &level)
{
  SendBuffer *b = m_send_buffers + node;
  calculate_send_buffer_level(b->m_used_bytes,
                              m_tot_send_buffer_memory,
                              m_tot_used_buffer_memory,
                              0,
                              level);
  return;
}

Uint32
TransporterRegistry::updateWritePtr(NodeId node, Uint32 lenBytes, Uint32 prio)
{
  assert(m_use_default_send_buffer);

  SendBuffer *b = m_send_buffers + node;
  SendBufferPage *page = b->m_last_page;
  assert(page != NULL);
  assert(page->m_bytes + lenBytes <= page->max_data_bytes());
  page->m_bytes += lenBytes;
  b->m_used_bytes += lenBytes;
  return b->m_used_bytes;
}

bool
TransporterRegistry::forceSend(NodeId node)
{
  Transporter *t = get_transporter(node);
  if (t)
    return t->doSend();
  else
    return false;
}


void
TransporterRegistry::print_transporters(const char* where, NdbOut& out)
{
  out << where << " >>" << endl;

  for(unsigned i = 0; i < maxTransporters; i++){
    if(theTransporters[i] == NULL)
      continue;

    const NodeId remoteNodeId = theTransporters[i]->getRemoteNodeId();
    struct in_addr conn_addr = get_connect_address(remoteNodeId);
    char addr_buf[NDB_ADDR_STRLEN];
    char *addr_str = Ndb_inet_ntop(AF_INET,
                                   static_cast<void*>(&conn_addr),
                                   addr_buf,
                                   sizeof(addr_buf));

    out << i << " "
        << getPerformStateString(remoteNodeId) << " to node: "
        << remoteNodeId << " at " << addr_str << endl;
  }

  out << "<<" << endl;

  for (unsigned i= 0; i < m_transporter_interface.size(); i++){
    Transporter_interface tf= m_transporter_interface[i];

    out << i
        << " remote node: " << tf.m_remote_nodeId
        << " port: " << tf.m_s_service_port
        << " interface: " << tf.m_interface << endl;
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
