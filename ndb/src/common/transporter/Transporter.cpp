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


#include "Transporter.hpp"
#include "TransporterInternalDefinitions.hpp"
#include <NdbSleep.h>

Transporter::Transporter(NodeId lNodeId, NodeId rNodeId, 
			 int _byteorder, 
			 bool _compression, bool _checksum, bool _signalId)
  : localNodeId(lNodeId), remoteNodeId(rNodeId),
    m_packer(_signalId, _checksum)
{
  byteOrder       = _byteorder;
  compressionUsed = _compression;
  checksumUsed    = _checksum;
  signalIdUsed    = _signalId;

  _threadError = TE_NO_ERROR;  

  _connecting    = false;
  _disconnecting = false;
  _connected     = false;
  _timeOutMillis = 1000;
  theThreadPtr   = NULL;
  theMutexPtr    = NdbMutex_Create();
}

Transporter::~Transporter(){
  NdbMutex_Destroy(theMutexPtr);

  if(theThreadPtr != 0){
    void * retVal;
    NdbThread_WaitFor(theThreadPtr, &retVal);
    NdbThread_Destroy(&theThreadPtr);
  }
}

extern "C" 
void *
runConnect_C(void * me)
{
  runConnect(me);
  NdbThread_Exit(0);
  return NULL;
}

void *
runConnect(void * me){
  Transporter * t = (Transporter *) me;

  DEBUG("Connect thread to " << t->remoteNodeId << " started");

  while(true){
    NdbMutex_Lock(t->theMutexPtr);
    if(t->_disconnecting){
      t->_connecting = false;
      NdbMutex_Unlock(t->theMutexPtr);
      DEBUG("Connect Thread " << t->remoteNodeId << " stop due to disconnect");
      return 0;
    }
    NdbMutex_Unlock(t->theMutexPtr);
    
    bool res = t->connectImpl(t->_timeOutMillis); // 1000 ms
    DEBUG("Waiting for " << t->remoteNodeId << "...");
    if(res){
      t->_connected  = true;
      t->_connecting = false;
      t->_errorCount = 0;
      t->_threadError = TE_NO_ERROR;
      DEBUG("Connect Thread " << t->remoteNodeId << " stop due to connect");
      return 0;
    }
  }
}

void
Transporter::doConnect() {
  
  NdbMutex_Lock(theMutexPtr);
  if(_connecting || _disconnecting || _connected){
    NdbMutex_Unlock(theMutexPtr);
    return;
  }
  
  _connecting = true;

  _threadError = TE_NO_ERROR;

  // Start thread
  
  char buf[16];
  snprintf(buf, sizeof(buf), "ndb_con_%d", remoteNodeId);

  if(theThreadPtr != 0){
    void * retVal;
    NdbThread_WaitFor(theThreadPtr, &retVal);
    NdbThread_Destroy(&theThreadPtr);
  }
  
  theThreadPtr = NdbThread_Create(runConnect_C,
				  (void**)this,
				  32768,
				  buf, 
                                  NDB_THREAD_PRIO_LOW);
  
  NdbSleep_MilliSleep(100); // Let thread start
  
  NdbMutex_Unlock(theMutexPtr);
}

void
Transporter::doDisconnect() {  
  
  NdbMutex_Lock(theMutexPtr);
  _disconnecting = true;
  while(_connecting){
    DEBUG("Waiting for connect to finish...");
    
    NdbMutex_Unlock(theMutexPtr);
    NdbSleep_MilliSleep(500);
    NdbMutex_Lock(theMutexPtr);
  }
    
  _connected     = false;
  
  disconnectImpl();
  _threadError = TE_NO_ERROR;
  _disconnecting = false;
  
  NdbMutex_Unlock(theMutexPtr);
}
