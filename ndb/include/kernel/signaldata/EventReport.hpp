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

#ifndef SD_EVENT_REPORT_H
#define SD_EVENT_REPORT_H

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
  enum EventType {
    // CONNECTION
    Connected = 0,
    Disconnected = 1,
    CommunicationClosed = 2,
    CommunicationOpened = 3,
    ConnectedApiVersion = 51,
    // CHECKPOINT
    GlobalCheckpointStarted = 4,
    GlobalCheckpointCompleted = 5,
    LocalCheckpointStarted = 6,
    LocalCheckpointCompleted = 7,
    LCPStoppedInCalcKeepGci = 8,
    LCPFragmentCompleted = 9,
    // STARTUP
    NDBStartStarted = 10,
    NDBStartCompleted = 11,
    STTORRYRecieved = 12,
    StartPhaseCompleted = 13,
    CM_REGCONF = 14,
    CM_REGREF = 15,
    FIND_NEIGHBOURS = 16,
    NDBStopStarted = 17,
    NDBStopAborted = 18,
    StartREDOLog = 19,
    StartLog = 20,
    UNDORecordsExecuted = 21,

    // NODERESTART
    NR_CopyDict = 22,
    NR_CopyDistr = 23,
    NR_CopyFragsStarted = 24,
    NR_CopyFragDone = 25,
    NR_CopyFragsCompleted = 26,
    
    // NODEFAIL
    NodeFailCompleted = 27,
    NODE_FAILREP = 28,
    ArbitState = 29,
    ArbitResult = 30,
    GCP_TakeoverStarted = 31,
    GCP_TakeoverCompleted = 32,
    LCP_TakeoverStarted = 33,
    LCP_TakeoverCompleted = 34,
    
    // STATISTIC
    TransReportCounters = 35,
    OperationReportCounters = 36,
    TableCreated = 37,
    UndoLogBlocked = 38,
    JobStatistic = 39,
    SendBytesStatistic = 40,
    ReceiveBytesStatistic = 41,
    MemoryUsage = 50,

    // ERROR
    TransporterError = 42,
    TransporterWarning = 43,
    MissedHeartbeat = 44,
    DeadDueToHeartbeat = 45,
    WarningEvent = 46,
    // INFO
    SentHeartbeat = 47,
    CreateLogBytes = 48,
    InfoEvent = 49,

    // SINGLE USER
    SingleUser = 52,
    /* unused 53 */

    //BACKUP
    BackupStarted = 54,
    BackupFailedToStart = 55,
    BackupCompleted = 56,
    BackupAborted = 57
  };
  
  void setEventType(EventType type);
  EventType getEventType() const;
  UintR eventType;    // DATA 0
};

inline
void
EventReport::setEventType(EventType type){
  eventType = (UintR) type;
}

inline
EventReport::EventType
EventReport::getEventType() const {
  return (EventType)eventType;
}

#endif
