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

#include <ndb_global.h>
#include <my_pthread.h>

#include <TransporterRegistry.hpp>
#include "TransporterInternalDefinitions.hpp"

#include "Transporter.hpp"
#include <SocketAuthenticator.hpp>

#ifdef NDB_TCP_TRANSPORTER
#include "TCP_Transporter.hpp"
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
#include <NdbTick.h>
#include <InputStream.hpp>
#include <OutputStream.hpp>

#include <mgmapi/mgmapi.h>
#include <mgmapi_internal.h>
#include <mgmapi/mgmapi_debug.h>

#include <EventLogger.hpp>
extern EventLogger * g_eventLogger;

struct in_addr
TransporterRegistry::get_connect_address(NodeId node_id) const
{
  return theTransporters[node_id]->m_connect_address;
}

SocketServer::Session * TransporterService::newSession(NDB_SOCKET_TYPE sockfd)
{
  DBUG_ENTER("SocketServer::Session * TransporterService::newSession");
  if (m_auth && !m_auth->server_authenticate(sockfd)){
    NDB_CLOSE_SOCKET(sockfd);
    DBUG_RETURN(0);
  }

  if (!m_transporter_registry->connect_server(sockfd))
  {
    NDB_CLOSE_SOCKET(sockfd);
    DBUG_RETURN(0);
  }

  DBUG_RETURN(0);
}

TransporterRegistry::TransporterRegistry(TransporterCallback *callback,
                                         bool use_default_send_buffer,
					 unsigned _maxTransporters,
					 unsigned sizeOfLongSignalMemory) :
  m_mgm_handle(0),
  m_transp_count(0),
  m_use_default_send_buffer(use_default_send_buffer),
  m_send_buffers(0), m_page_freelist(0), m_send_buffer_memory(0),
  m_total_max_send_buffer(0)
{
  DBUG_ENTER("TransporterRegistry::TransporterRegistry");

  nodeIdSpecified = false;
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
  m_disconnect_errnum = new int               [maxTransporters];
  m_error_states      = new ErrorState        [maxTransporters];
 
#if defined(HAVE_EPOLL_CREATE)
 m_epoll_fd = -1;
 m_epoll_events       = new struct epoll_event[maxTransporters];
 m_epoll_fd = epoll_create(maxTransporters);
 if (m_epoll_fd == -1 || !m_epoll_events)
 {
   /* Failure to allocate data or get epoll socket, abort */
   perror("Failed to alloc epoll-array or calling epoll_create... falling back to select!");
   ndbout_c("Falling back to select");
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
 }
 else
 {
   memset((char*)m_epoll_events, 0,
          maxTransporters * sizeof(struct epoll_event));
 }

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
    m_disconnect_errnum[i]= 0;
    m_error_states[i]     = default_error_state;
  }

  DBUG_VOID_RETURN;
}

void
TransporterRegistry::allocate_send_buffers(Uint32 total_send_buffer)
{
  if (!m_use_default_send_buffer)
    return;

  /* Initialize transporter send buffers (initially empty). */
  m_send_buffers = new SendBuffer[maxTransporters];
  for (unsigned i = 0; i < maxTransporters; i++)
  {
    SendBuffer &b = m_send_buffers[i];
    b.m_first_page = NULL;
    b.m_last_page = NULL;
    b.m_current_page = NULL;
    b.m_offset_unsent_data = 0;
    b.m_offset_start_data = 0;
    b.m_used_bytes = 0;
  }

  /* Initialize the page freelist. */
  Uint32 send_buffer_pages =
    (total_send_buffer + SendBufferPage::PGSIZE - 1)/SendBufferPage::PGSIZE;
  /* Add one extra page of internal fragmentation overhead per transporter. */
  send_buffer_pages += nTransporters;

  m_send_buffer_memory =
    new unsigned char[send_buffer_pages * SendBufferPage::PGSIZE];
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
  
  removeAll();
  
  delete[] theTCPTransporters;
  delete[] theSCITransporters;
  delete[] theSHMTransporters;
  delete[] theTransporterTypes;
  delete[] theTransporters;
  delete[] performStates;
  delete[] ioStates;
  delete[] m_disconnect_errnum;
  delete[] m_error_states;

  if (m_send_buffers)
    delete[] m_send_buffers;
  m_page_freelist = NULL;
  if (m_send_buffer_memory)
    delete[] m_send_buffer_memory;

#if defined(HAVE_EPOLL_CREATE)
  if (m_epoll_events) delete [] m_epoll_events;
  if (m_epoll_fd != -1) close(m_epoll_fd);
#endif
  if (m_mgm_handle)
    ndb_mgm_destroy_handle(&m_mgm_handle);

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
  nodeIdSpecified = true;
  localNodeId = nodeId;
  
  DEBUG("TransporterRegistry started node: " << localNodeId);
  
  DBUG_RETURN(true);
}

