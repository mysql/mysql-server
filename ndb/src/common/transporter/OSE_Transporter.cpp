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

#include <ose.h>
#include "OSE_Transporter.hpp"
#include "OSE_Signals.hpp"

#include <TransporterCallback.hpp>
#include "TransporterInternalDefinitions.hpp"

#include <NdbMutex.h>

#include <NdbHost.h>
#include <NdbOut.hpp>
#include <time.h>

OSE_Transporter::OSE_Transporter(int _prioASignalSize,
                                 int _prioBSignalSize,
                                 NodeId localNodeId,
                                 const char * lHostName,
                                 NodeId remoteNodeId,
                                 const char * rHostName,
                                 int byteorder,
                                 bool compression, 
                                 bool checksum, 
                                 bool signalId,
                                 Uint32 reportFreq) :
  Transporter(localNodeId,
              remoteNodeId,
              byteorder,
              compression,
              checksum,
              signalId),
  isServer(localNodeId < remoteNodeId)
{

  signalIdCounter = 0;
  prioBSignalSize = _prioBSignalSize;
  
  if (strcmp(lHostName, rHostName) == 0){    
    BaseString::snprintf(remoteNodeName, sizeof(remoteNodeName), 
             "ndb_node%d", remoteNodeId);
  } else {
    BaseString::snprintf(remoteNodeName, sizeof(remoteNodeName), 
             "%s/ndb_node%d", rHostName, remoteNodeId); 
  }
  
  prioBSignal      = NIL;
}

OSE_Transporter::~OSE_Transporter() {

#if 0 
  /**
  * Don't free these buffers since they have already been freed
  * when the process allocating them died (wild pointers)
  */
  if(prioBSignal != NIL)
    free_buf(&prioBSignal);
#endif
}

bool
OSE_Transporter::initTransporter() {

  struct OS_pcb * pcb = get_pcb(current_process());
  if(pcb != NULL){
    if(pcb->type != OS_ILLEGAL){
      if(prioBSignalSize > pcb->max_sigsize){
        DEBUG("prioBSignalSize(" << prioBSignalSize << ") > max_sigsize("
              << pcb->max_sigsize << ") using max_sigsize");
        prioBSignalSize = pcb->max_sigsize;
      }
    }
    free_buf((union SIGNAL **)&pcb);
  }

  maxPrioBDataSize = prioBSignalSize;
  maxPrioBDataSize -= (sizeof(NdbTransporterData) + MAX_MESSAGE_SIZE - 4);
  
  if(maxPrioBDataSize < 0){
    
#ifdef DEBUG_TRANSPORTER
    printf("maxPrioBDataSize < 0 %d\n",
           maxPrioBDataSize);
#endif
    return false;
  }

  initSignals();
  
  return true;
}

void
OSE_Transporter::initSignals(){
  if(prioBSignal == NIL){
    prioBSignal = alloc(prioBSignalSize, NDB_TRANSPORTER_DATA);
    prioBInsertPtr = &prioBSignal->dataSignal.data[0];
  
    prioBSignal->dataSignal.length = 0;
    prioBSignal->dataSignal.senderNodeId = localNodeId;
  }
  dataToSend = 0;
}

NdbTransporterData *
OSE_Transporter::allocPrioASignal(Uint32 messageLenBytes) const
{
  
  const Uint32 lenBytes = messageLenBytes + sizeof(NdbTransporterData) - 4;
  
  NdbTransporterData * sig = 
    (NdbTransporterData*)alloc(lenBytes, NDB_TRANSPORTER_PRIO_A);
  
  sig->length = 0;
  sig->senderNodeId = localNodeId;
  
  return sig;
}

Uint32 *
OSE_Transporter::getWritePtr(Uint32 lenBytes, Uint32 prio){
  if(prio >= 1){
    prio = 1;
    insertPtr  = prioBInsertPtr;
    signal     = (NdbTransporterData*)prioBSignal;
  } else {
    signal    = allocPrioASignal(lenBytes);
    insertPtr = &signal->data[0];
  }
  return insertPtr;
}

void
OSE_Transporter::updateWritePtr(Uint32 lenBytes, Uint32 prio){

  Uint32 bufferSize = signal->length;
  bufferSize += lenBytes;
  signal->length = bufferSize;
  if(prio >= 1){
    prioBInsertPtr += (lenBytes / 4);
    if(bufferSize >= maxPrioBDataSize)
      doSend();
  } else {
    /**
     * Prio A signal are sent directly
     */
    signal->sigId = 0;
    
    ::send((union SIGNAL**)&signal, remoteNodePid);
  }
}

