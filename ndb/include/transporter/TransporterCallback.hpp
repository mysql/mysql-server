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

//**************************************************************************** 
// 
//  AUTHOR 
//      Åsa Fransson 
// 
//  NAME 
//      TransporterCallback 
// 
// 
//***************************************************************************/ 
#ifndef TRANSPORTER_CALLBACK_H 
#define TRANSPORTER_CALLBACK_H 
 
#include <kernel_types.h> 
#include "TransporterDefinitions.hpp" 
 
 
/** 
 * Call back functions 
 */ 
 
/** 
 * The execute function 
 */ 
void
execute(void * callbackObj, 
	SignalHeader * const header,  
	Uint8 prio,  
	Uint32 * const signalData,
	LinearSectionPtr ptr[3]);

/** 
 * A function to avoid job buffer overflow in NDB kernel, empty in API 
 * Non-zero return means we executed signals. This is necessary information 
 * to the transporter to ensure that it properly uses the transporter after 
 * coming back again. 
 */ 
int
checkJobBuffer(); 

/** 
 * Report send length 
 */ 
void 
reportSendLen(void * callbackObj,
	      NodeId nodeId, Uint32 count, Uint64 bytes); 
 
/** 
 * Report average receive length 
 */ 
void 
reportReceiveLen(void * callbackObj, 
		 NodeId nodeId, Uint32 count, Uint64 bytes); 
 
/** 
 * Report connection established 
 */ 
void 
reportConnect(void * callbackObj, NodeId nodeId); 
 
/** 
 * Report connection broken 
 */ 
 
void 
reportDisconnect(void * callbackObj,
		 NodeId nodeId, Uint32 errNo); 

#define TE_DO_DISCONNECT 0x8000

enum TransporterError { 
  TE_NO_ERROR = 0,
  /** 
   * TE_ERROR_CLOSING_SOCKET 
   * 
   *   Error found during closing of socket 
   * 
   * Recommended behavior: Ignore 
   */ 
  TE_ERROR_CLOSING_SOCKET = 0x1, 
 
  /** 
   * TE_ERROR_IN_SELECT_BEFORE_ACCEPT 
   * 
   *   Error found during accept (just before) 
   *     The transporter will retry. 
   * 
   * Recommended behavior: Ignore  
   *   (or possible do setPerformState(PerformDisconnect) 
   */ 
  TE_ERROR_IN_SELECT_BEFORE_ACCEPT = 0x2, 
 
  /** 
   * TE_INVALID_MESSAGE_LENGTH 
   * 
   *   Error found in message (message length) 
   * 
   * Recommended behavior: setPerformState(PerformDisconnect) 
   */ 
  TE_INVALID_MESSAGE_LENGTH = 0x3 | TE_DO_DISCONNECT, 
   
  /** 
   * TE_INVALID_CHECKSUM 
   * 
   *   Error found in message (checksum) 
   * 
   * Recommended behavior: setPerformState(PerformDisonnect) 
   */ 
  TE_INVALID_CHECKSUM = 0x4 | TE_DO_DISCONNECT, 
 
  /** 
   * TE_COULD_NOT_CREATE_SOCKET 
   * 
   *   Error found while creating socket 
   * 
   * Recommended behavior: setPerformState(PerformDisonnect) 
   */ 
  TE_COULD_NOT_CREATE_SOCKET = 0x5, 
 
  /** 
   * TE_COULD_NOT_BIND_SOCKET 
   * 
   *   Error found while binding server socket 
   * 
   * Recommended behavior: setPerformState(PerformDisonnect) 
   */ 
  TE_COULD_NOT_BIND_SOCKET = 0x6, 
 
  /** 
   * TE_LISTEN_FAILED 
   * 
   *   Error found while listening to server socket 
   * 
   * Recommended behavior: setPerformState(PerformDisonnect) 
   */ 
  TE_LISTEN_FAILED = 0x7, 
 
