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

//****************************************************************************
//
//  AUTHOR
//      Magnus Svensson
//
//  NAME
//      OSE_Transporter
//
//  DESCRIPTION
//      A OSE_Transporter instance is created when OSE-signal communication 
//      shall be used (user specified). It handles connect, disconnect, 
//      send and receive.
//
//
//
//***************************************************************************/
#ifndef OSE_Transporter_H
#define OSE_Transporter_H

#include "Transporter.hpp"

#include "ose.h"

class OSE_Transporter : public Transporter {
  friend class OSE_Receiver;
  friend class TransporterRegistry;
public:
  
  // Initialize member variables
  OSE_Transporter(int prioASignalSize,
		  int prioBSignalSize,
		  NodeId localNodeId,
		  const char * lHostName,
		  NodeId remoteNodeId,
		  NodeId serverNodeId,
		  const char * rHostName,
		  int byteorder,
		  bool compression, 
		  bool checksum, 
		  bool signalId,
		  Uint32 reportFreq = 4096);
  
  // Disconnect, delete send buffers and receive buffer
  ~OSE_Transporter();
  
  /**
   * Allocate buffers for sending and receiving
   */
  bool initTransporter();

  /**
   * Connect
   */
  virtual void doConnect();

  /**
   * Disconnect
   */
  virtual void doDisconnect();
  
  Uint32 * getWritePtr(Uint32 lenBytes, Uint32 prio);
  void updateWritePtr(Uint32 lenBytes, Uint32 prio);
  
  /**
   * Retrieves the contents of the send buffers, copies it into 
   * an OSE signal and sends it. Until the send buffers are empty
   */
  void doSend();

  bool hasDataToSend() const {
    return prioBSignal->dataSignal.length > 0;
  }
  
protected:
  /**
   * Not implemented
   *   OSE uses async connect/disconnect
   */
  virtual bool connectImpl(Uint32 timeOut){
    return false;
  }
  
  /**
   * Not implemented
   *   OSE uses async connect/disconnect
   */
  virtual void disconnectImpl(){
  }

private:
  const bool isServer;
  
  int maxPrioBDataSize;

  /**
   * Remote node name
   * On same machine: ndb_node1
   * On remote machine: rhost/ndb_node1
   **/
  PROCESS  remoteNodePid;
  OSATTREF remoteNodeRef;
  char remoteNodeName[256];

  Uint32 signalIdCounter;

  int prioBSignalSize;

  Uint32 * prioBInsertPtr;
  union SIGNAL * prioBSignal;

  struct NdbTransporterData * allocPrioASignal(Uint32 lenBytes) const;
  
  /**
   * Statistics
   */
  Uint32 reportFreq;
  Uint32 receiveCount;
  Uint64 receiveSize;
  Uint32 sendCount;
  Uint64 sendSize;

  void initSignals();

  /**
   * OSE Receiver callbacks
   */
  void huntReceived(struct NdbTransporterHunt * sig);
  bool connectReq(struct NdbTransporterConnectReq * sig);
  bool connectRef(struct NdbTransporterConnectRef * sig);
  bool connectConf(struct NdbTransporterConnectConf * sig);
  bool disconnectOrd(struct NdbTransporterDisconnectOrd * sig);

  enum OSETransporterState {
    DISCONNECTED              = 0,
    WAITING_FOR_HUNT          = 1,
    WAITING_FOR_CONNECT_REQ   = 2,
    WAITING_FOR_CONNECT_CONF  = 3,
    CONNECTED                 = 4
  } state;
};

// Define of OSE_Transporter_H
#endif
