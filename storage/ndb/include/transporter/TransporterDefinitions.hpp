/*
   Copyright (c) 2003, 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef TransporterDefinitions_H
#define TransporterDefinitions_H

#include <ndb_global.h> 
#include <kernel_types.h> 
#include <NdbOut.hpp>

/**
 * The sendbuffer limit after which the contents of the buffer is sent
 */
const int TCP_SEND_LIMIT = 64000;

enum SendStatus { 
  SEND_OK = 0, 
  SEND_BLOCKED = 1, 
  SEND_DISCONNECTED = 2, 
  SEND_BUFFER_FULL = 3,
  SEND_MESSAGE_TOO_BIG = 4,
  SEND_UNKNOWN_NODE = 5
};

enum TransporterType {
  tt_TCP_TRANSPORTER = 1,
  tt_SHM_TRANSPORTER = 3,
  tt_Multi_TRANSPORTER = 4
};

enum SB_LevelType
{
  SB_NO_RISK_LEVEL = 0,
  SB_LOW_LEVEL = 1,
  SB_MEDIUM_LEVEL = 2,
  SB_HIGH_LEVEL = 3,
  SB_RISK_LEVEL = 4,
  SB_CRITICAL_LEVEL = 5
};

/**
 * Maximum message sizes
 * ---------------------
 * Maximum byte sizes for sent and received messages.
 * The maximum send message size is temporarily smaller than 
 * the maximum receive message size to support online
 * upgrade
 * Maximum received size increased in :
 *   mysql-5.1-telco-6.3.18 from 16516 bytes to 32768
 * Maximum send size increased in :
 *   mysql-5.1-telco-6.4.0 from 16516 bytes to 32768
 *
 * Therefore mysql-5.1-telco-6.4.0 cannot safely communicate 
 * with nodes at versions lower than mysql-5.1-telco-6.3.18 
 * 
 */
constexpr Uint32 MAX_RECV_MESSAGE_BYTESIZE = 32768;
constexpr Uint32 MAX_SEND_MESSAGE_BYTESIZE = 32768;

/**
 * TransporterConfiguration
 *
 * used for setting up a transporter. the union member specific is for
 * information specific to a transporter type.
 */
struct TransporterConfiguration {
  Int32 s_port; // negative port number implies dynamic port
  const char *remoteHostName;
  const char *localHostName;
  TrpId transporterIndex;
  NodeId remoteNodeId;
  NodeId localNodeId;
  NodeId serverNodeId;
  bool checksum;
  bool signalId;
  bool isMgmConnection; // is a mgm connection, requires transforming
  TransporterType type;
  bool preSendChecksum;

  union { // Transporter specific configuration information

    struct {
      Uint32 sendBufferSize;     // Size of SendBuffer of priority B 
      Uint32 maxReceiveSize;     // Maximum no of bytes to receive
      Uint32 tcpSndBufSize;
      Uint32 tcpRcvBufSize;
      Uint32 tcpMaxsegSize;
      Uint32 tcpOverloadLimit;
      Uint32 tcpSpintime;
    } tcp;
    
    struct {
      Uint32 shmKey;
      Uint32 shmSize;
      Uint32 shmSpintime;
      Uint32 sendBufferSize;
    } shm;
  };
};

struct SignalHeader {	
  Uint32 theVerId_signalNumber;    // 4 bit ver id - 16 bit gsn
  Uint32 theReceiversBlockNumber;  // Only 16 bit blocknum  
  Uint32 theSendersBlockRef;
  Uint32 theLength;
  Uint32 theSendersSignalId;
  Uint32 theSignalId;
  Uint16 theTrace;
  Uint8  m_noOfSections;
  Uint8  m_fragmentInfo;
}; /** 7x4 = 28 Bytes */

class NdbOut & operator <<(class NdbOut & out, SignalHeader & sh);

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

  /* Used 0x16 - 0x22 */

  /**
   * TE_UNSUPPORTED_BYTE_ORDER
   *
   *   Error found in message (byte order)
   *
   * Recommended behavior: setPerformState(PerformDisonnect)
   */
  , TE_UNSUPPORTED_BYTE_ORDER = 0x23 | TE_DO_DISCONNECT

  /**
   * TE_COMPRESSED_UNSUPPORTED
   *
   *   Error found in message (compressed flag)
   *
   * Recommended behavior: setPerformState(PerformDisonnect)
   */
  , TE_COMPRESSED_UNSUPPORTED = 0x24 | TE_DO_DISCONNECT

  /**
   *
   * Error found in signal, not following NDB protocol
   * Recommended behavior: setPerformState(PerformDisonnect)
   */
  , TE_INVALID_SIGNAL = 0x25 | TE_DO_DISCONNECT
};

#endif // Define of TransporterDefinitions_H
