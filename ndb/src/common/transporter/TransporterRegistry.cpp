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

#include <ndb_global.h>
#include <my_pthread.h>

#include <TransporterRegistry.hpp>
#include "TransporterInternalDefinitions.hpp"

#include "Transporter.hpp"
#include <SocketAuthenticator.hpp>

#ifdef NDB_TCP_TRANSPORTER
#include "TCP_Transporter.hpp"
#endif

#ifdef NDB_OSE_TRANSPORTER
#include "OSE_Receiver.hpp"
#include "OSE_Transporter.hpp"
#endif

#ifdef NDB_SCI_TRANSPORTER
#include "SCI_Transporter.hpp"
#endif

#ifdef NDB_SHM_TRANSPORTER
#include "SHM_Transporter.hpp"
#endif

#include "TransporterCallback.hpp"
#include "NdbOut.hpp"
#include <NdbSleep.h>
#include <NdbTick.h>
#include <InputStream.hpp>
#include <OutputStream.hpp>

SocketServer::Session * TransporterService::newSession(NDB_SOCKET_TYPE sockfd)
{
  DBUG_ENTER("SocketServer::Session * TransporterService::newSession");
  if (m_auth && !m_auth->server_authenticate(sockfd)){
    NDB_CLOSE_SOCKET(sockfd);
    DBUG_RETURN(0);
  }

  {
    // read node id from client
    int nodeId;
    SocketInputStream s_input(sockfd);
    char buf[256];
    if (s_input.gets(buf, 256) == 0) {
      NDB_CLOSE_SOCKET(sockfd);
      DBUG_PRINT("error", ("Could not get node id from client"));
      DBUG_RETURN(0);
    }
    if (sscanf(buf, "%d", &nodeId) != 1) {
      NDB_CLOSE_SOCKET(sockfd);
      DBUG_PRINT("error", ("Error in node id from client"));
      DBUG_RETURN(0);
    }

    //check that nodeid is valid and that there is an allocated transporter
    if ( nodeId < 0 || nodeId >= (int)m_transporter_registry->maxTransporters) {
      NDB_CLOSE_SOCKET(sockfd); 
      DBUG_PRINT("error", ("Node id out of range from client"));
      DBUG_RETURN(0);
    }
    if (m_transporter_registry->theTransporters[nodeId] == 0) {
      NDB_CLOSE_SOCKET(sockfd);
      DBUG_PRINT("error", ("No transporter for this node id from client"));
      DBUG_RETURN(0);
    }
    
    //check that the transporter should be connected
    if (m_transporter_registry->performStates[nodeId] != TransporterRegistry::CONNECTING) {
      NDB_CLOSE_SOCKET(sockfd);
      DBUG_PRINT("error", ("Transporter in wrong state for this node id from client"));
      DBUG_RETURN(0);
    }

    Transporter *t= m_transporter_registry->theTransporters[nodeId];

    // send info about own id (just as response to acknowledge connection)
    SocketOutputStream s_output(sockfd);
    s_output.println("%d", t->getLocalNodeId());

    // setup transporter (transporter responsible for closing sockfd)
    t->connect_server(sockfd);
  }

  DBUG_RETURN(0);
}

TransporterRegistry::TransporterRegistry(void * callback,
					 unsigned _maxTransporters,
					 unsigned sizeOfLongSignalMemory) {

  m_transporter_service= 0;
  nodeIdSpecified = false;
  maxTransporters = _maxTransporters;
  sendCounter = 1;
  
  callbackObj=callback;

  theTCPTransporters  = new TCP_Transporter * [maxTransporters];
  theSCITransporters  = new SCI_Transporter * [maxTransporters];
  theSHMTransporters  = new SHM_Transporter * [maxTransporters];
  theOSETransporters  = new OSE_Transporter * [maxTransporters];
  theTransporterTypes = new TransporterType   [maxTransporters];
  theTransporters     = new Transporter     * [maxTransporters];
  performStates       = new PerformState      [maxTransporters];
  ioStates            = new IOState           [maxTransporters]; 
  
  // Initialize member variables
  nTransporters    = 0;
  nTCPTransporters = 0;
  nSCITransporters = 0;
  nSHMTransporters = 0;
  nOSETransporters = 0;
  
  // Initialize the transporter arrays
  for (unsigned i=0; i<maxTransporters; i++) {
    theTCPTransporters[i] = NULL;
    theSCITransporters[i] = NULL;
    theSHMTransporters[i] = NULL;
    theOSETransporters[i] = NULL;
    theTransporters[i]    = NULL;
    performStates[i]      = DISCONNECTED;
    ioStates[i]           = NoHalt;
  }
  theOSEReceiver = 0;
  theOSEJunkSocketSend = 0;
  theOSEJunkSocketRecv = 0;
}