bool
TransporterRegistry::connect_server(NDB_SOCKET_TYPE sockfd)
{
  DBUG_ENTER("TransporterRegistry::connect_server");

  // read node id and transporter type from client
  int nodeId, remote_transporter_type= -1;
  SocketInputStream s_input(sockfd);
  char buf[11+1+11+1]; // <int> <int>
  if (s_input.gets(buf, sizeof(buf)) == 0) {
    DBUG_PRINT("error", ("Could not get node id from client"));
    DBUG_RETURN(false);
  }
  int r= sscanf(buf, "%d %d", &nodeId, &remote_transporter_type);
  switch (r) {
  case 2:
    break;
  case 1:
    // we're running version prior to 4.1.9
    // ok, but with no checks on transporter configuration compatability
    break;
  default:
    DBUG_PRINT("error", ("Error in node id from client"));
    DBUG_RETURN(false);
  }

  DBUG_PRINT("info", ("nodeId=%d remote_transporter_type=%d",
		      nodeId,remote_transporter_type));

  //check that nodeid is valid and that there is an allocated transporter
  if ( nodeId < 0 || nodeId >= (int)maxTransporters) {
    DBUG_PRINT("error", ("Node id out of range from client"));
    DBUG_RETURN(false);
  }
  if (theTransporters[nodeId] == 0) {
      DBUG_PRINT("error", ("No transporter for this node id from client"));
      DBUG_RETURN(false);
  }

  //check that the transporter should be connected
  if (performStates[nodeId] != TransporterRegistry::CONNECTING) {
    DBUG_PRINT("error", ("Transporter in wrong state for this node id from client"));
    DBUG_RETURN(false);
  }

  Transporter *t= theTransporters[nodeId];

  // send info about own id (just as response to acknowledge connection)
  // send info on own transporter type
  SocketOutputStream s_output(sockfd);
  s_output.println("%d %d", t->getLocalNodeId(), t->m_type);

  if (remote_transporter_type != -1)
  {
    if (remote_transporter_type != t->m_type)
    {
      DBUG_PRINT("error", ("Transporter types mismatch this=%d remote=%d",
			   t->m_type, remote_transporter_type));
      g_eventLogger->error("Incompatible configuration: Transporter type "
                           "mismatch with node %d", nodeId);

      // wait for socket close for 1 second to let message arrive at client
      {
	fd_set a_set;
	FD_ZERO(&a_set);
	FD_SET(sockfd, &a_set);
	struct timeval timeout;
	timeout.tv_sec  = 1; timeout.tv_usec = 0;
	select(sockfd+1, &a_set, 0, 0, &timeout);
      }
      DBUG_RETURN(false);
    }
  }
  else if (t->m_type == tt_SHM_TRANSPORTER)
  {
    g_eventLogger->warning("Unable to verify transporter compatability with node %d", nodeId);
  }

  // setup transporter (transporter responsible for closing sockfd)
  bool res = t->connect_server(sockfd);

  if (res && performStates[nodeId] != TransporterRegistry::CONNECTING)
  {
    DBUG_RETURN(false);
  }

  DBUG_RETURN(res);
}

bool
TransporterRegistry::createTCPTransporter(TransporterConfiguration *config) {
#ifdef NDB_TCP_TRANSPORTER

  if(!nodeIdSpecified){
    init(config->localNodeId);
  }
  
  if(config->localNodeId != localNodeId) 
    return false;
  
  if(theTransporters[config->remoteNodeId] != NULL)
    return false;
   
  TCP_Transporter * t = new TCP_Transporter(*this, config);

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
  
  if(!nodeIdSpecified){
    init(config->localNodeId);
  }
  
  if(config->localNodeId != localNodeId)
    return false;
 
  if(theTransporters[config->remoteNodeId] != NULL)
    return false;
 
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
					    config->signalId);
  
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
  if(!nodeIdSpecified){
    init(config->localNodeId);
  }
  
  if(config->localNodeId != localNodeId)
    return false;
  
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
  
  if(theTransporters[config->remoteNodeId] != NULL)
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
					    config->shm.shmSize
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

