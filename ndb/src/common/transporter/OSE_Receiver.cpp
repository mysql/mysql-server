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

#include <NdbOut.hpp>
#include "OSE_Receiver.hpp"
#include "OSE_Transporter.hpp"
#include "TransporterCallback.hpp"
#include <TransporterRegistry.hpp>
#include "TransporterInternalDefinitions.hpp"

OSE_Receiver::OSE_Receiver(TransporterRegistry * tr,
			   int _recBufSize,
			   NodeId _localNodeId) {
  theTransporterRegistry = tr;
  
  recBufSize       = _recBufSize;
  recBufReadIndex  = 0;
  recBufWriteIndex = 0;
  receiveBuffer = new union SIGNAL * [recBufSize];

  waitStackCount   = 0;
  waitStackSize    = _recBufSize;
  waitStack = new union SIGNAL * [waitStackSize];

  nextSigId = new Uint32[MAX_NTRANSPORTERS];
  for (int i = 0; i < MAX_NTRANSPORTERS; i++)
    nextSigId[i] = 0;

  phantomCreated = false;
  localNodeId    = _localNodeId;
  BaseString::snprintf(localHostName, sizeof(localHostName), 
	   "ndb_node%d", localNodeId);

  DEBUG("localNodeId = " << localNodeId << " -> localHostName = " 
	<< localHostName);
}

OSE_Receiver::~OSE_Receiver(){
  while(recBufReadIndex != recBufWriteIndex){
    free_buf(&receiveBuffer[recBufReadIndex]);
    recBufReadIndex = (recBufReadIndex + 1) % recBufSize;
  }
  delete [] receiveBuffer;
  destroyPhantom();
}

PROCESS
OSE_Receiver::createPhantom(){
  redir.sig = 1;
  redir.pid = current_process();

  if(!phantomCreated){
    phantomPid = create_process
      (OS_PHANTOM,    // Type
       localHostName, // Name
       NULL,          // Entry point
       0,             // Stack size
       0,             // Prio - Not used
       (OSTIME)0,     // Timeslice - Not used
       0,             // Block - current block
       &redir, 
       (OSVECTOR)0,   // vector
       (OSUSER)0);    // user
    phantomCreated = true;
    DEBUG("Created phantom pid: " << hex << phantomPid);
  }
  return phantomPid;
}

void
OSE_Receiver::destroyPhantom(){
  if(phantomCreated){
    DEBUG("Destroying phantom pid: " << hex << phantomPid);
    kill_proc(phantomPid);
    phantomCreated = false;
  }
}

static SIGSELECT PRIO_A_SIGNALS[] = { 6,
				      NDB_TRANSPORTER_PRIO_A,
				      NDB_TRANSPORTER_HUNT,
				      NDB_TRANSPORTER_CONNECT_REQ,
				      NDB_TRANSPORTER_CONNECT_REF,
				      NDB_TRANSPORTER_CONNECT_CONF,
				      NDB_TRANSPORTER_DISCONNECT_ORD
};

static SIGSELECT PRIO_B_SIGNALS[] = { 1, 
				      NDB_TRANSPORTER_DATA 
};

/**
 * Check waitstack for signals that are next in sequence	    
 * Put any found signal in receive buffer
 * Returns true if one signal is found
 */
bool 
OSE_Receiver::checkWaitStack(NodeId _nodeId){

  for(int i = 0; i < waitStackCount; i++){
    if (waitStack[i]->dataSignal.senderNodeId == _nodeId && 
        waitStack[i]->dataSignal.sigId == nextSigId[_nodeId]){
      
      ndbout_c("INFO: signal popped from waitStack, sigId = %d",
               waitStack[i]->dataSignal.sigId);	   
      
      if(isFull()){
        ndbout_c("ERROR: receiveBuffer is full");
	reportError(callbackObj, _nodeId, TE_RECEIVE_BUFFER_FULL);
	return false;
      }
      
      // The next signal was found, put it in the receive buffer
      insertReceiveBuffer(waitStack[i]);
      
      // Increase sequence id, set it to the next expected id
      nextSigId[_nodeId]++;
      
      // Move signals below up one step
      for(int j = i; j < waitStackCount-1; j++)
        waitStack[j] = waitStack[j+1];
      waitStack[waitStackCount] = NULL;
      waitStackCount--;
      
      // return true since signal was found
      return true;			   
    }
  }
  return false;
}