TransporterRegistry::~TransporterRegistry() {
  
  removeAll();
  
  delete[] theTCPTransporters;
  delete[] theSCITransporters;
  delete[] theSHMTransporters;
  delete[] theOSETransporters;
  delete[] theTransporterTypes;
  delete[] theTransporters;
  delete[] performStates;
  delete[] ioStates;

#ifdef NDB_OSE_TRANSPORTER
  if(theOSEReceiver != NULL){
    theOSEReceiver->destroyPhantom();
    delete theOSEReceiver;
    theOSEReceiver = 0;
  }
#endif
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
  nodeIdSpecified = true;
  localNodeId = nodeId;
  
  DEBUG("TransporterRegistry started node: " << localNodeId);
  
  //  return allocateLongSignalMemoryPool(nLargeSegments);
  return true;
}

bool
TransporterRegistry::createTransporter(TCP_TransporterConfiguration *config) {
#ifdef NDB_TCP_TRANSPORTER

  if(!nodeIdSpecified){
    init(config->localNodeId);
  }
  
  if(config->localNodeId != localNodeId) 
    return false;
  
  if(theTransporters[config->remoteNodeId] != NULL)
    return false;
   
  TCP_Transporter * t = new TCP_Transporter(*this,
					    config->sendBufferSize,
					    config->maxReceiveSize,
					    config->localHostName,
					    config->remoteHostName,
					    config->port,
					    localNodeId,
					    config->remoteNodeId,
					    config->checksum,
					    config->signalId);
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

#if defined NDB_OSE || defined NDB_SOFTOSE
  t->theReceiverPid = theReceiverPid;
#endif
  
  return true;
#else
  return false;
#endif
}

bool
TransporterRegistry::createTransporter(OSE_TransporterConfiguration *conf) {
#ifdef NDB_OSE_TRANSPORTER

  if(!nodeIdSpecified){
    init(conf->localNodeId);
  }
  
  if(conf->localNodeId != localNodeId)
    return false;
  
  if(theTransporters[conf->remoteNodeId] != NULL)
    return false;

  if(theOSEReceiver == NULL){
    theOSEReceiver = new OSE_Receiver(this,
				      10,
				      localNodeId);
  }
  
  OSE_Transporter * t = new OSE_Transporter(conf->prioASignalSize,
					    conf->prioBSignalSize,
					    localNodeId,
					    conf->localHostName,
					    conf->remoteNodeId,
					    conf->remoteHostName,
					    conf->checksum,
					    conf->signalId);
  if (t == NULL)
    return false;
  else if (!t->initTransporter()) {
    delete t;
    return false;
  }
  // Put the transporter in the transporter arrays
  theOSETransporters[nOSETransporters]      = t;
  theTransporters[t->getRemoteNodeId()]     = t;
  theTransporterTypes[t->getRemoteNodeId()] = tt_OSE_TRANSPORTER;
  performStates[t->getRemoteNodeId()]       = DISCONNECTED;
  
  nTransporters++;
  nOSETransporters++;

  return true;
#else
  return false;
#endif
}

