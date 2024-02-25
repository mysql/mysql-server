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

#ifndef SD_EVENT_REPORT_H
#define SD_EVENT_REPORT_H

#include <ndb_logevent.h>
#include "SignalData.hpp"

#define JAM_FILE_ID 52


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

     2) remember to update # of events below. Just to keep count...
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


#undef JAM_FILE_ID

#endif