  /** 
   * TE_ACCEPT_RETURN_ERROR 
   * 
   *   Error found during accept 
   *     The transporter will retry. 
   * 
   * Recommended behavior: Ignore  
   *   (or possible do setPerformState(PerformDisconnect) 
   */ 
  TE_ACCEPT_RETURN_ERROR = 0x8 
 
  /** 
   * TE_SHM_DISCONNECT  
   * 
   *    The remote node has disconnected 
   * 
   * Recommended behavior: setPerformState(PerformDisonnect) 
   */ 
  ,TE_SHM_DISCONNECT = 0xb | TE_DO_DISCONNECT
 
  /** 
   * TE_SHM_IPC_STAT 
   * 
   *    Unable to check shm segment 
   *      probably because remote node 
   *      has disconnected and removed it 
   * 
   * Recommended behavior: setPerformState(PerformDisonnect) 
   */ 
  ,TE_SHM_IPC_STAT = 0xc | TE_DO_DISCONNECT

  /**
   * Permanent error
   */
  ,TE_SHM_IPC_PERMANENT = 0x21 
 
  /** 
   * TE_SHM_UNABLE_TO_CREATE_SEGMENT 
   * 
   *    Unable to create shm segment 
   *      probably os something error 
   * 
   * Recommended behavior: setPerformState(PerformDisonnect) 
   */ 
  ,TE_SHM_UNABLE_TO_CREATE_SEGMENT = 0xd 
 
  /** 
   * TE_SHM_UNABLE_TO_ATTACH_SEGMENT 
   * 
   *    Unable to attach shm segment 
   *      probably invalid group / user 
   * 
   * Recommended behavior: setPerformState(PerformDisonnect) 
   */ 
  ,TE_SHM_UNABLE_TO_ATTACH_SEGMENT = 0xe 
 
  /** 
   * TE_SHM_UNABLE_TO_REMOVE_SEGMENT 
   * 
   *    Unable to remove shm segment 
   * 
   * Recommended behavior: Ignore (not much to do) 
   *                       Print warning to logfile 
   */ 
  ,TE_SHM_UNABLE_TO_REMOVE_SEGMENT = 0xf 
 
  ,TE_TOO_SMALL_SIGID = 0x10 
  ,TE_TOO_LARGE_SIGID = 0x11 
  ,TE_WAIT_STACK_FULL = 0x12 | TE_DO_DISCONNECT
  ,TE_RECEIVE_BUFFER_FULL = 0x13 | TE_DO_DISCONNECT
 
  /** 
   * TE_SIGNAL_LOST_SEND_BUFFER_FULL 
   * 
   *   Send buffer is full, and trying to force send fails 
   *   a signal is dropped!! very bad very bad 
   * 
   */ 
  ,TE_SIGNAL_LOST_SEND_BUFFER_FULL = 0x14 | TE_DO_DISCONNECT
 
  /** 
   * TE_SIGNAL_LOST 
   * 
   *   Send failed for unknown reason 
   *   a signal is dropped!! very bad very bad 
   * 
   */ 
  ,TE_SIGNAL_LOST = 0x15 
 
  /** 
   * TE_SEND_BUFFER_FULL 
   *  
   *   The send buffer was full, but sleeping for a while solved it 
   */ 
  ,TE_SEND_BUFFER_FULL = 0x16 
 
  /** 
   * TE_SCI_UNABLE_TO_CLOSE_CHANNEL 
   *  
   *  Unable to close the sci channel and the resources allocated by  
   *  the sisci api. 
   */ 
  ,TE_SCI_UNABLE_TO_CLOSE_CHANNEL = 0x22 
 
  /** 
   * TE_SCI_LINK_ERROR 
   *  
   *  There is no link from this node to the switch.  
   *  No point in continuing. Must check the connections. 
   * Recommended behavior: setPerformState(PerformDisonnect) 
   */ 
  ,TE_SCI_LINK_ERROR = 0x0017 
 