#if 0
int getSeq(int _seq){
  if (_seq > 0){
    switch (_seq % 100){
    case 10:
      return _seq - 1;
    case 9: 
      return _seq + 1;
    default:
      return _seq;
    }
  }else{
    return _seq;
  }
}
int getSeq(int _seq){

    switch (_seq % 40){
    case 10:       
      return _seq-4;
    case 9:
      return _seq-2;
    case 8:       
      return _seq;
    case 7:
      return _seq+2;
    case 6:       
      return _seq+4;


    case 30:       
      return _seq-9;
    case 29:
      return _seq-7;
    case 28:       
      return _seq-5;
    case 27:
      return _seq-3;
    case 26:       
      return _seq-1;
    case 25:       
      return _seq+1;
    case 24:
      return _seq+3;
    case 23:       
      return _seq+5;
    case 22:
      return _seq+7;
    case 21:       
      return _seq+9;

    default:
      return _seq;
    
    }
}
#endif

void
OSE_Transporter::doSend() {
  /**
   * restore is always called to make sure the signal buffer is taken over 
   * by a process that is alive, this will otherwise lead to that these buffers
   * are removed when the process that allocated them dies
   */
  restore(prioBSignal);
  if(prioBSignal->dataSignal.length > 0){

    prioBSignal->dataSignal.sigId = signalIdCounter;
    signalIdCounter++;

    ::send(&prioBSignal, remoteNodePid);
  }
  
  initSignals();
}

void
OSE_Transporter::doConnect() {
  
  NdbMutex_Lock(theMutexPtr);
  if(_connecting || _disconnecting || _connected){
    NdbMutex_Unlock(theMutexPtr);
    return;
  }
  
  _connecting = true;
  signalIdCounter = 0;

  if(isServer){
    DEBUG("Waiting for connect req: ");
    state = WAITING_FOR_CONNECT_REQ;
  } else {
    state = WAITING_FOR_HUNT;
    
    DEBUG("Hunting for: " << remoteNodeName);
    
    union SIGNAL* huntsig;
    huntsig = alloc(sizeof(NdbTransporterHunt), NDB_TRANSPORTER_HUNT);
    huntsig->ndbHunt.remoteNodeId = remoteNodeId;
    hunt(remoteNodeName, 0, NULL, &huntsig);
  }   
  NdbMutex_Unlock(theMutexPtr);
}

void
OSE_Transporter::doDisconnect() {  
  NdbMutex_Lock(theMutexPtr);

  switch(state){
  case DISCONNECTED:
  case WAITING_FOR_HUNT:
  case WAITING_FOR_CONNECT_REQ:
  case WAITING_FOR_CONNECT_CONF:
    break;
  case CONNECTED:
    {
#if 0      
      /** 
       * There should not be anything in the buffer that needs to be sent here
       */
      DEBUG("Doing send before disconnect");
      doSend();
#endif
      union SIGNAL * sig = alloc(sizeof(NdbTransporterDisconnectOrd),
                                 NDB_TRANSPORTER_DISCONNECT_ORD);
      sig->ndbDisconnect.senderNodeId = localNodeId;
      sig->ndbDisconnect.reason = NdbTransporterDisconnectOrd::NDB_DISCONNECT;
      ::send(&sig, remoteNodePid);
      detach(&remoteNodeRef);

    }
    break;
  }
  state = DISCONNECTED;
  
  _connected = false;
  _connecting = false;
  _disconnecting = false;

  NdbMutex_Unlock(theMutexPtr);
}

void
OSE_Transporter::huntReceived(struct NdbTransporterHunt * sig){
  if(isServer){
    WARNING("Hunt received for server: remoteNodeId: " <<
            sig->remoteNodeId);
    return;
  }
  
  if(state != WAITING_FOR_HUNT){
    WARNING("Hunt received while in state: " << state);
    return;
  }
  remoteNodePid = sender((union SIGNAL**)&sig);
  union SIGNAL * signal = alloc(sizeof(NdbTransporterConnectReq),
                                NDB_TRANSPORTER_CONNECT_REQ);
  signal->ndbConnectReq.remoteNodeId = remoteNodeId;
  signal->ndbConnectReq.senderNodeId = localNodeId;

  DEBUG("Sending connect req to pid: " << hex << remoteNodePid);
  
  ::send(&signal, remoteNodePid);
  state = WAITING_FOR_CONNECT_CONF;
  return;
}

