/*
   Copyright (C) 2005, 2006, 2008 MySQL AB, 2008, 2009 Sun Microsystems, Inc.
    All rights reserved. Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef TESTDATA_H
#define TESTDATA_H

/***************************************************************
* I N C L U D E D   F I L E S                                  *
***************************************************************/
#include <NdbTick.h>
#include <NdbThread.h>
#include <NDBT_Stats.hpp>
#include <random.h>
#include "testDefinitions.h"

/***************************************************************
* M A C R O S                                                  *
***************************************************************/

/***************************************************************/
/* C O N S T A N T S                                           */
/***************************************************************/

#define NUM_TRANSACTION_TYPES    5
#define SESSION_LIST_LENGTH   1000

/***************************************************************
* D A T A   S T R U C T U R E S                                *
***************************************************************/

typedef struct {
  SubscriberNumber subscriberNumber;
  ServerId         serverId;
} SessionElement;

typedef struct {
  SessionElement list[SESSION_LIST_LENGTH];
  unsigned int readIndex;
  unsigned int writeIndex;
  unsigned int numberInList;
} SessionList;  

typedef struct {
  unsigned int  count;
  unsigned int  branchExecuted;
  unsigned int  rollbackExecuted;

  /**
   * Latency measures
   */
  NDB_TICKS     startTime;
  NDBT_Stats    latency;
  unsigned int  latencyCounter;

  inline void startLatency(){
    if((latencyCounter & 127) == 127)
      startTime = NdbTick_CurrentMillisecond();
  }

  inline void stopLatency(){
    if((latencyCounter & 127) == 127){
      const NDB_TICKS tmp = NdbTick_CurrentMillisecond() - startTime;
      latency.addObservation((double)tmp);
    }
    latencyCounter++;
  }
} TransactionDefinition;

typedef struct {
  RandomSequence transactionSequence;
  RandomSequence rollbackSequenceT4;
  RandomSequence rollbackSequenceT5;
  
  TransactionDefinition transactions[NUM_TRANSACTION_TYPES];

  unsigned int totalTransactions;
    
  double       outerLoopTime;
  double       outerTps;
  
  SessionList  activeSessions;
  
} GeneratorStatistics;

typedef enum{
  Runnable,
  Running
} RunState ;

typedef struct {
  SubscriberNumber    number;	
  SubscriberSuffix    suffix;
  SubscriberName      name;
  Location            location;
  ChangedBy           changed_by;
  ChangedTime         changed_time;
  ServerId            server_id;
  ServerBit           server_bit;
  SessionDetails      session_details;

  GroupId             group_id;
  ActiveSessions      sessions;
  Permission          permission;

  unsigned int        do_rollback;

  unsigned int        branchExecuted;
  unsigned int        sessionElement;
} TransactionData ;

typedef struct {
  const class NdbRecord* subscriberTableNdbRecord;
  const class NdbRecord* groupTableAllowReadNdbRecord;
  const class NdbRecord* groupTableAllowInsertNdbRecord;
  const class NdbRecord* groupTableAllowDeleteNdbRecord;
  const class NdbRecord* sessionTableNdbRecord;
  const class NdbInterpretedCode* incrServerReadsProg;
  const class NdbInterpretedCode* incrServerInsertsProg;
  const class NdbInterpretedCode* incrServerDeletesProg;
  const class NdbRecord* serverTableNdbRecord;
} NdbRecordSharedData ;

typedef struct {
  struct NdbThread* pThread;

  unsigned long randomSeed;
  unsigned long changedTime;

  unsigned int warmUpSeconds;
  unsigned int testSeconds;
  unsigned int coolDownSeconds;

  GeneratorStatistics generator;
  
  /**
   * For async execution
   */
  RunState              runState;
  double                startTime;
  TransactionData       transactionData;
  class Ndb           * pNDB;
  NdbRecordSharedData*  ndbRecordSharedData;
  bool                  useCombinedUpdate;
} ThreadData;

/***************************************************************
 * P U B L I C    F U N C T I O N S                             *
 ***************************************************************/

/***************************************************************
 * E X T E R N A L   D A T A                                    *
 ***************************************************************/



#endif /* TESTDATA_H */