  /** 
   * TE_SCI_UNABLE_TO_START_SEQUENCE 
   *  
   *  Could not start a sequence, because system resources  
   *  are exumed or no sequence has been created. 
   *  Recommended behavior: setPerformState(PerformDisonnect) 
   */ 
  ,TE_SCI_UNABLE_TO_START_SEQUENCE = 0x18 | TE_DO_DISCONNECT
   
  /** 
   * TE_SCI_UNABLE_TO_REMOVE_SEQUENCE 
   *  
   *  Could not remove a sequence 
   */ 
  ,TE_SCI_UNABLE_TO_REMOVE_SEQUENCE = 0x19 | TE_DO_DISCONNECT
 
  /** 
   * TE_SCI_UNABLE_TO_CREATE_SEQUENCE 
   *  
   *  Could not create a sequence, because system resources are 
   *  exempted. Must reboot. 
   *  Recommended behavior: setPerformState(PerformDisonnect) 
   */ 
  ,TE_SCI_UNABLE_TO_CREATE_SEQUENCE = 0x1a | TE_DO_DISCONNECT
 
  /** 
   * TE_SCI_UNRECOVERABLE_DATA_TFX_ERROR 
   *  
   *  Tried to send data on redundant link but failed. 
   *  Recommended behavior: setPerformState(PerformDisonnect) 
   */ 
  ,TE_SCI_UNRECOVERABLE_DATA_TFX_ERROR = 0x1b | TE_DO_DISCONNECT
 
  /** 
   * TE_SCI_CANNOT_INIT_LOCALSEGMENT 
   *  
   *  Cannot initialize local segment. A whole lot of things has 
   *  gone wrong (no system resources). Must reboot. 
   *  Recommended behavior: setPerformState(PerformDisonnect) 
   */ 
  ,TE_SCI_CANNOT_INIT_LOCALSEGMENT = 0x1c | TE_DO_DISCONNECT
 
  /** 
   * TE_SCI_CANNOT_MAP_REMOTESEGMENT 
   *  
   *  Cannot map remote segment. No system resources are left.  
   *  Must reboot system. 
   *  Recommended behavior: setPerformState(PerformDisonnect) 
   */ 
  ,TE_SCI_CANNOT_MAP_REMOTESEGMENT = 0x1d | TE_DO_DISCONNECT
 
   /** 
   * TE_SCI_UNABLE_TO_UNMAP_SEGMENT 
   *  
   *  Cannot free the resources used by this segment (step 1). 
   *  Recommended behavior: setPerformState(PerformDisonnect) 
   */ 
  ,TE_SCI_UNABLE_TO_UNMAP_SEGMENT = 0x1e | TE_DO_DISCONNECT
 
   /** 
   * TE_SCI_UNABLE_TO_REMOVE_SEGMENT 
   *  
   *  Cannot free the resources used by this segment (step 2). 
   *  Cannot guarantee that enough resources exist for NDB 
   *  to map more segment 
   *  Recommended behavior: setPerformState(PerformDisonnect) 
   */ 
  ,TE_SCI_UNABLE_TO_REMOVE_SEGMENT = 0x1f  | TE_DO_DISCONNECT
 
   /** 
   * TE_SCI_UNABLE_TO_DISCONNECT_SEGMENT 
   *  
   *  Cannot disconnect from a remote segment. 
   *  Recommended behavior: setPerformState(PerformDisonnect) 
   */ 
  ,TE_SCI_UNABLE_TO_DISCONNECT_SEGMENT = 0x20 | TE_DO_DISCONNECT
 
  /* Used 0x21 */
  /* Used 0x22 */
}; 
 
/** 
 * Report error 
 */ 
void 
reportError(void * callbackObj, NodeId nodeId, TransporterError errorCode,
	    const char *info = 0);

void
transporter_recv_from(void* callbackObj, NodeId node);
 
#endif   
