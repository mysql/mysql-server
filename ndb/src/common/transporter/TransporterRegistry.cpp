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

#include "TransporterRegistry.hpp"
#include "TransporterInternalDefinitions.hpp"

#include "Transporter.hpp"

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
#define STEPPING 1

TransporterRegistry::TransporterRegistry(void * callback,
					 unsigned _maxTransporters,
					 unsigned sizeOfLongSignalMemory) {

  nodeIdSpecified = false;
  maxTransporters = _maxTransporters;
  sendCounter = 1;
  m_ccCount = 0;
  m_ccIndex = 0;
  m_ccStep = STEPPING;
  m_ccReady = false;
  m_nTransportersPerformConnect=0;
  
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
    performStates[i]      = PerformNothing;
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
    

  TCP_Transporter * t = new TCP_Transporter(config->sendBufferSize, 
					    config->maxReceiveSize, 
					    config->port,
					    config->remoteHostName,
					    config->localHostName,
					    config->remoteNodeId,
					    localNodeId,
					    config->byteOrder,
					    config->compression,
					    config->checksum,
					    config->signalId);
  if (t == NULL) 
    return false;
  else if (!t->initTransporter()) {
    delete t;
    return false;
  }

  t->setCallbackObject(callbackObj);
  
  // Put the transporter in the transporter arrays
  theTCPTransporters[nTCPTransporters]      = t;
  theTransporters[t->getRemoteNodeId()]     = t;
  theTransporterTypes[t->getRemoteNodeId()] = tt_TCP_TRANSPORTER;
  performStates[t->getRemoteNodeId()]       = PerformNothing;
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
					    conf->byteOrder,
					    conf->compression,
					    conf->checksum,
					    conf->signalId);
  if (t == NULL)
    return false;
  else if (!t->initTransporter()) {
    delete t;
    return false;
  }
  t->setCallbackObject(callbackObj);  
  // Put the transporter in the transporter arrays
  theOSETransporters[nOSETransporters]      = t;
  theTransporters[t->getRemoteNodeId()]     = t;
  theTransporterTypes[t->getRemoteNodeId()] = tt_OSE_TRANSPORTER;
  performStates[t->getRemoteNodeId()]       = PerformNothing;
  
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
 
  SCI_Transporter * t = new SCI_Transporter(config->sendLimit, 
					    config->bufferSize,
					    config->nLocalAdapters,
					    config->remoteSciNodeId0,
					    config->remoteSciNodeId1,
					    localNodeId,
					    config->remoteNodeId,
					    config->byteOrder,
					    config->compression,
					    config->checksum,
					    config->signalId);
  
  if (t == NULL) 
    return false;
  else if (!t->initTransporter()) {
    delete t;
    return false;
  }
  t->setCallbackObject(callbackObj);
  // Put the transporter in the transporter arrays
  theSCITransporters[nSCITransporters]      = t;
  theTransporters[t->getRemoteNodeId()]     = t;
  theTransporterTypes[t->getRemoteNodeId()] = tt_SCI_TRANSPORTER;
  performStates[t->getRemoteNodeId()]       = PerformNothing;
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

  SHM_Transporter * t = new SHM_Transporter(config->localNodeId,
					    config->remoteNodeId,
					    config->shmKey,
					    config->shmSize,
					    config->compression,
					    config->checksum,
					    config->signalId
					    );
  if (t == NULL)
    return false;
  else if (!t->initTransporter()) {
    delete t;
    return false;
  }
  t->setCallbackObject(callbackObj);
  // Put the transporter in the transporter arrays
  theSHMTransporters[nSHMTransporters]      = t;
  theTransporters[t->getRemoteNodeId()]     = t;
  theTransporterTypes[t->getRemoteNodeId()] = tt_SHM_TRANSPORTER;
  performStates[t->getRemoteNodeId()]       = PerformNothing;
  
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
      if(performStates[nodeId] == PerformIO){
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
      if(performStates[nodeId] == PerformIO){
	if(t->isConnected() && t->checkConnected()){
	  Uint32 * readPtr, * eodPtr;
	  t->getReceivePtr(&readPtr, &eodPtr);
	  readPtr = unpack(readPtr, eodPtr, nodeId, ioStates[nodeId]);
	  t->updateReceivePtr(readPtr);
	}
      } 
    }
#endif
#ifdef NDB_SHM_TRANSPORTER
    for (int i=0; i<nSHMTransporters; i++) {
      checkJobBuffer();
      SHM_Transporter *t = theSHMTransporters[i];
      const NodeId nodeId = t->getRemoteNodeId();
      if(performStates[nodeId] == PerformIO){
	if(t->isConnected() && t->checkConnected()){
	  Uint32 * readPtr, * eodPtr;
	  t->getReceivePtr(&readPtr, &eodPtr);
	  readPtr = unpack(readPtr, eodPtr, nodeId, ioStates[nodeId]);
	  t->updateReceivePtr(readPtr);
	}
      } 
    }
#endif
}