SendStatus
TransporterRegistry::prepareSend(TransporterSendBufferHandle *sendHandle,
                                 const SignalHeader * const signalHeader,
				 Uint8 prio,
				 const Uint32 * const signalData,
				 NodeId nodeId, 
				 const LinearSectionPtr ptr[3]){


  Transporter *t = theTransporters[nodeId];
  if(t != NULL && 
     (((ioStates[nodeId] != HaltOutput) && (ioStates[nodeId] != HaltIO)) || 
      ((signalHeader->theReceiversBlockNumber == 252) ||
       (signalHeader->theReceiversBlockNumber == 4002)))) {
	 
    if(t->isConnected()){
      Uint32 lenBytes = t->m_packer.getMessageLength(signalHeader, ptr);
      if(lenBytes <= MAX_SEND_MESSAGE_BYTESIZE){
	Uint32 * insertPtr = getWritePtr(sendHandle, nodeId, lenBytes, prio);
	if(insertPtr != 0){
	  t->m_packer.pack(insertPtr, prio, signalHeader, signalData, ptr);
	  sendHandle->updateWritePtr(nodeId, lenBytes, prio);
	  return SEND_OK;
	}

	int sleepTime = 2;	

	/**
	 * @note: on linux/i386 the granularity is 10ms
	 *        so sleepTime = 2 generates a 10 ms sleep.
	 */
	for(int i = 0; i<50; i++){
	  if((nSHMTransporters+nSCITransporters) == 0)
	    NdbSleep_MilliSleep(sleepTime); 
	  insertPtr = getWritePtr(sendHandle, nodeId, lenBytes, prio);
	  if(insertPtr != 0){
	    t->m_packer.pack(insertPtr, prio, signalHeader, signalData, ptr);
	    sendHandle->updateWritePtr(nodeId, lenBytes, prio);
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
	return SEND_MESSAGE_TOO_BIG;
      }
    } else {
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
                                 const SignalHeader * const signalHeader,
				 Uint8 prio,
				 const Uint32 * const signalData,
				 NodeId nodeId, 
				 class SectionSegmentPool & thePool,
				 const SegmentedSectionPtr ptr[3]){
  

  Transporter *t = theTransporters[nodeId];
  if(t != NULL && 
     (((ioStates[nodeId] != HaltOutput) && (ioStates[nodeId] != HaltIO)) || 
      ((signalHeader->theReceiversBlockNumber == 252)|| 
       (signalHeader->theReceiversBlockNumber == 4002)))) {
    
    if(t->isConnected()){
      Uint32 lenBytes = t->m_packer.getMessageLength(signalHeader, ptr);
      if(lenBytes <= MAX_SEND_MESSAGE_BYTESIZE){
	Uint32 * insertPtr = getWritePtr(sendHandle, nodeId, lenBytes, prio);
	if(insertPtr != 0){
	  t->m_packer.pack(insertPtr, prio, signalHeader, signalData, thePool, ptr);
	  sendHandle->updateWritePtr(nodeId, lenBytes, prio);
	  return SEND_OK;
	}
	
	
	/**
	 * @note: on linux/i386 the granularity is 10ms
	 *        so sleepTime = 2 generates a 10 ms sleep.
	 */
	int sleepTime = 2;
	for(int i = 0; i<50; i++){
	  if((nSHMTransporters+nSCITransporters) == 0)
	    NdbSleep_MilliSleep(sleepTime); 
	  insertPtr = getWritePtr(sendHandle, nodeId, lenBytes, prio);
	  if(insertPtr != 0){
	    t->m_packer.pack(insertPtr, prio, signalHeader, signalData, thePool, ptr);
	    sendHandle->updateWritePtr(nodeId, lenBytes, prio);
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
	return SEND_MESSAGE_TOO_BIG;
      }
    } else {
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
                                 const SignalHeader * const signalHeader,
				 Uint8 prio,
				 const Uint32 * const signalData,
				 NodeId nodeId, 
				 GenericSectionPtr ptr[3]){


  Transporter *t = theTransporters[nodeId];
  if(t != NULL && 
     (((ioStates[nodeId] != HaltOutput) && (ioStates[nodeId] != HaltIO)) || 
      ((signalHeader->theReceiversBlockNumber == 252) ||
       (signalHeader->theReceiversBlockNumber == 4002)))) {
	 
    if(t->isConnected()){
      Uint32 lenBytes = t->m_packer.getMessageLength(signalHeader, ptr);
      if(lenBytes <= MAX_SEND_MESSAGE_BYTESIZE){
        Uint32 * insertPtr = getWritePtr(sendHandle, nodeId, lenBytes, prio);
        if(insertPtr != 0){
          t->m_packer.pack(insertPtr, prio, signalHeader, signalData, ptr);
          sendHandle->updateWritePtr(nodeId, lenBytes, prio);
          return SEND_OK;
	}


	/**
	 * @note: on linux/i386 the granularity is 10ms
	 *        so sleepTime = 2 generates a 10 ms sleep.
	 */
        int sleepTime = 2;	
	for(int i = 0; i<50; i++){
	  if((nSHMTransporters+nSCITransporters) == 0)
	    NdbSleep_MilliSleep(sleepTime); 
	  insertPtr = getWritePtr(sendHandle, nodeId, lenBytes, prio);
	  if(insertPtr != 0){
	    t->m_packer.pack(insertPtr, prio, signalHeader, signalData, ptr);
	    sendHandle->updateWritePtr(nodeId, lenBytes, prio);
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
	return SEND_MESSAGE_TOO_BIG;
      }
    } else {
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

void
TransporterRegistry::external_IO(Uint32 timeOutMillis) {
  //-----------------------------------------------------------
  // Most of the time we will send the buffers here and then wait
  // for new signals. Thus we start by sending without timeout
  // followed by the receive part where we expect to sleep for
  // a while.
  //-----------------------------------------------------------
  if(pollReceive(timeOutMillis)){
    performReceive();
  }
  performSend();
}

Uint32
TransporterRegistry::pollReceive(Uint32 timeOutMillis){
  Uint32 retVal = 0;

  if((nSCITransporters) > 0)
  {
    timeOutMillis=0;
  }

#ifdef NDB_SHM_TRANSPORTER
  if(nSHMTransporters > 0)
  {
    Uint32 res = poll_SHM(0);
    if(res)
    {
      retVal |= res;
      timeOutMillis = 0;
    }
  }
#endif

#ifdef NDB_TCP_TRANSPORTER
#if defined(HAVE_EPOLL_CREATE)
  if (likely(m_epoll_fd != -1))
  {
    Uint32 num_trps = nTCPTransporters;
    /**
     * If any transporters have left-over data that was not fully executed in
     * last loop, don't wait and return 'data available' even if nothing new
     * from epoll.
     */
    if (!m_has_data_transporters.isclear())
    {
      timeOutMillis = 0;
      retVal = 1;
    }
    
    if (num_trps)
    {
      tcpReadSelectReply = epoll_wait(m_epoll_fd, m_epoll_events,
                                      num_trps, timeOutMillis);
      retVal |= tcpReadSelectReply;
    }
  }
  else
#endif
  {
    if(nTCPTransporters > 0 || retVal == 0)
    {
      retVal |= poll_TCP(timeOutMillis);
    }
    else
      tcpReadSelectReply = 0;
  }
#endif
#ifdef NDB_SCI_TRANSPORTER
  if(nSCITransporters > 0)
    retVal |= poll_SCI(timeOutMillis);
#endif
#ifdef NDB_SHM_TRANSPORTER
  if(nSHMTransporters > 0 && retVal == 0)
  {
    int res = poll_SHM(0);
    retVal |= res;
  }
#endif
  return retVal;
}


#ifdef NDB_SCI_TRANSPORTER
Uint32
TransporterRegistry::poll_SCI(Uint32 timeOutMillis)
{
  for (int i=0; i<nSCITransporters; i++) {
    SCI_Transporter * t = theSCITransporters[i];
    Uint32 node_id= t->getRemoteNodeId();
    if (t->isConnected() && is_connected(node_id)) {
      if(t->hasDataToRead())
	return 1;
    }
  }
  return 0;
}
#endif


#ifdef NDB_SHM_TRANSPORTER
static int g_shm_counter = 0;
Uint32
TransporterRegistry::poll_SHM(Uint32 timeOutMillis)
{  
  for(int j=0; j < 100; j++)
  {
    for (int i=0; i<nSHMTransporters; i++) {
      SHM_Transporter * t = theSHMTransporters[i];
      Uint32 node_id= t->getRemoteNodeId();
      if (t->isConnected() && is_connected(node_id)) {
	if(t->hasDataToRead()) {
	  return 1;
	}
      }
    }
  }
  return 0;
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
TransporterRegistry::poll_TCP(Uint32 timeOutMillis)
{
  bool hasdata = false;
  if (false && nTCPTransporters == 0)
  {
    tcpReadSelectReply = 0;
    return 0;
  }
  
  NDB_SOCKET_TYPE maxSocketValue = -1;
  
  // Needed for TCP/IP connections
  // The read- and writeset are used by select
  
  FD_ZERO(&tcpReadset);

  // Prepare for sending and receiving
  for (int i = 0; i < nTCPTransporters; i++) {
    TCP_Transporter * t = theTCPTransporters[i];
    Uint32 node_id= t->getRemoteNodeId();
    
    // If the transporter is connected
    if (is_connected(node_id) && t->isConnected()) {
      
      const NDB_SOCKET_TYPE socket = t->getSocket();
      if (socket == NDB_INVALID_SOCKET)
        continue;
      // Find the highest socket value. It will be used by select
      if (socket > maxSocketValue)
	maxSocketValue = socket;
      
      // Put the connected transporters in the socket read-set 
      FD_SET(socket, &tcpReadset);
    }
    hasdata |= t->hasReceiveData();
  }
  
  timeOutMillis = hasdata ? 0 : timeOutMillis;
  
  struct timeval timeout;
  timeout.tv_sec  = timeOutMillis / 1000;
  timeout.tv_usec = (timeOutMillis % 1000) * 1000;

  // The highest socket value plus one
  maxSocketValue++; 
  
  tcpReadSelectReply = select(maxSocketValue, &tcpReadset, 0, 0, &timeout);  
  if(false && tcpReadSelectReply == -1 && errno == EINTR)
    g_eventLogger->info("woke-up by signal");

#ifdef NDB_WIN32
  if(tcpReadSelectReply == SOCKET_ERROR)
  {
    NdbSleep_MilliSleep(timeOutMillis);
  }
#endif
  
  return tcpReadSelectReply || hasdata;
}
#endif

#if defined(HAVE_EPOLL_CREATE)
bool
TransporterRegistry::change_epoll(TCP_Transporter *t, bool add)
{
  struct epoll_event event_poll;
  bzero(&event_poll, sizeof(event_poll));
  int sock_fd = t->getSocket();
  int node_id = t->getRemoteNodeId();
  int op = add ? EPOLL_CTL_ADD : EPOLL_CTL_DEL;
  int ret_val, error;

  if (sock_fd == NDB_INVALID_SOCKET)
    return FALSE;

  event_poll.data.u32 = t->getRemoteNodeId();
  event_poll.events = EPOLLIN;
  ret_val = epoll_ctl(m_epoll_fd, op, sock_fd, &event_poll);
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
    ndbout_c("Failed to %s epollfd: %u fd %u node %u to epoll-set,"
             " errno: %u %s",
             add ? "ADD" : "DEL",
             m_epoll_fd,
             sock_fd,
             node_id,
             error,
             strerror(error));
    abort();
  }
  ndbout << "We lacked memory to add the socket for node id ";
  ndbout << node_id << endl;
  return TRUE;

ok:
  return FALSE;
}

/**
 * In multi-threaded cases, this must be protected by a global receive lock.
 */
void
TransporterRegistry::get_tcp_data(TCP_Transporter *t)
{
  const NodeId node_id = t->getRemoteNodeId();
  bool hasdata = false;
  callbackObj->checkJobBuffer();
  if (is_connected(node_id) && t->isConnected())
  {
    t->doReceive();
    
    Uint32 *ptr;
    Uint32 sz = t->getReceiveData(&ptr);
    callbackObj->transporter_recv_from(node_id);
    Uint32 szUsed = unpack(ptr, sz, node_id, ioStates[node_id]);
    t->updateReceiveDataPtr(szUsed);
    hasdata = t->hasReceiveData();
  }
  m_has_data_transporters.set(node_id, hasdata);
}

#endif

void
TransporterRegistry::performReceive()
{
#ifdef NDB_TCP_TRANSPORTER
#if defined(HAVE_EPOLL_CREATE)
  if (likely(m_epoll_fd != -1))
  {
    int num_socket_events = tcpReadSelectReply;
    int i;
    
    if (num_socket_events > 0)
    {
      for (i = 0; i < num_socket_events; i++)
      {
        m_has_data_transporters.set(m_epoll_events[i].data.u32);
      }
    }
    else if (num_socket_events < 0)
    {
      assert(errno == EINTR);
    }
    
    Uint32 id = 0;
    while ((id = m_has_data_transporters.find(id + 1)) != BitmaskImpl::NotFound)
    {
      get_tcp_data((TCP_Transporter*)theTransporters[id]);
    }
  }
  else
#endif
  {
    for (int i=0; i<nTCPTransporters; i++) 
    {
      callbackObj->checkJobBuffer();
      TCP_Transporter *t = theTCPTransporters[i];
      const NodeId nodeId = t->getRemoteNodeId();
      const NDB_SOCKET_TYPE socket    = t->getSocket();
      if(is_connected(nodeId)){
        if(t->isConnected())
        {
          if (FD_ISSET(socket, &tcpReadset))
          {
            t->doReceive();
          }
          
          if (t->hasReceiveData())
          {
            Uint32 * ptr;
            Uint32 sz = t->getReceiveData(&ptr);
            callbackObj->transporter_recv_from(nodeId);
            Uint32 szUsed = unpack(ptr, sz, nodeId, ioStates[nodeId]);
            t->updateReceiveDataPtr(szUsed);
          }
        } 
      }
    }
  }
#endif
  
#ifdef NDB_SCI_TRANSPORTER
  //performReceive
  //do prepareReceive on the SCI transporters  (prepareReceive(t,,,,))
  for (int i=0; i<nSCITransporters; i++) 
  {
    callbackObj->checkJobBuffer();
    SCI_Transporter  *t = theSCITransporters[i];
    const NodeId nodeId = t->getRemoteNodeId();
    if(is_connected(nodeId))
    {
      if(t->isConnected() && t->checkConnected())
      {
	Uint32 * readPtr, * eodPtr;
	t->getReceivePtr(&readPtr, &eodPtr);
	callbackObj->transporter_recv_from(nodeId);
	Uint32 *newPtr = unpack(readPtr, eodPtr, nodeId, ioStates[nodeId]);
	t->updateReceivePtr(newPtr);
      }
    } 
  }
#endif
#ifdef NDB_SHM_TRANSPORTER
  for (int i=0; i<nSHMTransporters; i++) 
  {
    callbackObj->checkJobBuffer();
    SHM_Transporter *t = theSHMTransporters[i];
    const NodeId nodeId = t->getRemoteNodeId();
    if(is_connected(nodeId)){
      if(t->isConnected() && t->checkConnected())
      {
	Uint32 * readPtr, * eodPtr;
	t->getReceivePtr(&readPtr, &eodPtr);
	callbackObj->transporter_recv_from(nodeId);
	Uint32 *newPtr = unpack(readPtr, eodPtr, nodeId, ioStates[nodeId]);
	t->updateReceivePtr(newPtr);
      }
    } 
  }
#endif
}

/**
 * In multi-threaded cases, this must be protected by send lock (can use
 * different locks for each node).
 */
void
TransporterRegistry::performSend(NodeId nodeId)
{
  Transporter *t = get_transporter(nodeId);
  if (t && t->has_data_to_send() && t->isConnected() &&
      is_connected(nodeId))
    t->doSend();
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
        t->isConnected() && is_connected(t->getRemoteNodeId()))
    {
      t->doSend();
    }
  }
  for (i = 0; i < m_transp_count && i < nTCPTransporters; i++) 
  {
    TCP_Transporter *t = theTCPTransporters[i];
    if (t && t->has_data_to_send() &&
        t->isConnected() && is_connected(t->getRemoteNodeId()))
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
      if(t->isConnected() && t->has_data_to_send())
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
      if(t->isConnected())
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

IOState
TransporterRegistry::ioState(NodeId nodeId) { 
  return ioStates[nodeId]; 
}

void
TransporterRegistry::setIOState(NodeId nodeId, IOState state) {
  DEBUG("TransporterRegistry::setIOState("
	<< nodeId << ", " << state << ")");
  ioStates[nodeId] = state;
}

static void * 
run_start_clients_C(void * me)
{
  ((TransporterRegistry*) me)->start_clients_thread();
  return 0;
}

/**
 * This method is used to initiate connection, called from the CMVMI blockx.
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
    break;
  }
  DBUG_ENTER("TransporterRegistry::do_connect");
  DBUG_PRINT("info",("performStates[%d]=CONNECTING",node_id));
  curr_state= CONNECTING;
  DBUG_VOID_RETURN;
}

/**
 * This method is used to initiate disconnect from CMVMI. It is also called
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
TransporterRegistry::report_connect(NodeId node_id)
{
  DBUG_ENTER("TransporterRegistry::report_connect");
  DBUG_PRINT("info",("performStates[%d]=CONNECTED",node_id));
  performStates[node_id] = CONNECTED;
#if defined(HAVE_EPOLL_CREATE)
  if (likely(m_epoll_fd != -1))
  {
    if (change_epoll((TCP_Transporter*)theTransporters[node_id],
                     TRUE))
    {
      performStates[node_id] = DISCONNECTING;
      DBUG_VOID_RETURN;
    }
  }
#endif
  callbackObj->reportConnect(node_id);
  DBUG_VOID_RETURN;
}

void
TransporterRegistry::report_disconnect(NodeId node_id, int errnum)
{
  DBUG_ENTER("TransporterRegistry::report_disconnect");
  DBUG_PRINT("info",("performStates[%d]=DISCONNECTED",node_id));
  performStates[node_id] = DISCONNECTED;
#ifdef HAVE_EPOLL_CREATE
  m_has_data_transporters.clear(node_id);
#endif
  callbackObj->reportDisconnect(node_id, errnum);
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
 */
void
TransporterRegistry::update_connections()
{
  for (int i= 0, n= 0; n < nTransporters; i++){
    Transporter * t = theTransporters[i];
    if (!t)
      continue;
    n++;

    const NodeId nodeId = t->getRemoteNodeId();

    TransporterError code = m_error_states[nodeId].m_code;
    const char *info = m_error_states[nodeId].m_info;
    if (code != TE_NO_ERROR && info != (const char *)~(UintPtr)0)
    {
      callbackObj->reportError(nodeId, code, info);
      m_error_states[nodeId].m_code = TE_NO_ERROR;
      m_error_states[nodeId].m_info = (const char *)~(UintPtr)0;
    }

    switch(performStates[nodeId]){
    case CONNECTED:
    case DISCONNECTED:
      break;
    case CONNECTING:
      if(t->isConnected())
	report_connect(nodeId);
      break;
    case DISCONNECTING:
      if(!t->isConnected())
	report_disconnect(nodeId, m_disconnect_errnum[nodeId]);
      break;
    }
  }
}

// run as own thread
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
	  bool connected= false;
	  /**
	   * First, we try to connect (if we have a port number).
	   */
	  if (t->get_s_port())
	    connected= t->connect_client();

	  /**
	   * If dynamic, get the port for connecting from the management server
	   */
	  if( !connected && t->get_s_port() <= 0) {	// Port is dynamic
	    int server_port= 0;
	    struct ndb_mgm_reply mgm_reply;

	    if(!ndb_mgm_is_connected(m_mgm_handle))
	      ndb_mgm_connect(m_mgm_handle, 0, 0, 0);
	    
	    if(ndb_mgm_is_connected(m_mgm_handle))
	    {
	      int res=
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
		/**
		 * Server_port == 0 just means that that a mgmt server
		 * has not received a new port yet. Keep the old.
		 */
		if (server_port)
		  t->set_s_port(server_port);
	      }
	      else if(ndb_mgm_is_connected(m_mgm_handle))
	      {
                g_eventLogger->info("Failed to get dynamic port to connect to: %d", res);
		ndb_mgm_disconnect(m_mgm_handle);
	      }
	      else
	      {
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

bool
TransporterRegistry::start_clients()
{
  char thread_object[THREAD_CONTAINER_SIZE];
  uint len;

  m_run_start_clients_thread= true;
  ndb_thread_fill_thread_object((void*)thread_object, &len, FALSE);
  m_start_clients_thread= NdbThread_CreateWithFunc(run_start_clients_C,
					   (void**)this,
					   32768,
					   "ndb_start_clients",
					   NDB_THREAD_PRIO_LOW,
                                           ndb_thread_add_thread_id,
                                           thread_object,
                                           len,
                                           ndb_thread_remove_thread_id,
                                           thread_object,
                                           len);
  if (m_start_clients_thread == 0) {
    m_run_start_clients_thread= false;
    return false;
  }
  return true;
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
  if (m_transporter_interface.size() > 0 && !nodeIdSpecified)
  {
    g_eventLogger->error("TransporterRegistry::startReceiving: localNodeId not specified");
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
static
RETSIGTYPE 
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
    while((ret = sigaction(g_ndb_shm_signum, &sa, 0)) == -1 && errno == EINTR);
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
  /**
   * Disconnect all transporters, this includes detach from remote node
   * and since that must be done from the same process that called attach
   * it's done here in the receive thread
   */
  disconnectAll();
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

/**
 * Given a connected NdbMgmHandle, turns it into a transporter
 * and returns the socket.
 */
NDB_SOCKET_TYPE TransporterRegistry::connect_ndb_mgmd(NdbMgmHandle *h)
{
  struct ndb_mgm_reply mgm_reply;

  if ( h==NULL || *h == NULL )
  {
    g_eventLogger->error("%s: %d", __FILE__, __LINE__);
    return NDB_INVALID_SOCKET;
  }

  for(unsigned int i=0;i < m_transporter_interface.size();i++)
    if (m_transporter_interface[i].m_s_service_port < 0
	&& ndb_mgm_set_connection_int_parameter(*h,
				   get_localNodeId(),
				   m_transporter_interface[i].m_remote_nodeId,
				   CFG_CONNECTION_SERVER_PORT,
				   m_transporter_interface[i].m_s_service_port,
				   &mgm_reply) < 0)
    {
      g_eventLogger->error("Error: %s: %d",
                           ndb_mgm_get_latest_error_desc(*h),
                           ndb_mgm_get_latest_error(*h));
      g_eventLogger->error("%s: %d", __FILE__, __LINE__);
      ndb_mgm_destroy_handle(h);
      return NDB_INVALID_SOCKET;
    }

  /**
   * convert_to_transporter also disposes of the handle (i.e. we don't leak
   * memory here.
   */
  NDB_SOCKET_TYPE sockfd= ndb_mgm_convert_to_transporter(h);
  if ( sockfd == NDB_INVALID_SOCKET)
  {
    g_eventLogger->error("Error: %s: %d",
                         ndb_mgm_get_latest_error_desc(*h),
                         ndb_mgm_get_latest_error(*h));
    g_eventLogger->error("%s: %d", __FILE__, __LINE__);
    ndb_mgm_destroy_handle(h);
  }
  return sockfd;
}

/**
 * Given a SocketClient, creates a NdbMgmHandle, turns it into a transporter
 * and returns the socket.
 */
NDB_SOCKET_TYPE TransporterRegistry::connect_ndb_mgmd(SocketClient *sc)
{
  NdbMgmHandle h= ndb_mgm_create_handle();

  if ( h == NULL )
  {
    return NDB_INVALID_SOCKET;
  }

  /**
   * Set connectstring
   */
  {
    BaseString cs;
    cs.assfmt("%s:%u",sc->get_server_name(),sc->get_port());
    ndb_mgm_set_connectstring(h, cs.c_str());
  }

  if(ndb_mgm_connect(h, 0, 0, 0)<0)
  {
    ndb_mgm_destroy_handle(&h);
    return NDB_INVALID_SOCKET;
  }

  return connect_ndb_mgmd(&h);
}

/**
 * Default implementation of transporter send buffer handler.
 */

Uint32 *
TransporterRegistry::getWritePtr(TransporterSendBufferHandle *handle,
                                 NodeId node, Uint32 lenBytes, Uint32 prio)
{
  Transporter *t = theTransporters[node];
  Uint32 *insertPtr = handle->getWritePtr(node, lenBytes, prio,
                                          t->get_max_send_buffer());

  if (insertPtr == 0) {
    struct timeval timeout = {0, 10000};
    //-------------------------------------------------
    // Buffer was completely full. We have severe problems.
    // We will attempt to wait for a small time
    //-------------------------------------------------
    if(t->send_is_possible(&timeout)) {
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
    struct timeval no_timeout = {0,0};
    if(t->send_is_possible(&no_timeout)) {
      //-------------------------------------------------
      // Send was possible, attempt at a send.
      //-------------------------------------------------
      handle->forceSend(node);
    }//if
  }
}

int
TransporterRegistry::get_bytes_to_send_iovec(NodeId node, struct iovec *dst,
                                             Uint32 max)
{
  assert(m_use_default_send_buffer);

  if (max == 0)
    return 0;
  SendBuffer *b = m_send_buffers + node;

  SendBufferPage *page = b->m_current_page;
  if (page == NULL)
    return 0;

  Uint32 offset = b->m_offset_unsent_data;
  assert(offset <= page->m_bytes);
  if (offset == page->m_bytes)
    return 0;

  dst[0].iov_base = (char*)(page->m_data + offset);
  dst[0].iov_len = page->m_bytes - offset;
  Uint32 count = 1;
  page = page->m_next;

  while (page != NULL && count < max)
  {
    dst[count].iov_base = (char*)page->m_data;
    dst[count].iov_len = page->m_bytes;
    page = page->m_next;
    count++;
  }

  if (page != NULL)
  {
    b->m_current_page = page;
    b->m_offset_unsent_data = 0;
  }
  else
  {
    assert(b->m_last_page != NULL);
    b->m_current_page = b->m_last_page;
    b->m_offset_unsent_data = b->m_last_page->m_bytes;
  }

  return count;
}

Uint32
TransporterRegistry::bytes_sent(NodeId node, const struct iovec *src,
                                Uint32 bytes)
{
  assert(m_use_default_send_buffer);

  SendBuffer *b = m_send_buffers + node;
  Uint32 used_bytes = b->m_used_bytes;

  if (bytes == 0)
    return used_bytes;

  used_bytes -= bytes;
  b->m_used_bytes = used_bytes;

  SendBufferPage *page = b->m_first_page;
  assert(page != NULL);

  /**
   * On the first page, part of the page may have been sent previously, as
   * indicated by b->m_offset_start_data.
   *
   * Additionally, there may be more data on the page than what was sent, or
   * else we will need to release this (and possibly more) pages.
   */
  assert(b->m_offset_start_data < page->m_bytes);
  Uint32 rest = page->m_bytes - b->m_offset_start_data;
  if (rest > bytes)
  {
    b->m_offset_start_data += bytes;
    return used_bytes;
  }
  bytes -= rest;
  /**
   * Now loop, releasing pages until we find one where not all data has been sent.
   */
  for(;;) {
    if (page == b->m_last_page)
    {
      /**
       * Don't free the last page if emptied completely.
       * Instead keep it for storing more data later.
       */
      break;
    }
    SendBufferPage *next = page->m_next;
    assert(next != NULL);
    if (page == b->m_current_page)
    {
      assert(page->m_bytes == b->m_offset_unsent_data);
      b->m_current_page = next;
      b->m_offset_unsent_data = 0;
    }
    release_page(page);
    page = next;
    if (bytes == 0)
      break;
    assert(page != NULL);
    if (bytes < page->m_bytes)
      break;
    bytes -= page->m_bytes;
  }
  if (page == NULL)
  {
    /* We have sent everything we had. */
    assert(bytes == 0);
    assert(b->m_current_page == NULL);
    assert(b->m_offset_unsent_data == 0);
    b->m_first_page = NULL;
    b->m_last_page = NULL;
    b->m_offset_start_data = 0;
  }
  else
  {
    /* We have sent only part of a page. */
    b->m_first_page = page;
    b->m_offset_start_data = bytes;
  }
  return used_bytes;
}

bool
TransporterRegistry::has_data_to_send(NodeId node)
{
  assert(m_use_default_send_buffer);

  SendBuffer *b = m_send_buffers + node;
  return (b->m_current_page != NULL &&
          b->m_current_page->m_bytes > b->m_offset_unsent_data);
}

void
TransporterRegistry::reset_send_buffer(NodeId node)
{
  assert(m_use_default_send_buffer);

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
  b->m_current_page = NULL;
  b->m_offset_unsent_data = 0;
  b->m_offset_start_data = 0;
  b->m_used_bytes = 0;
}

TransporterRegistry::SendBufferPage *
TransporterRegistry::alloc_page()
{
  SendBufferPage *page = m_page_freelist;
  if (page != NULL)
  {
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
  m_page_freelist = page;
}

Uint32 *
TransporterRegistry::getWritePtr(NodeId node, Uint32 lenBytes, Uint32 prio,
                                 Uint32 max_use)
{
  assert(m_use_default_send_buffer);

  SendBuffer *b = m_send_buffers + node;
  Uint32 *p;

  if (b->m_used_bytes + lenBytes > max_use)
    return NULL;

  /* First check if we have room in already allocated page. */
  SendBufferPage *page = b->m_last_page;
  if (page != NULL && page->m_bytes + lenBytes <= page->max_data_bytes())
  {
    p = (Uint32 *)(page->m_data + page->m_bytes);
    return p;
  }

  /* Allocate a new page. */
  page = alloc_page();
  if (page == NULL)
    return NULL;
  page->m_next = NULL;
  page->m_bytes = 0;

  if (b->m_last_page == NULL)
  {
    b->m_first_page = page;
    b->m_last_page = page;
    b->m_current_page = page;
    b->m_offset_unsent_data = 0;
    b->m_offset_start_data = 0;
  }
  else
  {
    assert(b->m_first_page != NULL);
    if (b->m_current_page == NULL)
    {
      b->m_current_page = page;
      b->m_offset_unsent_data = 0;
    }
    b->m_last_page->m_next = page;
    b->m_last_page = page;
  }
  return (Uint32 *)(page->m_data);
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

  /**
   * If we have no data not returned from get_bytes_to_send_iovec(), and the
   * first signal spills over into a new page, we move the current pointer to
   * not have to deal with a page with zero data in get_bytes_to_send_iovec().
   */
  if (b->m_current_page != NULL &&
      b->m_current_page->m_bytes == b->m_offset_unsent_data)
  {
    b->m_current_page = b->m_current_page->m_next;
    assert(b->m_current_page == page);
    b->m_offset_unsent_data = 0;
  }
  /**
   * If all data has been sent, and the first new signal spills over into a
   * new page, we get a first page with no data which we need to free.
   */
  SendBufferPage *tmp = b->m_first_page;
  if (tmp != NULL && tmp->m_bytes == b->m_offset_start_data)
  {
    b->m_first_page = tmp->m_next;
    assert(b->m_first_page == page);
    assert(b->m_current_page == page);
    release_page(tmp);
    b->m_offset_start_data = 0;
  }

  /**
   * ToDo: To get better buffer utilization, we might at this point attempt
   * to copy back part of the new data into a previous page.
   *
   * This will be especially worthwhile in case of big long signals.
   */

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

template class Vector<TransporterRegistry::Transporter_interface>;