bool
TransporterRegistry::createTransporter(SCI_TransporterConfiguration *config) {
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
                                            config->port,
                                            config->sendLimit, 
					    config->bufferSize,
					    config->nLocalAdapters,
					    config->remoteSciNodeId0,
					    config->remoteSciNodeId1,
					    localNodeId,
					    config->remoteNodeId,
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
TransporterRegistry::createTransporter(SHM_TransporterConfiguration *config) {
#ifdef NDB_SHM_TRANSPORTER
  if(!nodeIdSpecified){
    init(config->localNodeId);
  }
  
  if(config->localNodeId != localNodeId)
    return false;
  
  if(theTransporters[config->remoteNodeId] != NULL)
    return false;

  SHM_Transporter * t = new SHM_Transporter(*this,
					    "localhost",
					    "localhost",
					    config->port,
					    localNodeId,
					    config->remoteNodeId,
					    config->checksum,
					    config->signalId,
					    config->shmKey,
					    config->shmSize
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

  return true;
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
  case tt_OSE_TRANSPORTER:
#ifdef NDB_OSE_TRANSPORTER
    for(; ind < nOSETransporters; ind++)
      if(theOSETransporters[ind]->getRemoteNodeId() == nodeId)
	break;
    ind++;
    for(; ind<nOSETransporters; ind++)
      theOSETransporters[ind-1] = theOSETransporters[ind];
    nOSETransporters --;
#endif
    break;
  }
  
  nTransporters--;

  // Delete the transporter and remove it from theTransporters array
  delete theTransporters[nodeId];
  theTransporters[nodeId] = NULL;        
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
      (signalHeader->theReceiversBlockNumber == 252))) {

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
      (signalHeader->theReceiversBlockNumber == 252))) {

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
#ifdef NDB_OSE_TRANSPORTER
  retVal |= poll_OSE(timeOutMillis);
  retVal |= poll_TCP(0);
  return retVal;
#endif
  
  if((nSHMTransporters+nSCITransporters) > 0)
    timeOutMillis=0;
#ifdef NDB_TCP_TRANSPORTER
  if(nTCPTransporters > 0)
    retVal |= poll_TCP(timeOutMillis);
  else
    tcpReadSelectReply = 0;
#endif
#ifdef NDB_SCI_TRANSPORTER
  if(nSCITransporters > 0)
    retVal |= poll_SCI(timeOutMillis);
#endif
#ifdef NDB_SHM_TRANSPORTER
  if(nSHMTransporters > 0)
    retVal |= poll_SHM(timeOutMillis);
#endif
  return retVal;
}


#ifdef NDB_SCI_TRANSPORTER
Uint32
TransporterRegistry::poll_SCI(Uint32 timeOutMillis){
 
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
Uint32
TransporterRegistry::poll_SHM(Uint32 timeOutMillis)
{  
  for(int j=0; j < 20; j++)
  for (int i=0; i<nSHMTransporters; i++) {
    SHM_Transporter * t = theSHMTransporters[i];
    if (t->isConnected()) {
      if(t->hasDataToRead()) {
	return 1;
      }
    }
  }
  /**
   * @note: granularity of linux/i386 timer is not good enough.
   * Can't sleep if using SHM as it is now.
   */
  /*
    if(timeOutMillis > 0)
    NdbSleep_MilliSleep(timeOutMillis);
    else 
    NdbSleep_MilliSleep(1);
  */
  return 0;
#if 0
  NDB_TICKS startTime =   NdbTick_CurrentMillisecond();
  for(int i=0; i<100; i++) {
    for (int i=0; i<nSHMTransporters; i++) {
      SHM_Transporter * t = theSHMTransporters[i];
      if (t->isConnected()) {
	if(t->hasDataToRead()){
	  return 1;
	}
	else
	  continue;
      }
      else
	continue;
    }

    if(NdbTick_CurrentMillisecond() > (startTime +timeOutMillis))
      return 0;      
  }
  NdbSleep_MilliSleep(5);
  return 0;
  
#endif
#if 0

  for(int j=0; j < 100; j++) {
    for (int i=0; i<nSHMTransporters; i++) {
      SHM_Transporter * t = theSHMTransporters[i];
      if (t->isConnected()) {
	if(t->hasDataToRead())
	  return 1;
      }
    }
  }
  return 0;
#endif
}


#endif

#ifdef NDB_OSE_TRANSPORTER
Uint32
TransporterRegistry::poll_OSE(Uint32 timeOutMillis){
  if(theOSEReceiver != NULL){
    return theOSEReceiver->doReceive(timeOutMillis);
  }
  NdbSleep_MilliSleep(timeOutMillis);
  return 0;
}
#endif

#ifdef NDB_TCP_TRANSPORTER
Uint32 
TransporterRegistry::poll_TCP(Uint32 timeOutMillis){
  
  if (nTCPTransporters == 0){
    tcpReadSelectReply = 0;
    return 0;
  }
  
  struct timeval timeout;
#ifdef NDB_OSE

  // Return directly if there are no TCP transporters configured

  if(timeOutMillis <= 1){
    timeout.tv_sec  = 0;
    timeout.tv_usec = 1025;
  } else {
    timeout.tv_sec  = timeOutMillis / 1000;
    timeout.tv_usec = (timeOutMillis % 1000) * 1000;
  }
#else  
  timeout.tv_sec  = timeOutMillis / 1000;
  timeout.tv_usec = (timeOutMillis % 1000) * 1000;
#endif

  NDB_SOCKET_TYPE maxSocketValue = 0;
  
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
  }
  
  // The highest socket value plus one
  maxSocketValue++; 
  
  tcpReadSelectReply = select(maxSocketValue, &tcpReadset, 0, 0, &timeout);  
#ifdef NDB_WIN32
  if(tcpReadSelectReply == SOCKET_ERROR)
  {
    NdbSleep_MilliSleep(timeOutMillis);
  }
#endif

  return tcpReadSelectReply;
}
#endif


