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

#ifndef OSE_RECEIVER_HPP
#define OSE_RECEIVER_HPP

#include "ose.h"
#include "OSE_Signals.hpp"
#include <kernel_types.h>

class OSE_Receiver {
public:
  OSE_Receiver(class TransporterRegistry *,
	       int recBufSize,
	       NodeId localNodeId);

  ~OSE_Receiver();

  bool hasData() const ;
  bool isFull() const ;
  
  Uint32 getReceiveData(NodeId * remoteNodeId,
			Uint32 ** readPtr);
  
  void updateReceiveDataPtr(Uint32 szRead);
  
  bool doReceive(Uint32 timeOutMillis);

  PROCESS createPhantom();
  void destroyPhantom();
  
private:
  class TransporterRegistry * theTransporterRegistry;
  
  NodeId localNodeId;
  char localHostName[255];
  
  bool phantomCreated;
  PROCESS phantomPid;
  struct OS_redir_entry redir;
  
  int recBufReadIndex;
  int recBufWriteIndex;
  int recBufSize;
  union SIGNAL **receiveBuffer;

  // Stack for signals that are received out of order
  int waitStackCount;
  int waitStackSize;
  union SIGNAL** waitStack;

  // Counters for the next signal id
  Uint32* nextSigId;

  class OSE_Transporter * getTransporter(NodeId nodeId);

  void insertReceiveBuffer(union SIGNAL * _sig);
  void clearRecvBuffer(NodeId _nodeId);
  bool checkWaitStack(NodeId _nodeId);
  void clearWaitStack(NodeId _nodeId);
  void insertWaitStack(union SIGNAL* _sig);
};

inline
bool
OSE_Receiver::hasData () const {
  return recBufReadIndex != recBufWriteIndex;
}

inline
bool
OSE_Receiver::isFull () const {
  return ((recBufWriteIndex + 1) % recBufSize) == recBufWriteIndex;
}

inline
Uint32
OSE_Receiver::getReceiveData(NodeId * remoteNodeId,
			     Uint32 ** readPtr){
  NdbTransporterData *s = (NdbTransporterData *)receiveBuffer[recBufReadIndex];
  if(recBufReadIndex != recBufWriteIndex){
    * remoteNodeId = s->senderNodeId;
    * readPtr      = &s->data[0];
    return s->length;
  }
  return 0;
}

inline
void
OSE_Receiver::updateReceiveDataPtr(Uint32 bytesRead){
  if(bytesRead != 0){
    free_buf(&receiveBuffer[recBufReadIndex]);
    recBufReadIndex = (recBufReadIndex + 1) % recBufSize;
  }
}

inline 
void
OSE_Receiver::insertReceiveBuffer(union SIGNAL * _sig){
  receiveBuffer[recBufWriteIndex] = _sig;
  recBufWriteIndex = (recBufWriteIndex + 1) % recBufSize;
}


#endif