static int x = 0;
void
TransporterRegistry::performSend(){
    
    sendCounter = 1;
    
#ifdef NDB_OSE_TRANSPORTER
    for (int i = 0; i < nOSETransporters; i++){
        OSE_Transporter *t = theOSETransporters[i];
        if((performStates[t->getRemoteNodeId()] == PerformIO) &&
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
        for (int i = 0; i < nTCPTransporters; i++) {
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
        for (int i = 0; i < nTCPTransporters; i++) {
            TCP_Transporter *t = theTCPTransporters[i];
            const NodeId nodeId = t->getRemoteNodeId();
            const int socket    = t->getSocket();
            if(performStates[nodeId] == PerformIO){
                if(t->isConnected() && FD_ISSET(socket, &writeset)) {
                    t->doSend();
                }//if
            }//if
        }//for
    }
#endif
#ifdef NDB_TCP_TRANSPORTER
    for (int i = x; i < nTCPTransporters; i++) {
        TCP_Transporter *t = theTCPTransporters[i];
        if (t &&
            (t->hasDataToSend()) &&
            (t->isConnected()) &&
            (performStates[t->getRemoteNodeId()] == PerformIO)) {
            t->doSend();
        }//if
    }//for
    for (int i = 0; i < x && i < nTCPTransporters; i++) {
        TCP_Transporter *t = theTCPTransporters[i];
        if (t &&
            (t->hasDataToSend()) &&
            (t->isConnected()) &&
            (performStates[t->getRemoteNodeId()] == PerformIO)) {
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
    for (int i=0; i<nSCITransporters; i++) {
      SCI_Transporter  *t = theSCITransporters[i];
      const NodeId nodeId = t->getRemoteNodeId();
      
      if(performStates[nodeId] == PerformIO){
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

PerformState
TransporterRegistry::performState(NodeId nodeId) { 
  return performStates[nodeId]; 
}

#ifdef DEBUG_TRANSPORTER
const char *
performStateString(PerformState state){
  switch(state){
  case PerformNothing:
    return "PerformNothing";
    break;
  case PerformIO:
    return "PerformIO";
    break;
  case PerformConnect:
    return "PerformConnect";
    break;
  case PerformDisconnect:
    return "PerformDisconnect";
    break;
  case RemoveTransporter:
    return "RemoveTransporter";
    break;
  }
  return "Unknown";
}
#endif

void
TransporterRegistry::setPerformState(NodeId nodeId, PerformState state) {
  DEBUG("TransporterRegistry::setPerformState(" 
	<< nodeId << ", " << performStateString(state) << ")");
  
  performStates[nodeId] = state;
}

void
TransporterRegistry::setPerformState(PerformState state) {
  int count = 0;
  int index = 0;
  while(count < nTransporters){
    if(theTransporters[index] != 0){
      setPerformState(theTransporters[index]->getRemoteNodeId(), state);
      count ++;
    }
    index ++;
  }
}

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

void
TransporterRegistry::startReceiving(){
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

/**
 * The old implementation did not scale with a large
 * number of nodes. (Watchdog killed NDB because
 * it took too long time to allocated threads in 
 * doConnect.
 *
 * The new implementation only checks the connection
 * for a number of transporters (STEPPING), until to
 * the point where all transporters has executed 
 * doConnect once. After that, the behaviour is as 
 * in the old implemenation, i.e, checking the connection
 * for all transporters. 
 * @todo: instead of STEPPING, maybe we should only
 * allow checkConnections to execute for a certain
 * time that somehow factors in heartbeat times and
 * watchdog times.
 * 
 */

void
TransporterRegistry::checkConnections(){
  if(m_ccStep > nTransporters)
    m_ccStep = nTransporters;

  while(m_ccCount < m_ccStep){
    if(theTransporters[m_ccIndex] != 0){
      Transporter * t = theTransporters[m_ccIndex];
      const NodeId nodeId = t->getRemoteNodeId();
      if(t->getThreadError() != 0) {
	reportError(callbackObj, nodeId, t->getThreadError()); 
	t->resetThreadError();
      }
      
      switch(performStates[nodeId]){
      case PerformConnect:
	if(!t->isConnected()){
	  t->doConnect();
	  if(m_nTransportersPerformConnect!=nTransporters)
	    m_nTransportersPerformConnect++;
	    
	} else {
	  performStates[nodeId] = PerformIO;
	  reportConnect(callbackObj, nodeId);
	}
	break;
      case PerformDisconnect:
	{
	  bool wasConnected = t->isConnected();
	  t->doDisconnect();
	  performStates[nodeId] = PerformNothing;
	  if(wasConnected){
	    reportDisconnect(callbackObj, nodeId,0);
	  }
	}
	break;
      case RemoveTransporter:
	removeTransporter(nodeId);
	break;
      case PerformNothing:
      case PerformIO:
	break;
      }
      m_ccCount ++;
    }
    m_ccIndex ++;
  }
  
  if(!m_ccReady) {
    if(m_ccCount < nTransporters) {
      if(nTransporters - m_ccStep < STEPPING)
	m_ccStep += nTransporters-m_ccStep;
      else
	m_ccStep += STEPPING;
      
      //      ndbout_c("count %d step %d ", m_ccCount, m_ccStep);
    }  
    else {
      m_ccCount = 0;
      m_ccIndex = 0;
      m_ccStep = STEPPING;
      //     ndbout_c("count %d step %d ", m_ccCount, m_ccStep);
    }
  }
  if((nTransporters == m_nTransportersPerformConnect) || m_ccReady) {
    m_ccReady = true;
    m_ccCount = 0;
    m_ccIndex = 0;
    m_ccStep = nTransporters;
    //    ndbout_c("alla count %d step %d ", m_ccCount, m_ccStep);
  }  

}//TransporterRegistry::checkConnections()

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
