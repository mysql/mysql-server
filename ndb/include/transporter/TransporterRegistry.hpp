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
//  NAME
//      TransporterRegistry
//
//  DESCRIPTION
//      TransporterRegistry (singelton) is the interface to the 
//      transporter layer. It handles transporter states and 
//      holds the transporter arrays.
//
//***************************************************************************/
#ifndef TransporterRegistry_H
#define TransporterRegistry_H

#include "TransporterDefinitions.hpp"

#include <NdbTCP.h>

// A transporter is always in a PerformState.
// PerformIO is used initially and as long as any of the events 
// PerformConnect, ... 
enum PerformState {
  PerformNothing    = 4, // Does nothing
  PerformIO         = 0, // Is connected
  PerformConnect    = 1, // Is trying to connect
  PerformDisconnect = 2, // Trying to disconnect
  RemoveTransporter = 3  // Will be removed
};

// A transporter is always in an IOState.
// NoHalt is used initially and as long as it is no restrictions on
// sending or receiving.
enum IOState {
  NoHalt     = 0,
  HaltInput  = 1,
  HaltOutput = 2,
  HaltIO     = 3
};

enum TransporterType {
  tt_TCP_TRANSPORTER = 1,
  tt_SCI_TRANSPORTER = 2,
  tt_SHM_TRANSPORTER = 3,
  tt_OSE_TRANSPORTER = 4
};

class Transporter;
class TCP_Transporter;
class SCI_Transporter;
class SHM_Transporter;
class OSE_Transporter;

/**
 * @class TransporterRegistry
 * @brief ...
 */
class TransporterRegistry {
  friend class OSE_Receiver;
public:
 /**
  * Constructor
  */
  TransporterRegistry(void * callback = 0 , 
		      unsigned maxTransporters = MAX_NTRANSPORTERS, 
		      unsigned sizeOfLongSignalMemory = 100);
  
  bool init(NodeId localNodeId);
  
  /**
   * Remove all transporters
   */
  void removeAll();
  
  /**
   * Disconnect all transporters
   */
  void disconnectAll();

  /**
   * Stops the server, disconnects all the transporter 
   * and deletes them and remove it from the transporter arrays
   */
  ~TransporterRegistry();

  /**
   * Start/Stop receiving
   */
  void startReceiving();
  void stopReceiving();
  
  /**
   * Start/Stop sending
   */
  void startSending();
  void stopSending();

  /**
   * Get and set methods for PerformState
   */
  PerformState performState(NodeId nodeId);
  void setPerformState(NodeId nodeId, PerformState state);
  
  /**
   * Set perform state for all transporters
   */
  void setPerformState(PerformState state);
  
  /**
   * Get and set methods for IOState
   */
  IOState ioState(NodeId nodeId);
  void setIOState(NodeId nodeId, IOState state);

  /** 
   * createTransporter
   *
   * If the config object indicates that the transporter
   * to be created will act as a server and no server is
   * started, startServer is called. A transporter of the selected kind
   * is created and it is put in the transporter arrays.
   */
  bool createTransporter(struct TCP_TransporterConfiguration * config);
  bool createTransporter(struct SCI_TransporterConfiguration * config);
  bool createTransporter(struct SHM_TransporterConfiguration * config);
  bool createTransporter(struct OSE_TransporterConfiguration * config);
  
  /**
   * prepareSend
   *
   * When IOState is HaltOutput or HaltIO do not send or insert any 
   * signals in the SendBuffer, unless it is intended for the remote 
   * CMVMI block (blockno 252)
   * Perform prepareSend on the transporter. 
   *
   * NOTE signalHeader->xxxBlockRef should contain block numbers and 
   *                                not references
   */
  SendStatus prepareSend(const SignalHeader * const signalHeader, Uint8 prio,
			 const Uint32 * const signalData,
			 NodeId nodeId, 
			 const LinearSectionPtr ptr[3]);

  SendStatus prepareSend(const SignalHeader * const signalHeader, Uint8 prio,
			 const Uint32 * const signalData,
			 NodeId nodeId, 
			 class SectionSegmentPool & pool,
			 const SegmentedSectionPtr ptr[3]);
  
  /**
   * external_IO
   *
   * Equal to: poll(...); perform_IO()
   *
   */
  void external_IO(Uint32 timeOutMillis);
  
  Uint32 pollReceive(Uint32 timeOutMillis);
  void performReceive();
  void performSend();
  
  void checkConnections();
  
  /**
   * Force sending if more than or equal to sendLimit
   * number have asked for send. Returns 0 if not sending
   * and 1 if sending.
   */
  int forceSendCheck(int sendLimit);
  
#ifdef DEBUG_TRANSPORTER
  void printState();
#endif
  
protected:
  
private:
  void * callbackObj;

  int sendCounter;
  NodeId localNodeId;
  bool nodeIdSpecified;
  unsigned maxTransporters;
  int nTransporters;
  int nTCPTransporters;
  int nSCITransporters;
  int nSHMTransporters;
  int nOSETransporters;

  int m_ccCount;
  int m_ccIndex;
  int m_ccStep;
  int m_nTransportersPerformConnect;
  bool m_ccReady;
  /**
   * Arrays holding all transporters in the order they are created
   */
  TCP_Transporter** theTCPTransporters;
  SCI_Transporter** theSCITransporters;
  SHM_Transporter** theSHMTransporters;
  OSE_Transporter** theOSETransporters;
  
  /**
   * Array, indexed by nodeId, holding all transporters
   */
  TransporterType* theTransporterTypes;
  Transporter**    theTransporters;

  /**
   * OSE Receiver
   */
  class OSE_Receiver * theOSEReceiver;

  /**
   * In OSE you for some bizar reason needs to create a socket
   *  the first thing you do when using inet functions.
   *
   * Furthermore a process doing select has to "own" a socket
   * 
   */  
  int theOSEJunkSocketSend;
  int theOSEJunkSocketRecv;
#if defined NDB_OSE || defined NDB_SOFTOSE
  PROCESS theReceiverPid;
#endif
  
  /** 
   * State arrays, index by host id
   */
  PerformState* performStates;
  IOState*      ioStates;
 
  /**
   * Unpack signal data
   */
  Uint32 unpack(Uint32 * readPtr,
		Uint32 bufferSize,
		NodeId remoteNodeId, 
		IOState state);
  
  Uint32 * unpack(Uint32 * readPtr,
		  Uint32 * eodPtr,
		  NodeId remoteNodeId,
		  IOState state);

  /** 
   * Disconnect the transporter and remove it from 
   * theTransporters array. Do not allow any holes 
   * in theTransporters. Delete the transporter 
   * and remove it from theIndexedTransporters array
   */
  void removeTransporter(NodeId nodeId);
  
  /**
   * Used in polling if exists TCP_Transporter
   */
  int tcpReadSelectReply;
  fd_set tcpReadset;
  
  Uint32 poll_OSE(Uint32 timeOutMillis);
  Uint32 poll_TCP(Uint32 timeOutMillis);
  Uint32 poll_SCI(Uint32 timeOutMillis);
  Uint32 poll_SHM(Uint32 timeOutMillis);
};

#endif // Define of TransporterRegistry_H