/**
 * Clear waitstack for signals from node with _nodeId
 */
void
OSE_Receiver::clearWaitStack(NodeId _nodeId){
  
  for(int i = 0; i < waitStackCount; i++){
    if (waitStack[i]->dataSignal.senderNodeId == _nodeId){
      
      // Free signal buffer
      free_buf(&waitStack[i]);
      
      // Move signals below up one step
      for(int j = i; j < waitStackCount-1; j++)
        waitStack[j] = waitStack[j+1];
      waitStack[waitStackCount] = NULL;
      waitStackCount--;
    }
  }
  nextSigId[_nodeId] = 0;
}


inline 
void 
OSE_Receiver::insertWaitStack(union SIGNAL* _sig){
  if (waitStackCount <= waitStackSize){
    waitStack[waitStackCount] = _sig;
    waitStackCount++;
  } else {	    
    ndbout_c("ERROR: waitStack is full");
    reportError(callbackObj, localNodeId, TE_WAIT_STACK_FULL);
  }
}

bool 
OSE_Receiver::doReceive(Uint32 timeOutMillis) {
  if(isFull())
    return false;
  
  union SIGNAL * sig = receive_w_tmo(0,
				     PRIO_A_SIGNALS);
  if(sig == NIL){
    sig = receive_w_tmo(timeOutMillis,
			PRIO_B_SIGNALS);
    if(sig == NIL)
      return false;
  }
  
  DEBUG("Received signal: " << sig->sigNo << " " 
	<< sigNo2String(sig->sigNo));
  
  switch(sig->sigNo){
  case NDB_TRANSPORTER_PRIO_A:
    {
      OSE_Transporter * t = getTransporter(sig->dataSignal.senderNodeId);
      if (t != 0 && t->isConnected()){
	insertReceiveBuffer(sig);
      } else {
	free_buf(&sig);	
      }
    }
    break;
  case NDB_TRANSPORTER_DATA:
    {
      OSE_Transporter * t = getTransporter(sig->dataSignal.senderNodeId);
      if (t != 0 && t->isConnected()){     
	int nodeId = sig->dataSignal.senderNodeId;
	Uint32 currSigId = sig->dataSignal.sigId;
      
	/**
	 * Check if signal is the next in sequence
	 * nextSigId is always set to the next sigId to wait for
	 */
	if (nextSigId[nodeId] == currSigId){
	  
	  // Insert in receive buffer
	  insertReceiveBuffer(sig);
	  
	  // Increase sequence id, set it to the next expected id
	  nextSigId[nodeId]++;
	  
	  // Check if there are any signal in the wait stack
	  if (waitStackCount > 0){
	    while(checkWaitStack(nodeId));
	  }
	} else {
	  // Signal was not received in correct order
	  // Check values and put it in the waitStack
	  ndbout_c("WARNING: sigId out of order,"
		   " currSigId = %d, nextSigId = %d", 
		   currSigId,  nextSigId[nodeId]);
	  
	  if (currSigId < nextSigId[nodeId]){
	    // Current recieved sigId was smaller than nextSigId
	    // There is no use to put it in the waitStack
	    ndbout_c("ERROR: recieved sigId was smaller than nextSigId");
	    reportError(callbackObj, nodeId, TE_TOO_SMALL_SIGID);
	    return false;
	  }
	  
	  if (currSigId > (nextSigId[nodeId] + waitStackSize)){
	    // Current sigId was larger than nextSigId + size of waitStack
	    // we can never "save" so many signal's on the stack
	    ndbout_c("ERROR: currSigId >  (nextSigId + size of waitStack)"); 
	    reportError(callbackObj, nodeId, TE_TOO_LARGE_SIGID);
	    return false;
	  }
	  
	  // Insert in wait stack
	  insertWaitStack(sig);
	}        
      } else {
	free_buf(&sig);
      }
    }
    break;
  case NDB_TRANSPORTER_HUNT:
    {
      NdbTransporterHunt * s = (NdbTransporterHunt*)sig;
      OSE_Transporter * t = getTransporter(s->remoteNodeId);
      if(t != 0)
	t->huntReceived(s);
      free_buf(&sig);
    }
    break;
  case NDB_TRANSPORTER_CONNECT_REQ:
    {
      NdbTransporterConnectReq * s = (NdbTransporterConnectReq*)sig;
      OSE_Transporter * t = getTransporter(s->senderNodeId);
      if(t != 0){
	if(t->connectReq(s)){
	  clearWaitStack(s->senderNodeId);
	  clearRecvBuffer(s->senderNodeId);
	}
      }
      free_buf(&sig);
    }
    break;
  case NDB_TRANSPORTER_CONNECT_REF:
    {
      NdbTransporterConnectRef * s = (NdbTransporterConnectRef*)sig;
      OSE_Transporter * t = getTransporter(s->senderNodeId);
      if(t != 0){
	if(t->connectRef(s)){
	  clearWaitStack(s->senderNodeId);
	  clearRecvBuffer(s->senderNodeId);
	}
      }
      free_buf(&sig);
    }
    break;
  case NDB_TRANSPORTER_CONNECT_CONF:
    {
      NdbTransporterConnectConf * s = (NdbTransporterConnectConf*)sig;
      OSE_Transporter * t = getTransporter(s->senderNodeId);
      if(t != 0){
	if(t->connectConf(s)){
	  clearWaitStack(s->senderNodeId);
	  clearRecvBuffer(s->senderNodeId);
	}
      }
      free_buf(&sig);
    }
    break;
  case NDB_TRANSPORTER_DISCONNECT_ORD:
    {
      NdbTransporterDisconnectOrd * s = (NdbTransporterDisconnectOrd*)sig;
      OSE_Transporter * t = getTransporter(s->senderNodeId);
      if(t != 0){
	if(t->disconnectOrd(s)){
	  clearWaitStack(s->senderNodeId);
	  clearRecvBuffer(s->senderNodeId);
	}
      }
      free_buf(&sig);
    }
  }
  return true;
}

OSE_Transporter * 
OSE_Receiver::getTransporter(NodeId nodeId){
  if(theTransporterRegistry->theTransporterTypes[nodeId] != tt_OSE_TRANSPORTER)
    return 0;
  return (OSE_Transporter *)
    theTransporterRegistry->theTransporters[nodeId];
}

void
OSE_Receiver::clearRecvBuffer(NodeId nodeId){
  int tmpIndex = 0;
  union SIGNAL** tmp = new union SIGNAL * [recBufSize];

  /**
   * Put all signal that I want to keep into tmp
   */
  while(recBufReadIndex != recBufWriteIndex){
    if(receiveBuffer[recBufReadIndex]->dataSignal.senderNodeId != nodeId){
      tmp[tmpIndex] = receiveBuffer[recBufReadIndex];
      tmpIndex++;
    } else {
      free_buf(&receiveBuffer[recBufReadIndex]);
    }
    recBufReadIndex = (recBufReadIndex + 1) % recBufSize;
  }

  /**
   * Put all signals that I kept back into receiveBuffer
   */
  for(int i = 0; i<tmpIndex; i++)
    insertReceiveBuffer(tmp[i]);
  
  delete [] tmp;
}
