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

#ifndef OSE_SIGNALS_HPP
#define OSE_SIGNALS_HPP

#include <ose.h>
#include <kernel_types.h>

#define NDB_TRANSPORTER_SIGBASE  3000
   
#define NDB_TRANSPORTER_DATA           (NDB_TRANSPORTER_SIGBASE + 1)  /* !-SIGNO(struct NdbTransporterData)-! */  
#define NDB_TRANSPORTER_HUNT           (NDB_TRANSPORTER_SIGBASE + 2)  /* !-SIGNO(struct NdbTransporterHunt)-! */  
#define NDB_TRANSPORTER_CONNECT_REQ    (NDB_TRANSPORTER_SIGBASE + 3)  /* !-SIGNO(struct NdbTransporterConnectReq)-! */  
#define NDB_TRANSPORTER_CONNECT_REF    (NDB_TRANSPORTER_SIGBASE + 4)  /* !-SIGNO(struct NdbTransporterConnectRef)-! */  
#define NDB_TRANSPORTER_CONNECT_CONF   (NDB_TRANSPORTER_SIGBASE + 5)  /* !-SIGNO(struct NdbTransporterConnectConf)-! */  
#define NDB_TRANSPORTER_DISCONNECT_ORD (NDB_TRANSPORTER_SIGBASE + 6)  /* !-SIGNO(struct NdbTransporterDisconnectOrd)-! */  
#define NDB_TRANSPORTER_PRIO_A         (NDB_TRANSPORTER_SIGBASE + 7)

inline
const char *
sigNo2String(SIGSELECT sigNo){
  switch(sigNo){
  case NDB_TRANSPORTER_PRIO_A:
    return "PRIO_A_DATA";
    break;
  case NDB_TRANSPORTER_DATA:
    return "PRIO_B_DATA";
    break;
  case NDB_TRANSPORTER_HUNT:
    return "HUNT";
    break;
  case NDB_TRANSPORTER_CONNECT_REQ:
    return "CONNECT_REQ";
    break;
  case NDB_TRANSPORTER_CONNECT_REF:
    return "CONNECT_REF";
    break;
  case NDB_TRANSPORTER_CONNECT_CONF:
    return "CONNECT_CONF";
    break;
  case NDB_TRANSPORTER_DISCONNECT_ORD:
    return "DISCONNECT_ORD";
    break;
  }
  return "UNKNOWN";
}

struct NdbTransporterData
{
  SIGSELECT sigNo;
  Uint32 sigId; // Sequence number for this signal
  Uint32 senderNodeId;
  Uint32 length;
  Uint32 data[1];
};

struct NdbTransporterData_PrioA
{
  SIGSELECT sigNo;
  Uint32 sigId; // Sequence number for this signal
  Uint32 senderNodeId;
  Uint32 length;
  Uint32 data[1];
};

struct NdbTransporterHunt
{
  SIGSELECT sigNo;
  NodeId remoteNodeId;
};


struct NdbTransporterConnectReq
{
  SIGSELECT sigNo;
  NodeId remoteNodeId;
  NodeId senderNodeId;
};


struct NdbTransporterConnectConf
{
  SIGSELECT sigNo;
  NodeId remoteNodeId;
  NodeId senderNodeId;
};

struct NdbTransporterConnectRef
{
  SIGSELECT sigNo;
  NodeId remoteNodeId;
  NodeId senderNodeId;
  Uint32 reason;

  /**
   * Node is not accepting connections
   */
  static const Uint32 INVALID_STATE = 1;
};

struct NdbTransporterDisconnectOrd
{
  SIGSELECT sigNo;
  NodeId senderNodeId;
  Uint32 reason;

  /**
   * Process died
   */
  static const Uint32 PROCESS_DIED = 1;
  
  /**
   * Ndb disconnected
   */
  static const Uint32 NDB_DISCONNECT = 2;
};

union SIGNAL 
{
  SIGSELECT sigNo;
  struct NdbTransporterData          dataSignal;
  struct NdbTransporterData          prioAData;
  struct NdbTransporterHunt          ndbHunt;
  struct NdbTransporterConnectReq    ndbConnectReq;
  struct NdbTransporterConnectRef    ndbConnectRef;
  struct NdbTransporterConnectConf   ndbConnectConf;
  struct NdbTransporterDisconnectOrd ndbDisconnect;
};

#endif
