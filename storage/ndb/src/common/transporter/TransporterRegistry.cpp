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

#include "TransporterCallback.hpp"
#include "NdbOut.hpp"
#include <NdbSleep.h>
#include <NdbTick.h>
#include <InputStream.hpp>
#include <OutputStream.hpp>

#include <mgmapi/mgmapi.h>
#include <mgmapi_internal.h>
#include <mgmapi/mgmapi_debug.h>

#include <EventLogger.hpp>
extern EventLogger g_eventLogger;

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

TransporterRegistry::TransporterRegistry(void * callback,
					 unsigned _maxTransporters,
					 unsigned sizeOfLongSignalMemory) :
  m_mgm_handle(0),
  m_transp_count(0)
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
  
  // Initialize member variables
  nTransporters    = 0;
  nTCPTransporters = 0;
  nSCITransporters = 0;
  nSHMTransporters = 0;
  
  // Initialize the transporter arrays
  for (unsigned i=0; i<maxTransporters; i++) {
    theTCPTransporters[i] = NULL;
    theSCITransporters[i] = NULL;
    theSHMTransporters[i] = NULL;
    theTransporters[i]    = NULL;
    performStates[i]      = DISCONNECTED;
    ioStates[i]           = NoHalt;
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
  
  removeAll();
  
  delete[] theTCPTransporters;
  delete[] theSCITransporters;
  delete[] theSHMTransporters;
  delete[] theTransporterTypes;
  delete[] theTransporters;
  delete[] performStates;
  delete[] ioStates;

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

  // read node id from client
  // read transporter type
  int nodeId, remote_transporter_type= -1;
  SocketInputStream s_input(sockfd);
  char buf[256];
  if (s_input.gets(buf, 256) == 0) {
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
      g_eventLogger.error("Incompatible configuration: Transporter type "
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
    g_eventLogger.warning("Unable to verify transporter compatability with node %d", nodeId);
  }

  // setup transporter (transporter responsible for closing sockfd)
  t->connect_server(sockfd);

  DBUG_RETURN(true);
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

#if 0
					    config->tcp.sendBufferSize,
					    config->tcp.maxReceiveSize,
					    config->localHostName,
					    config->remoteHostName,
					    config->s_port,
					    config->isMgmConnection,
					    localNodeId,
					    config->remoteNodeId,
					    config->serverNodeId,
					    config->checksum,
					    config->signalId);
#endif

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

Uint32
TransporterRegistry::get_free_buffer(Uint32 node) const
{
  Transporter *t;
  if(likely((t = theTransporters[node]) != 0))
  {
    return t->get_free_buffer();
  }
  return 0;
}


SendStatus
TransporterRegistry::prepareSend(const SignalHeader * const signalHeader, 
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
      if(lenBytes <= MAX_MESSAGE_SIZE){
	Uint32 * insertPtr = t->getWritePtr(lenBytes, prio);
	if(insertPtr != 0){
	  t->m_packer.pack(insertPtr, prio, signalHeader, signalData, ptr);
	  t->updateWritePtr(lenBytes, prio);
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
	  insertPtr = t->getWritePtr(lenBytes, prio);
	  if(insertPtr != 0){
	    t->m_packer.pack(insertPtr, prio, signalHeader, signalData, ptr);
	    t->updateWritePtr(lenBytes, prio);
	    break;
	  }
	}
	
	if(insertPtr != 0){
	  /**
	   * Send buffer full, but resend works
	   */
	  reportError(callbackObj, nodeId, TE_SEND_BUFFER_FULL);
	  return SEND_OK;
	}
	
	WARNING("Signal to " << nodeId << " lost(buffer)");
	reportError(callbackObj, nodeId, TE_SIGNAL_LOST_SEND_BUFFER_FULL);
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
TransporterRegistry::prepareSend(const SignalHeader * const signalHeader, 
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
      if(lenBytes <= MAX_MESSAGE_SIZE){
	Uint32 * insertPtr = t->getWritePtr(lenBytes, prio);
	if(insertPtr != 0){
	  t->m_packer.pack(insertPtr, prio, signalHeader, signalData, thePool, ptr);
	  t->updateWritePtr(lenBytes, prio);
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
	  insertPtr = t->getWritePtr(lenBytes, prio);
	  if(insertPtr != 0){
	    t->m_packer.pack(insertPtr, prio, signalHeader, signalData, thePool, ptr);
	    t->updateWritePtr(lenBytes, prio);
	    break;
	  }
	}
	
	if(insertPtr != 0){
	  /**
	   * Send buffer full, but resend works
	   */
	  reportError(callbackObj, nodeId, TE_SEND_BUFFER_FULL);
	  return SEND_OK;
	}
	
	WARNING("Signal to " << nodeId << " lost(buffer)");
	reportError(callbackObj, nodeId, TE_SIGNAL_LOST_SEND_BUFFER_FULL);
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
  if(nTCPTransporters > 0 || retVal == 0)
  {
    retVal |= poll_TCP(timeOutMillis);
  }
  else
    tcpReadSelectReply = 0;
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
    if (t->isConnected()) {
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
      if (t->isConnected()) {
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
    
    // If the transporter is connected
    if (t->isConnected()) {
      
      const NDB_SOCKET_TYPE socket = t->getSocket();
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
    g_eventLogger.info("woke-up by signal");

#ifdef NDB_WIN32
  if(tcpReadSelectReply == SOCKET_ERROR)
  {
    NdbSleep_MilliSleep(timeOutMillis);
  }
#endif
  
  return tcpReadSelectReply || hasdata;
}
#endif


void
TransporterRegistry::performReceive()
{
#ifdef NDB_TCP_TRANSPORTER
  for (int i=0; i<nTCPTransporters; i++) 
  {
    checkJobBuffer();
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
          transporter_recv_from(callbackObj, nodeId);
          Uint32 szUsed = unpack(ptr, sz, nodeId, ioStates[nodeId]);
          t->updateReceiveDataPtr(szUsed);
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
    checkJobBuffer();
    SCI_Transporter  *t = theSCITransporters[i];
    const NodeId nodeId = t->getRemoteNodeId();
    if(is_connected(nodeId))
    {
      if(t->isConnected() && t->checkConnected())
      {
	Uint32 * readPtr, * eodPtr;
	t->getReceivePtr(&readPtr, &eodPtr);
	transporter_recv_from(callbackObj, nodeId);
	Uint32 *newPtr = unpack(readPtr, eodPtr, nodeId, ioStates[nodeId]);
	t->updateReceivePtr(newPtr);
      }
    } 
  }
#endif
#ifdef NDB_SHM_TRANSPORTER
  for (int i=0; i<nSHMTransporters; i++) 
  {
    checkJobBuffer();
    SHM_Transporter *t = theSHMTransporters[i];
    const NodeId nodeId = t->getRemoteNodeId();
    if(is_connected(nodeId)){
      if(t->isConnected() && t->checkConnected())
      {
	Uint32 * readPtr, * eodPtr;
	t->getReceivePtr(&readPtr, &eodPtr);
	transporter_recv_from(callbackObj, nodeId);
	Uint32 *newPtr = unpack(readPtr, eodPtr, nodeId, ioStates[nodeId]);
	t->updateReceivePtr(newPtr);
      }
    } 
  }
#endif
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
    if (t && t->hasDataToSend() && t->isConnected() &&
	is_connected(t->getRemoteNodeId())) 
    {
      t->doSend();
    }
  }
  for (i = 0; i < m_transp_count && i < nTCPTransporters; i++) 
  {
    TCP_Transporter *t = theTCPTransporters[i];
    if (t && t->hasDataToSend() && t->isConnected() &&
	is_connected(t->getRemoteNodeId())) 
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
      if(t->isConnected() && t->hasDataToSend()) {
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

// Run by kernel thread
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
void
TransporterRegistry::do_disconnect(NodeId node_id)
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
  DBUG_VOID_RETURN;
}

void
TransporterRegistry::report_connect(NodeId node_id)
{
  DBUG_ENTER("TransporterRegistry::report_connect");
  DBUG_PRINT("info",("performStates[%d]=CONNECTED",node_id));
  performStates[node_id] = CONNECTED;
  reportConnect(callbackObj, node_id);
  DBUG_VOID_RETURN;
}

void
TransporterRegistry::report_disconnect(NodeId node_id, int errnum)
{
  DBUG_ENTER("TransporterRegistry::report_disconnect");
  DBUG_PRINT("info",("performStates[%d]=DISCONNECTED",node_id));
  performStates[node_id] = DISCONNECTED;
  reportDisconnect(callbackObj, node_id, errnum);
  DBUG_VOID_RETURN;
}

void
TransporterRegistry::update_connections()
{
  for (int i= 0, n= 0; n < nTransporters; i++){
    Transporter * t = theTransporters[i];
    if (!t)
      continue;
    n++;

    const NodeId nodeId = t->getRemoteNodeId();
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
	report_disconnect(nodeId, 0);
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
		g_eventLogger.info("Failed to get dynamic port to connect to: %d", res);
		ndb_mgm_disconnect(m_mgm_handle);
	      }
	      else
	      {
		g_eventLogger.info("Management server closed connection early. "
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
  m_run_start_clients_thread= true;
  m_start_clients_thread= NdbThread_Create(run_start_clients_C,
					   (void**)this,
					   32768,
					   "ndb_start_clients",
					   NDB_THREAD_PRIO_LOW);
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
    g_eventLogger.error("TransporterRegistry::startReceiving: localNodeId not specified");
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
	g_eventLogger.error("Unable to setup transporter service port: %s:%d!\n"
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
      g_eventLogger.error("Failed to install signal handler for"
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
    g_eventLogger.error("%s: %d", __FILE__, __LINE__);
    return false;
  }
  Transporter * t = theTransporters[mgm_nodeid];
  if (!t)
  {
    g_eventLogger.error("%s: %d", __FILE__, __LINE__);
    return false;
  }
  DBUG_RETURN(t->connect_client(connect_ndb_mgmd(h)));
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
    g_eventLogger.error("%s: %d", __FILE__, __LINE__);
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
      g_eventLogger.error("Error: %s: %d",
	       ndb_mgm_get_latest_error_desc(*h),
	       ndb_mgm_get_latest_error(*h));
      g_eventLogger.error("%s: %d", __FILE__, __LINE__);
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
    g_eventLogger.error("Error: %s: %d",
	     ndb_mgm_get_latest_error_desc(*h),
	     ndb_mgm_get_latest_error(*h));
    g_eventLogger.error("%s: %d", __FILE__, __LINE__);
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

template class Vector<TransporterRegistry::Transporter_interface>;
