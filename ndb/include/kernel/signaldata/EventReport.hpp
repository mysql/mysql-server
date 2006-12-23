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

#ifndef SD_EVENT_REPORT_H
#define SD_EVENT_REPORT_H

#include <ndb_logevent.h>
#include "SignalData.hpp"

/**
 * Send by different block to report that a event has taken place
 *
 * SENDER:  *Block*
 * RECIVER: SimBlockCMCtrBlck
 */
class EventReport {
  friend class SimulatedBlock;
  friend class Cmvmi;
  friend class SimblockMissra;
  friend class Dbacc;
  friend class Dblqh;
  friend class Dbtup;
  friend class Dbtc;
  friend class Ndbcntr;
  friend class Qmgr;
  friend class Dbdih;
  friend class Dbdict;
  friend class MgmtSrvr;
  friend class Grep;
public:
  /* 
     EventType defines what event reports to send. 

     The ORDER is NOT important anymore. //ejonore 2003-07-24 15:03
     
     HOW TO ADD A NEW EVENT
     --------------------
     1) Add SentHeartbeat EventType in the category where it belongs.
        ...
        // INFO
        SentHeartbeat,
        InfoEvent
        ...

     2) remeber to update # of events below. Just to keep count...
        Number of event types = 53

     3) Add a new SentHeartBeat entry to EventLogger::matrix[]. 
       ...
       // INFO
       { EventReport::SentHeartbeat, LogLevel::llInfo, 11, INFO },
       { EventReport::InfoEvent,     LogLevel::llInfo,  2, INFO }      
       ...

     4) Add SentHeartbeat in EventLogger::getText()

   */
  void setNodeId(Uint32 nodeId);
  Uint32 getNodeId() const;
  void setEventType(Ndb_logevent_type type);
  Ndb_logevent_type getEventType() const;
  UintR eventType;    // DATA 0
};

inline
void
EventReport::setNodeId(Uint32 nodeId){
  eventType = (nodeId << 16) | (eventType & 0xFFFF);
}

inline
Uint32
EventReport::getNodeId() const {
  return eventType >> 16;
}

inline
void
EventReport::setEventType(Ndb_logevent_type type){
  eventType = (eventType & 0xFFFF0000) | (((UintR) type) & 0xFFFF);
}

inline
Ndb_logevent_type
EventReport::getEventType() const {
  return (Ndb_logevent_type)(eventType & 0xFFFF);
}

#endif
