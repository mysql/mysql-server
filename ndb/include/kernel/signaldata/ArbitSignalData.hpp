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

#ifndef ARBIT_SIGNAL_DATA_H
#define ARBIT_SIGNAL_DATA_H

#include <string.h>
#include <NodeBitmask.hpp>
#include <NdbTick.h>
#include <NdbHost.h>
#include "SignalData.hpp"
#include "SignalDataPrint.hpp"

/**
 * The ticket.
 */
class ArbitTicket {
private:
  Uint32 data[2];

public:
  STATIC_CONST( DataLength = 2 );
  STATIC_CONST( TextLength = DataLength * 8 );  // hex digits

  inline void clear() {
    data[0] = 0;
    data[1] = 0;
  }

  inline void update() {
    Uint16 cnt = data[0] & 0xFFFF;              // previous count
    Uint16 pid = NdbHost_GetProcessId();
    data[0] = (pid << 16) | (cnt + 1);
    data[1] = NdbTick_CurrentMillisecond();
  }

  inline bool match(ArbitTicket& aTicket) const {
    return
      data[0] == aTicket.data[0] &&
      data[1] == aTicket.data[1];
  }

  inline void getText(char *buf, size_t buf_len) const {
    BaseString::snprintf(buf, buf_len, "%08x%08x", data[0], data[1]);
  }

/*  inline char* getText() const {
    static char buf[TextLength + 1];
    getText(buf, sizeof(buf));
    return buf;
  } */
};

/**
 * Result codes.  Part of signal data.  Each signal uses only
 * a subset but a common namespace is convenient.
 */
class ArbitCode {
public:
  STATIC_CONST( ErrTextLength = 80 );

  enum {
    NoInfo = 0,

    // CFG signals
    CfgRank1 = 1,               // these have to be 1 and 2
    CfgRank2 = 2,

    // QMGR continueB thread state
    ThreadStart = 11,           // continueB thread started

    // PREP signals
    PrepPart1 = 21,             // zero old ticket
    PrepPart2 = 22,             // get new ticket
    PrepAtrun = 23,             // late joiner gets ticket at RUN time

    // arbitrator state
    ApiStart = 31,              // arbitrator thread started
    ApiFail = 32,               // arbitrator died
    ApiExit = 33,               // arbitrator reported it will exit

    // arbitration result
    LoseNodes = 41,             // lose on ndb node count
    WinNodes = 42,              // win on ndb node count
    WinGroups = 43,             // we win, no need for arbitration
    LoseGroups = 44,            // we lose, missing node group
    Partitioning = 45,          // possible network partitioning
    WinChoose = 46,             // positive reply
    LoseChoose = 47,            // negative reply
    LoseNorun = 48,             // arbitrator required but not running
    LoseNocfg = 49,             // arbitrator required but none configured

    // general error codes
    ErrTicket = 91,             // invalid arbitrator-ticket
    ErrToomany = 92,            // too many requests
    ErrState = 93,              // invalid state
    ErrTimeout = 94,            // timeout waiting for signals
    ErrUnknown = 95             // unknown error
  };

  static inline void getErrText(Uint32 code, char* buf, size_t buf_len) {
    switch (code) {
    case ErrTicket:
      BaseString::snprintf(buf, buf_len, "invalid arbitrator-ticket");
      break;
    case ErrToomany:
      BaseString::snprintf(buf, buf_len, "too many requests");
      break;
    case ErrState:
      BaseString::snprintf(buf, buf_len, "invalid state");
      break;
    case ErrTimeout:
      BaseString::snprintf(buf, buf_len, "timeout");
      break;
    default:
      BaseString::snprintf(buf, buf_len, "unknown error [code=%u]", code);
      break;
    }
  }
};

/**
 * Common class for arbitration signal data.
 */
class ArbitSignalData {
public:
  Uint32 sender;                // sender's node id (must be word 0)
  Uint32 code;                  // result code or other info
  Uint32 node;                  // arbitrator node id
  ArbitTicket ticket;           // ticket
  NodeBitmask mask;             // set of nodes

  STATIC_CONST( SignalLength = 3 + ArbitTicket::DataLength + NodeBitmask::Size );

  inline bool match(ArbitSignalData& aData) const {
    return
      node == aData.node &&
      ticket.match(aData.ticket);
  }
};

#endif