void
TransporterRegistry::performReceive(){
#ifdef NDB_OSE_TRANSPORTER
  if(theOSEReceiver != 0){
    while(theOSEReceiver->hasData()){
      NodeId remoteNodeId;
      Uint32 * readPtr;
      Uint32 sz = theOSEReceiver->getReceiveData(&remoteNodeId, &readPtr);
      Uint32 szUsed = unpack(readPtr,
			     sz,
			     remoteNodeId,
			     ioStates[remoteNodeId]);
#ifdef DEBUG_TRANSPORTER
      /**
       * OSE transporter can handle executions of
       *   half signals
       */
      assert(sz == szUsed);
#endif
      theOSEReceiver->updateReceiveDataPtr(szUsed);
      theOSEReceiver->doReceive(0);
      //      checkJobBuffer();
    }
  }
#endif

#ifdef NDB_TCP_TRANSPORTER
  if(tcpReadSelectReply > 0){
    for (int i=0; i<nTCPTransporters; i++) {
      checkJobBuffer();
      TCP_Transporter *t = theTCPTransporters[i];
      const NodeId nodeId = t->getRemoteNodeId();
      const NDB_SOCKET_TYPE socket    = t->getSocket();
      if(is_connected(nodeId)){
	if(t->isConnected() && FD_ISSET(socket, &tcpReadset)) {
	  const int receiveSize = t->doReceive();
	  if(receiveSize > 0){
	    Uint32 * ptr;
	    Uint32 sz = t->getReceiveData(&ptr);
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
    for (int i=0; i<nSCITransporters; i++) {
      checkJobBuffer();
      SCI_Transporter  *t = theSCITransporters[i];
      const NodeId nodeId = t->getRemoteNodeId();
      if(is_connected(nodeId)){
	if(t->isConnected() && t->checkConnected()){
	  Uint32 * readPtr, * eodPtr;
          Uint32 sz = 0;
	  t->getReceivePtr(&readPtr, &eodPtr);
	  Uint32 *newPtr = unpack(readPtr, eodPtr, nodeId, ioStates[nodeId]);
	  t->updateReceivePtr(newPtr);
	}
      } 
    }
#endif
#ifdef NDB_SHM_TRANSPORTER
    for (int i=0; i<nSHMTransporters; i++) {
      checkJobBuffer();
      SHM_Transporter *t = theSHMTransporters[i];
      const NodeId nodeId = t->getRemoteNodeId();
      if(is_connected(nodeId)){
	if(t->isConnected() && t->checkConnected()){
	  Uint32 * readPtr, * eodPtr;
          Uint32 sz = 0;
	  t->getReceivePtr(&readPtr, &eodPtr);
	  Uint32 *newPtr = unpack(readPtr, eodPtr, nodeId, ioStates[nodeId]);
	  t->updateReceivePtr(newPtr);
	}
      } 
    }
#endif
}

static int x = 0;
void
TransporterRegistry::performSend(){
    int i; 
    sendCounter = 1;
    
#ifdef NDB_OSE_TRANSPORTER
    for (int i = 0; i < nOSETransporters; i++){
        OSE_Transporter *t = theOSETransporters[i];
        if((is_connected(t->getRemoteNodeId()) &&
            (t->isConnected())) {
            t->doSend();
        }//if
    }//for
#endif
    
#ifdef NDB_TCP_TRANSPORTER
#ifdef NDB_OSE
    {
        int maxSocketValue = 0;
        
        // Needed for TCP/IP connections
        // The writeset are used by select
        fd_set writeset;
        FD_ZERO(&writeset);
        
        // Prepare for sending and receiving
        for (i = 0; i < nTCPTransporters; i++) {
            TCP_Transporter * t = theTCPTransporters[i];
            
            // If the transporter is connected
            if ((t->hasDataToSend()) && (t->isConnected())) {
                const int socket = t->getSocket();
                // Find the highest socket value. It will be used by select
                if (socket > maxSocketValue) {
                    maxSocketValue = socket;
                }//if
                FD_SET(socket, &writeset);
            }//if
        }//for
        
        // The highest socket value plus one
        if(maxSocketValue == 0)
            return;
        
        maxSocketValue++; 
        struct timeval timeout = { 0, 1025 };
        Uint32 tmp = select(maxSocketValue, 0, &writeset, 0, &timeout);
        
        if (tmp == 0) {
            return;
        }//if
        for (i = 0; i < nTCPTransporters; i++) {
            TCP_Transporter *t = theTCPTransporters[i];
            const NodeId nodeId = t->getRemoteNodeId();
            const int socket    = t->getSocket();
            if(is_connected(nodeId)){
                if(t->isConnected() && FD_ISSET(socket, &writeset)) {
                    t->doSend();
                }//if
            }//if
        }//for
    }
#endif
#ifdef NDB_TCP_TRANSPORTER
    for (i = x; i < nTCPTransporters; i++) {
        TCP_Transporter *t = theTCPTransporters[i];
        if (t &&
            (t->hasDataToSend()) &&
            (t->isConnected()) &&
            (is_connected(t->getRemoteNodeId()))) {
            t->doSend();
        }//if
    }//for
    for (i = 0; i < x && i < nTCPTransporters; i++) {
        TCP_Transporter *t = theTCPTransporters[i];
        if (t &&
            (t->hasDataToSend()) &&
            (t->isConnected()) &&
            (is_connected(t->getRemoteNodeId()))) {
            t->doSend();
        }//if
    }//for
    x++;
    if (x == nTCPTransporters) x = 0;
#endif
#endif
#ifdef NDB_SCI_TRANSPORTER
    //scroll through the SCI transporters, 
    // get each transporter, check if connected, send data
    for (i=0; i<nSCITransporters; i++) {
      SCI_Transporter  *t = theSCITransporters[i];
      const NodeId nodeId = t->getRemoteNodeId();
      
      if(is_connected(nodeId)){
	if(t->isConnected() && t->hasDataToSend()) {
	  t->doSend();
	} //if
      } //if
    }  //if  
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
  my_thread_init();
  ((TransporterRegistry*) me)->start_clients_thread();
  my_thread_end();
  NdbThread_Exit(0);
  return me;
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
  curr_state= CONNECTING;
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
  curr_state= DISCONNECTING;
}

void
TransporterRegistry::report_connect(NodeId node_id)
{
  performStates[node_id] = CONNECTED;
  reportConnect(callbackObj, node_id);
}

void
TransporterRegistry::report_disconnect(NodeId node_id, int errnum)
{
  performStates[node_id] = DISCONNECTED;
  reportDisconnect(callbackObj, node_id, errnum);
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
  DBUG_ENTER("TransporterRegistry::start_clients_thread");
  while (m_run_start_clients_thread) {
    NdbSleep_MilliSleep(100);
    for (int i= 0, n= 0; n < nTransporters && m_run_start_clients_thread; i++){
      Transporter * t = theTransporters[i];
      if (!t)
	continue;
      n++;

      const NodeId nodeId = t->getRemoteNodeId();
      switch(performStates[nodeId]){
      case CONNECTING:
	if(!t->isConnected() && !t->isServer)
	    t->connect_client();
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
    int r= NdbThread_WaitFor(m_start_clients_thread, &status);
    NdbThread_Destroy(&m_start_clients_thread);
  }
  return true;
}

bool
TransporterRegistry::start_service(SocketServer& socket_server)
{
#if 0
  for (int i= 0, n= 0; n < nTransporters; i++){
    Transporter * t = theTransporters[i];
    if (!t)
      continue;
    n++;
    if (t->isServer) {
      t->m_service = new TransporterService(new SocketAuthSimple("ndbd passwd"));
      if(!socket_server.setup(t->m_service, t->m_r_port, 0))
      {
	ndbout_c("Unable to setup transporter service port: %d!\n"
		 "Please check if the port is already used,\n"
		 "(perhaps a mgmt server is already running)",
		 m_service_port);
	delete t->m_service;
	return false;
      }
    }
  }
#endif

  if (m_service_port != 0) {

    m_transporter_service = new TransporterService(new SocketAuthSimple("ndbd", "ndbd passwd"));

    if (nodeIdSpecified != true) {
      ndbout_c("TransporterRegistry::startReceiving: localNodeId not specified");
      return false;
    }

    //m_interface_name = "ndbd";
    m_interface_name = 0;

    if(!socket_server.setup(m_transporter_service, m_service_port, m_interface_name))
      {
	ndbout_c("Unable to setup transporter service port: %d!\n"
		 "Please check if the port is already used,\n"
		 "(perhaps a mgmt server is already running)",
		 m_service_port);
	delete m_transporter_service;
	return false;
      }
    m_transporter_service->setTransporterRegistry(this);
  } else
    m_transporter_service= 0;

  return true;
}

void
TransporterRegistry::startReceiving()
{
#ifdef NDB_OSE_TRANSPORTER
  if(theOSEReceiver != NULL){
    theOSEReceiver->createPhantom();
  }
#endif

#ifdef NDB_OSE
  theOSEJunkSocketRecv = socket(AF_INET, SOCK_STREAM, 0);
#endif

#if defined NDB_OSE || defined NDB_SOFTOSE
  theReceiverPid = current_process();
  for(int i = 0; i<nTCPTransporters; i++)
    theTCPTransporters[i]->theReceiverPid = theReceiverPid;
#endif
}

void
TransporterRegistry::stopReceiving(){
#ifdef NDB_OSE_TRANSPORTER
  if(theOSEReceiver != NULL){
    theOSEReceiver->destroyPhantom();
  }
#endif

  /**
   * Disconnect all transporters, this includes detach from remote node
   * and since that must be done from the same process that called attach
   * it's done here in the receive thread
   */
  disconnectAll();

#if defined NDB_OSE || defined NDB_SOFTOSE
  if(theOSEJunkSocketRecv > 0)
    close(theOSEJunkSocketRecv);
  theOSEJunkSocketRecv = -1;
#endif

}

void
TransporterRegistry::startSending(){
#if defined NDB_OSE || defined NDB_SOFTOSE
  theOSEJunkSocketSend = socket(AF_INET, SOCK_STREAM, 0);
#endif
}

void
TransporterRegistry::stopSending(){
#if defined NDB_OSE || defined NDB_SOFTOSE
  if(theOSEJunkSocketSend > 0)
    close(theOSEJunkSocketSend);
  theOSEJunkSocketSend = -1;
#endif
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