bool
OSE_Transporter::connectReq(struct NdbTransporterConnectReq * sig){
  if(!isServer){
    WARNING("OSE Connect Req received for client: senderNodeId: " <<
            sig->senderNodeId);
    return false;
  }
  
  if(state != WAITING_FOR_CONNECT_REQ){
    PROCESS pid = sender((union SIGNAL**)&sig);
    union SIGNAL * signal = alloc(sizeof(NdbTransporterConnectRef),
                                  NDB_TRANSPORTER_CONNECT_REF);
    signal->ndbConnectRef.senderNodeId = localNodeId;
    signal->ndbConnectRef.reason = NdbTransporterConnectRef::INVALID_STATE;

    DEBUG("Sending connect ref to pid: " << hex << pid);

    ::send(&signal, pid);
    return false;
  }
  
  NdbMutex_Lock(theMutexPtr);

  if(prioBSignal != NIL){
    restore(prioBSignal);
    free_buf(&prioBSignal);
  }
  initSignals();

  remoteNodePid = sender((union SIGNAL**)&sig);
  union SIGNAL * signal = alloc(sizeof(NdbTransporterConnectRef),
                                NDB_TRANSPORTER_CONNECT_CONF);
  signal->ndbConnectConf.senderNodeId = localNodeId;
  signal->ndbConnectConf.remoteNodeId = remoteNodeId;

  union SIGNAL * discon = alloc(sizeof(NdbTransporterDisconnectOrd),
                                NDB_TRANSPORTER_DISCONNECT_ORD);
  discon->ndbDisconnect.senderNodeId = remoteNodeId;
  discon->ndbDisconnect.reason = NdbTransporterDisconnectOrd::PROCESS_DIED;
  
  DEBUG("Attaching to pid: " << hex << remoteNodePid);

  remoteNodeRef = attach(&discon, remoteNodePid);
  
  DEBUG("Sending connect conf to pid: " << hex << remoteNodePid);

  ::send(&signal, remoteNodePid);
  state = CONNECTED;
  
  _connected     = true;
  _connecting    = false;
  _disconnecting = false;

  NdbMutex_Unlock(theMutexPtr);
  
  return true;
}

bool
OSE_Transporter::connectRef(struct NdbTransporterConnectRef * sig){
  if(isServer){
    WARNING("OSE Connect Ref received for server: senderNodeId: " <<
            sig->senderNodeId);
    return false;
  }
  if(state != WAITING_FOR_CONNECT_CONF){
    WARNING("OSE Connect Ref received for client while in state: " <<
            state << " senderNodeId: " << sig->senderNodeId);
    return false;
  }
  doDisconnect();
#if 0
  /** 
   * Don't call connect directly, wait until the next time 
   * checkConnections is called which will trigger a new connect attempt
   */
  doConnect();
#endif
  return true;
}


bool
OSE_Transporter::connectConf(struct NdbTransporterConnectConf * sig){
  if(isServer){
    WARNING("OSE Connect Conf received for server: senderNodeId: " <<
            sig->senderNodeId);
    return false;
  }
  if(state != WAITING_FOR_CONNECT_CONF){
    WARNING("OSE Connect Conf received while in state: " <<
            state);
    return false;
  }
  NdbMutex_Lock(theMutexPtr);

  // Free the buffers to get rid of any "junk" that they might contain
  if(prioBSignal != NIL){
    restore(prioBSignal);
    free_buf(&prioBSignal);
  }
  initSignals();

  union SIGNAL * discon = alloc(sizeof(NdbTransporterDisconnectOrd),
                                NDB_TRANSPORTER_DISCONNECT_ORD);
  discon->ndbDisconnect.senderNodeId = remoteNodeId;
  discon->ndbDisconnect.reason= NdbTransporterDisconnectOrd::PROCESS_DIED;
  
  remoteNodeRef = attach(&discon, remoteNodePid);
  
  state = CONNECTED;
  _connected     = true;
  _connecting    = false;
  _disconnecting = false;

  // Free the buffers to get rid of any "junk" that they might contain
  if(prioBSignal != NIL){
    restore(prioBSignal);
    free_buf(&prioBSignal);
  }
  initSignals();
  
  NdbMutex_Unlock(theMutexPtr);
  return true;
}


bool
OSE_Transporter::disconnectOrd(struct NdbTransporterDisconnectOrd * sig){
  if(state != CONNECTED){
    WARNING("OSE Disconnect Ord received while in state: " << state <<
            " reason: " << sig->reason);
    return false;
  }

  if(sig->reason == NdbTransporterDisconnectOrd::PROCESS_DIED){
    state = DISCONNECTED;
  }
  
  doDisconnect();
  reportDisconnect(callbackObj, remoteNodeId,0);
  return true;
}







