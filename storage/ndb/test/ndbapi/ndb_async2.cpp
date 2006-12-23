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

//#define DEBUG_ON

#include <string.h>
#include "userInterface.h"

#include "macros.h"
#include "ndb_schema.hpp"
#include "ndb_error.hpp"
#include <NdbSleep.h>

#include <NdbApi.hpp>

void T1_Callback(int result, NdbConnection * pCon, void * threadData);
void T2_Callback(int result, NdbConnection * pCon, void * threadData);
void T3_Callback_1(int result, NdbConnection * pCon, void * threadData);
void T3_Callback_2(int result, NdbConnection * pCon, void * threadData);
void T3_Callback_3(int result, NdbConnection * pCon, void * threadData);
void T4_Callback_1(int result, NdbConnection * pCon, void * threadData);
void T4_Callback_2(int result, NdbConnection * pCon, void * threadData);
void T4_Callback_3(int result, NdbConnection * pCon, void * threadData);
void T5_Callback_1(int result, NdbConnection * pCon, void * threadData);
void T5_Callback_2(int result, NdbConnection * pCon, void * threadData);
void T5_Callback_3(int result, NdbConnection * pCon, void * threadData);

static int stat_async = 0;

/**
 * Transaction 1 - T1 
 *
 * Update location and changed by/time on a subscriber
 *
 * Input: 
 *   SubscriberNumber,
 *   Location,
 *   ChangedBy,
 *   ChangedTime
 *
 * Output:
 */

#define SFX_START (SUBSCRIBER_NUMBER_LENGTH - SUBSCRIBER_NUMBER_SUFFIX_LENGTH)

inline
NdbConnection *
startTransaction(Ndb * pNDB, ThreadData * td){
  return pNDB->startTransactionDGroup (0, 
				       &td->transactionData.number[SFX_START],
				       1);
}

void
start_T1(Ndb * pNDB, ThreadData * td, int async){

  DEBUG2("T1(%.*s): - Starting", SUBSCRIBER_NUMBER_LENGTH, 
	 td->transactionData.number); 

  NdbConnection * pCON = 0;
  while((pCON = startTransaction(pNDB, td)) == 0){
    CHECK_ALLOWED_ERROR("T1: startTransaction", td, pNDB->getNdbError());
    NdbSleep_MilliSleep(10);
  }

  NdbOperation *MyOp = pCON->getNdbOperation(SUBSCRIBER_TABLE);
  if (MyOp != NULL) {  
    MyOp->updateTuple();  
    MyOp->equal(IND_SUBSCRIBER_NUMBER, 
		td->transactionData.number);
    MyOp->setValue(IND_SUBSCRIBER_LOCATION, 
		   (char *)&td->transactionData.location);
    MyOp->setValue(IND_SUBSCRIBER_CHANGED_BY, 
		   td->transactionData.changed_by);
    MyOp->setValue(IND_SUBSCRIBER_CHANGED_TIME, 
		   td->transactionData.changed_time);
    if (async == 1) {
      pCON->executeAsynchPrepare( Commit , T1_Callback, td);
    } else {
      int result = pCON->execute(Commit);
      T1_Callback(result, pCON, (void*)td);
      return;
    }//if
  } else {
    CHECK_NULL(MyOp, "T1: getNdbOperation", td, pCON->getNdbError());
  }//if
}

void
T1_Callback(int result, NdbConnection * pCON, void * threadData) {
  ThreadData * td = (ThreadData *)threadData;
  
  DEBUG2("T1(%.*s): - Completing", SUBSCRIBER_NUMBER_LENGTH, 
	 td->transactionData.number); 

  if (result == -1) {
    CHECK_ALLOWED_ERROR("T1: Commit", td, pCON->getNdbError());
    td->pNDB->closeTransaction(pCON);
    start_T1(td->pNDB, td, stat_async);
    return;
  }//if
  td->pNDB->closeTransaction(pCON);
  complete_T1(td);
}

/**
 * Transaction 2 - T2
 *
 * Read from Subscriber:
 *
 * Input: 
 *   SubscriberNumber
 *
 * Output:
 *   Location
 *   Changed by
 *   Changed Timestamp
 *   Name
 */
void
start_T2(Ndb * pNDB, ThreadData * td, int async){

  DEBUG3("T2(%.*s, %d): - Starting", SUBSCRIBER_NUMBER_LENGTH, 
	 td->transactionData.number, 
	 td->transactionData.location);
  
  NdbConnection * pCON = 0;
  
  while((pCON = startTransaction(pNDB, td)) == 0){
    CHECK_ALLOWED_ERROR("T2-1: startTransaction", td, pNDB->getNdbError());
    NdbSleep_MilliSleep(10);
  }

  NdbOperation *MyOp= pCON->getNdbOperation(SUBSCRIBER_TABLE);
  CHECK_NULL(MyOp, "T2: getNdbOperation", td,
	     pCON->getNdbError());
  
  MyOp->readTuple();
  MyOp->equal(IND_SUBSCRIBER_NUMBER,
	      td->transactionData.number);
  MyOp->getValue(IND_SUBSCRIBER_LOCATION, 
		 (char *)&td->transactionData.location);
  MyOp->getValue(IND_SUBSCRIBER_CHANGED_BY, 
		 td->transactionData.changed_by);
  MyOp->getValue(IND_SUBSCRIBER_CHANGED_TIME, 
		 td->transactionData.changed_time);
  MyOp->getValue(IND_SUBSCRIBER_NAME, 
		 td->transactionData.name);
  if (async == 1) {
    pCON->executeAsynchPrepare( Commit , T2_Callback, td);
  } else {
    int result = pCON->execute(Commit);
    T2_Callback(result, pCON, (void*)td);
    return;
  }//if
}

void
T2_Callback(int result, NdbConnection * pCON, void * threadData){
  ThreadData * td = (ThreadData *)threadData;
  DEBUG3("T2(%.*s, %d): - Completing", SUBSCRIBER_NUMBER_LENGTH, 
	 td->transactionData.number, 
	 td->transactionData.location);
  
  if (result == -1) {
    CHECK_ALLOWED_ERROR("T2: Commit", td, pCON->getNdbError());
    td->pNDB->closeTransaction(pCON);
    start_T2(td->pNDB, td, stat_async);
    return;
  }//if
  td->pNDB->closeTransaction(pCON);
  complete_T2(td);
}

/**
 * Transaction 3 - T3
 *
 * Read session details
 *
 * Input:
 *   SubscriberNumber
 *   ServerId
 *   ServerBit
 *
 * Output:
 *   BranchExecuted
 *   SessionDetails
 *   ChangedBy
 *   ChangedTime
 *   Location
 */
void
start_T3(Ndb * pNDB, ThreadData * td, int async){

  DEBUG3("T3(%.*s, %.2d): - Starting", SUBSCRIBER_NUMBER_LENGTH, 
	 td->transactionData.number, 
	 td->transactionData.server_id);
  
  NdbConnection * pCON = 0;

  while((pCON = startTransaction(pNDB, td)) == 0){
    CHECK_ALLOWED_ERROR("T3-1: startTransaction", td, pNDB->getNdbError());
    NdbSleep_MilliSleep(10);
  }
  
  NdbOperation *MyOp= pCON->getNdbOperation(SUBSCRIBER_TABLE);
  CHECK_NULL(MyOp, "T3-1: getNdbOperation", td,
	     pCON->getNdbError());
  
  MyOp->readTuple();
  MyOp->equal(IND_SUBSCRIBER_NUMBER, 
	      td->transactionData.number);
  MyOp->getValue(IND_SUBSCRIBER_LOCATION, 
		 (char *)&td->transactionData.location);
  MyOp->getValue(IND_SUBSCRIBER_CHANGED_BY, 
		 td->transactionData.changed_by);
  MyOp->getValue(IND_SUBSCRIBER_CHANGED_TIME, 
		 td->transactionData.changed_time);
  MyOp->getValue(IND_SUBSCRIBER_GROUP, 
		 (char *)&td->transactionData.group_id);
  MyOp->getValue(IND_SUBSCRIBER_SESSIONS, 
		 (char *)&td->transactionData.sessions);
  stat_async = async;
  if (async == 1) {
    pCON->executeAsynchPrepare( NoCommit , T3_Callback_1, td);
  } else {
    int result = pCON->execute( NoCommit );
    T3_Callback_1(result, pCON, (void*)td);
    return;
  }//if
}

void
T3_Callback_1(int result, NdbConnection * pCON, void * threadData){
  ThreadData * td = (ThreadData *)threadData;
  DEBUG3("T3(%.*s, %.2d): - Callback 1", SUBSCRIBER_NUMBER_LENGTH, 
	 td->transactionData.number, 
	 td->transactionData.server_id);

  if (result == -1) {
    CHECK_ALLOWED_ERROR("T3-1: execute", td, pCON->getNdbError());
    td->pNDB->closeTransaction(pCON);
    start_T3(td->pNDB, td, stat_async);
    return;
  }//if

  NdbOperation * MyOp = pCON->getNdbOperation(GROUP_TABLE);
  CHECK_NULL(MyOp, "T3-2: getNdbOperation", td,
	     pCON->getNdbError());
    
  MyOp->readTuple();
  MyOp->equal(IND_GROUP_ID,
	      (char*)&td->transactionData.group_id);
  MyOp->getValue(IND_GROUP_ALLOW_READ, 
		 (char *)&td->transactionData.permission);
  if (stat_async == 1) {
    pCON->executeAsynchPrepare( NoCommit , T3_Callback_2, td);
  } else {
    int result = pCON->execute( NoCommit );
    T3_Callback_2(result, pCON, (void*)td);
    return;
  }//if
}

void
T3_Callback_2(int result, NdbConnection * pCON, void * threadData){
  ThreadData * td = (ThreadData *)threadData;
  
  if (result == -1) {
    CHECK_ALLOWED_ERROR("T3-2: execute", td, pCON->getNdbError());
    td->pNDB->closeTransaction(pCON);
    start_T3(td->pNDB, td, stat_async);
    return;
  }//if
  
  Uint32 permission = td->transactionData.permission;
  Uint32 sessions   = td->transactionData.sessions;
  Uint32 server_bit = td->transactionData.server_bit;

  if(((permission & server_bit) == server_bit) &&
     ((sessions   & server_bit) == server_bit)){
    
    memcpy(td->transactionData.suffix,
	   &td->transactionData.number[SFX_START],
	   SUBSCRIBER_NUMBER_SUFFIX_LENGTH);
    DEBUG5("T3(%.*s, %.2d): - Callback 2 - reading(%.*s)", 
	   SUBSCRIBER_NUMBER_LENGTH, 
	   td->transactionData.number, 
	   td->transactionData.server_id,
	   SUBSCRIBER_NUMBER_SUFFIX_LENGTH, 
	   td->transactionData.suffix);
    
    /* Operation 3 */
    NdbOperation * MyOp = pCON->getNdbOperation(SESSION_TABLE);
    CHECK_NULL(MyOp, "T3-3: getNdbOperation", td,
	       pCON->getNdbError());
    
    MyOp->simpleRead();
    MyOp->equal(IND_SESSION_SUBSCRIBER,
		(char*)td->transactionData.number);
    MyOp->equal(IND_SESSION_SERVER,
		(char*)&td->transactionData.server_id);
    MyOp->getValue(IND_SESSION_DATA, 
		   (char *)td->transactionData.session_details);
    
    /* Operation 4 */
    MyOp = pCON->getNdbOperation(SERVER_TABLE);
    CHECK_NULL(MyOp, "T3-4: getNdbOperation", td,
	       pCON->getNdbError());
    
    MyOp->interpretedUpdateTuple();
    MyOp->equal(IND_SERVER_ID,
		(char*)&td->transactionData.server_id);
    MyOp->equal(IND_SERVER_SUBSCRIBER_SUFFIX,
		(char*)td->transactionData.suffix);
    MyOp->incValue(IND_SERVER_READS, (uint32)1);
    td->transactionData.branchExecuted = 1;
  } else {
    DEBUG3("T3(%.*s, %.2d): - Callback 2 - no read",
	   SUBSCRIBER_NUMBER_LENGTH, 
	   td->transactionData.number, 
	   td->transactionData.server_id);
    td->transactionData.branchExecuted = 0;
  }
  if (stat_async == 1) {
    pCON->executeAsynchPrepare( Commit , T3_Callback_3, td);
  } else {
    int result = pCON->execute( Commit );
    T3_Callback_3(result, pCON, (void*)td);
    return;
  }//if
}

void
T3_Callback_3(int result, NdbConnection * pCON, void * threadData){
  ThreadData * td = (ThreadData *)threadData;  
  DEBUG3("T3(%.*s, %.2d): - Completing", SUBSCRIBER_NUMBER_LENGTH, 
	 td->transactionData.number, 
	 td->transactionData.server_id);
  
  if (result == -1) {
    CHECK_ALLOWED_ERROR("T3-3: Commit", td, pCON->getNdbError());
    td->pNDB->closeTransaction(pCON);
    start_T3(td->pNDB, td, stat_async);
    return;
  }//if
  td->pNDB->closeTransaction(pCON);
  complete_T3(td);
}

/**
 * Transaction 4 - T4
 * 
 * Create session
 *
 * Input:
 *   SubscriberNumber
 *   ServerId
 *   ServerBit
 *   SessionDetails,
 *   DoRollback
 * Output:
 *   ChangedBy
 *   ChangedTime
 *   Location
 *   BranchExecuted
 */
void
start_T4(Ndb * pNDB, ThreadData * td, int async){

  DEBUG3("T4(%.*s, %.2d): - Starting", SUBSCRIBER_NUMBER_LENGTH, 
	 td->transactionData.number, 
	 td->transactionData.server_id);
  
  NdbConnection * pCON = 0;
  while((pCON = startTransaction(pNDB, td)) == 0){
    CHECK_ALLOWED_ERROR("T4-1: startTransaction", td, pNDB->getNdbError());
    NdbSleep_MilliSleep(10);
  }
  
  NdbOperation *MyOp= pCON->getNdbOperation(SUBSCRIBER_TABLE);
  CHECK_NULL(MyOp, "T4-1: getNdbOperation", td,
	     pCON->getNdbError());
  
  MyOp->interpretedUpdateTuple();
  MyOp->equal(IND_SUBSCRIBER_NUMBER, 
	      td->transactionData.number);
  MyOp->getValue(IND_SUBSCRIBER_LOCATION, 
		 (char *)&td->transactionData.location);
  MyOp->getValue(IND_SUBSCRIBER_CHANGED_BY, 
		 td->transactionData.changed_by);
  MyOp->getValue(IND_SUBSCRIBER_CHANGED_TIME, 
		 td->transactionData.changed_time);
  MyOp->getValue(IND_SUBSCRIBER_GROUP,
		 (char *)&td->transactionData.group_id);
  MyOp->getValue(IND_SUBSCRIBER_SESSIONS,
		 (char *)&td->transactionData.sessions); 
  MyOp->incValue(IND_SUBSCRIBER_SESSIONS, 
		 (uint32)td->transactionData.server_bit);
  stat_async = async;
  if (async == 1) {
    pCON->executeAsynchPrepare( NoCommit , T4_Callback_1, td);
  } else {
    int result = pCON->execute( NoCommit );
    T4_Callback_1(result, pCON, (void*)td);
    return;
  }//if
}

void
T4_Callback_1(int result, NdbConnection * pCON, void * threadData){
  ThreadData * td = (ThreadData *)threadData;  
  if (result == -1) {
    CHECK_ALLOWED_ERROR("T4-1: execute", td, pCON->getNdbError());
    td->pNDB->closeTransaction(pCON);
    start_T4(td->pNDB, td, stat_async);
    return;
  }//if
  
  DEBUG3("T4(%.*s, %.2d): - Callback 1", 
	 SUBSCRIBER_NUMBER_LENGTH, 
	 td->transactionData.number, 
	 td->transactionData.server_id);


  NdbOperation * MyOp = pCON->getNdbOperation(GROUP_TABLE);
  CHECK_NULL(MyOp, "T4-2: getNdbOperation", td,
	     pCON->getNdbError());
  
  MyOp->readTuple();
  MyOp->equal(IND_GROUP_ID,
	      (char*)&td->transactionData.group_id);
  MyOp->getValue(IND_GROUP_ALLOW_INSERT, 
		 (char *)&td->transactionData.permission);
  if (stat_async == 1) {
    pCON->executeAsynchPrepare( NoCommit , T4_Callback_2, td);
  } else {
    int result = pCON->execute( NoCommit );
    T4_Callback_2(result, pCON, (void*)td);
    return;
  }//if
}

void
T4_Callback_2(int result, NdbConnection * pCON, void * threadData){
  ThreadData * td = (ThreadData *)threadData;  
  if (result == -1) {
    CHECK_ALLOWED_ERROR("T4-2: execute", td, pCON->getNdbError());
    td->pNDB->closeTransaction(pCON);
    start_T4(td->pNDB, td, stat_async);
    return;
  }//if

  Uint32 permission = td->transactionData.permission;
  Uint32 sessions   = td->transactionData.sessions;
  Uint32 server_bit = td->transactionData.server_bit;
  
  if(((permission & server_bit) == server_bit) &&
     ((sessions   & server_bit) == 0)){
    
    memcpy(td->transactionData.suffix,
	   &td->transactionData.number[SFX_START],
	   SUBSCRIBER_NUMBER_SUFFIX_LENGTH);
    
    DEBUG5("T4(%.*s, %.2d): - Callback 2 - inserting(%.*s)", 
	   SUBSCRIBER_NUMBER_LENGTH, 
	   td->transactionData.number, 
	   td->transactionData.server_id,
	   SUBSCRIBER_NUMBER_SUFFIX_LENGTH, 
	   td->transactionData.suffix);
    
    /* Operation 3 */
    
    NdbOperation * MyOp = pCON->getNdbOperation(SESSION_TABLE);
    CHECK_NULL(MyOp, "T4-3: getNdbOperation", td,
	       pCON->getNdbError());
    
    MyOp->insertTuple();
    MyOp->equal(IND_SESSION_SUBSCRIBER,
		(char*)td->transactionData.number);
    MyOp->equal(IND_SESSION_SERVER,
		(char*)&td->transactionData.server_id);
    MyOp->setValue(SESSION_DATA, 
		   (char *)td->transactionData.session_details);
    /* Operation 4 */
    
    /* Operation 5 */
    MyOp = pCON->getNdbOperation(SERVER_TABLE);
    CHECK_NULL(MyOp, "T4-5: getNdbOperation", td,
	       pCON->getNdbError());
    
    MyOp->interpretedUpdateTuple();
    MyOp->equal(IND_SERVER_ID,
		(char*)&td->transactionData.server_id);
    MyOp->equal(IND_SERVER_SUBSCRIBER_SUFFIX,
		(char*)td->transactionData.suffix);
    MyOp->incValue(IND_SERVER_INSERTS, (uint32)1);
    td->transactionData.branchExecuted = 1;
  } else {
    td->transactionData.branchExecuted = 0;
    DEBUG5("T4(%.*s, %.2d): - Callback 2 - %s %s",
	   SUBSCRIBER_NUMBER_LENGTH, 
	   td->transactionData.number, 
	   td->transactionData.server_id,
	   ((permission & server_bit) ? 
	    "permission - " : "no permission - "),
	   ((sessions   & server_bit) ? 
	    "in session - " : "no in session - "));
  }
  
  if(!td->transactionData.do_rollback && td->transactionData.branchExecuted){
    if (stat_async == 1) {
      pCON->executeAsynchPrepare( Commit , T4_Callback_3, td);
    } else {
      int result = pCON->execute( Commit );
      T4_Callback_3(result, pCON, (void*)td);
      return;
    }//if
  } else {
    if (stat_async == 1) {
      pCON->executeAsynchPrepare( Rollback , T4_Callback_3, td);
    } else {
      int result = pCON->execute( Rollback );
      T4_Callback_3(result, pCON, (void*)td);
      return;
    }//if
  }
}

void
T4_Callback_3(int result, NdbConnection * pCON, void * threadData){
  ThreadData * td = (ThreadData *)threadData;  
  if (result == -1) {
    CHECK_ALLOWED_ERROR("T4-3: Commit", td, pCON->getNdbError());
    td->pNDB->closeTransaction(pCON);
    start_T4(td->pNDB, td, stat_async);
    return;
  }//if
  
  DEBUG3("T4(%.*s, %.2d): - Completing", 
	 SUBSCRIBER_NUMBER_LENGTH, 
	 td->transactionData.number, 
	 td->transactionData.server_id);

  td->pNDB->closeTransaction(pCON);
  complete_T4(td);
}

/**
 * Transaction 5 - T5
 * 
 * Delete session
 *
 * Input:
 *   SubscriberNumber
 *   ServerId
 *   ServerBit
 *   DoRollback
 * Output:
 *   ChangedBy
 *   ChangedTime
 *   Location
 *   BranchExecuted
 */
void
start_T5(Ndb * pNDB, ThreadData * td, int async){

  DEBUG3("T5(%.*s, %.2d): - Starting", SUBSCRIBER_NUMBER_LENGTH, 
	 td->transactionData.number, 
	 td->transactionData.server_id);

  NdbConnection * pCON = 0;
  while((pCON = startTransaction(pNDB, td)) == 0){
    CHECK_ALLOWED_ERROR("T5-1: startTransaction", td, pNDB->getNdbError());
    NdbSleep_MilliSleep(10);
  }
  
  NdbOperation * MyOp= pCON->getNdbOperation(SUBSCRIBER_TABLE);
  CHECK_NULL(MyOp, "T5-1: getNdbOperation", td,
	     pCON->getNdbError());
  
  MyOp->interpretedUpdateTuple();
  MyOp->equal(IND_SUBSCRIBER_NUMBER, 
	      td->transactionData.number);
  MyOp->getValue(IND_SUBSCRIBER_LOCATION, 
		 (char *)&td->transactionData.location);
  MyOp->getValue(IND_SUBSCRIBER_CHANGED_BY, 
		 td->transactionData.changed_by);
  MyOp->getValue(IND_SUBSCRIBER_CHANGED_TIME, 
		 td->transactionData.changed_time);
  MyOp->getValue(IND_SUBSCRIBER_GROUP,
		 (char *)&td->transactionData.group_id);
  MyOp->getValue(IND_SUBSCRIBER_SESSIONS,
		 (char *)&td->transactionData.sessions);
  MyOp->subValue(IND_SUBSCRIBER_SESSIONS, 
		 (uint32)td->transactionData.server_bit);
  stat_async = async;
  if (async == 1) {
    pCON->executeAsynchPrepare( NoCommit , T5_Callback_1, td);
  } else {
    int result = pCON->execute( NoCommit );
    T5_Callback_1(result, pCON, (void*)td);
    return;
  }//if
}

void
T5_Callback_1(int result, NdbConnection * pCON, void * threadData){
  ThreadData * td = (ThreadData *)threadData;  
  if (result == -1) {
    CHECK_ALLOWED_ERROR("T5-1: execute", td, pCON->getNdbError());
    td->pNDB->closeTransaction(pCON);
    start_T5(td->pNDB, td, stat_async);
    return;
  }//if

  DEBUG3("T5(%.*s, %.2d): - Callback 1", 
	 SUBSCRIBER_NUMBER_LENGTH, 
	 td->transactionData.number, 
	 td->transactionData.server_id);
  
  NdbOperation * MyOp = pCON->getNdbOperation(GROUP_TABLE);
  CHECK_NULL(MyOp, "T5-2: getNdbOperation", td,
	     pCON->getNdbError());
  
  MyOp->readTuple();
  MyOp->equal(IND_GROUP_ID,
	      (char*)&td->transactionData.group_id);
  MyOp->getValue(IND_GROUP_ALLOW_DELETE, 
		 (char *)&td->transactionData.permission);
  if (stat_async == 1) {
    pCON->executeAsynchPrepare( NoCommit , T5_Callback_2, td);
  } else {
    int result = pCON->execute( NoCommit );
    T5_Callback_2(result, pCON, (void*)td);
    return;
  }//if
}

void
T5_Callback_2(int result, NdbConnection * pCON, void * threadData){
  ThreadData * td = (ThreadData *)threadData;  
  if (result == -1) {
    CHECK_ALLOWED_ERROR("T5-2: execute", td, pCON->getNdbError());
    td->pNDB->closeTransaction(pCON);
    start_T5(td->pNDB, td, stat_async);
    return;
  }//if

  Uint32 permission = td->transactionData.permission;
  Uint32 sessions   = td->transactionData.sessions;
  Uint32 server_bit = td->transactionData.server_bit;

  if(((permission & server_bit) == server_bit) &&
     ((sessions   & server_bit) == server_bit)){
    
    memcpy(td->transactionData.suffix,
	   &td->transactionData.number[SFX_START],
	   SUBSCRIBER_NUMBER_SUFFIX_LENGTH);
    
    DEBUG5("T5(%.*s, %.2d): - Callback 2 - deleting(%.*s)", 
	   SUBSCRIBER_NUMBER_LENGTH, 
	   td->transactionData.number, 
	   td->transactionData.server_id,
	   SUBSCRIBER_NUMBER_SUFFIX_LENGTH, 
	   td->transactionData.suffix);
    
    /* Operation 3 */
    NdbOperation * MyOp = pCON->getNdbOperation(SESSION_TABLE);
    CHECK_NULL(MyOp, "T5-3: getNdbOperation", td,
	       pCON->getNdbError());
    
    MyOp->deleteTuple();
    MyOp->equal(IND_SESSION_SUBSCRIBER,
		(char*)td->transactionData.number);
    MyOp->equal(IND_SESSION_SERVER,
		(char*)&td->transactionData.server_id);
    /* Operation 4 */
    
    /* Operation 5 */
    MyOp = pCON->getNdbOperation(SERVER_TABLE);
    CHECK_NULL(MyOp, "T5-5: getNdbOperation", td,
	       pCON->getNdbError());
    
    MyOp->interpretedUpdateTuple();
    MyOp->equal(IND_SERVER_ID,
		(char*)&td->transactionData.server_id);
    MyOp->equal(IND_SERVER_SUBSCRIBER_SUFFIX,
		(char*)td->transactionData.suffix);
    MyOp->incValue(IND_SERVER_DELETES, (uint32)1);
    td->transactionData.branchExecuted = 1;
  } else {
    td->transactionData.branchExecuted = 0;

    DEBUG5("T5(%.*s, %.2d): - Callback 2 - no delete - %s %s", 
	   SUBSCRIBER_NUMBER_LENGTH, 
	   td->transactionData.number, 
	   td->transactionData.server_id,
	   ((permission & server_bit) ? 
	    "permission - " : "no permission - "),
	   ((sessions   & server_bit) ? 
	    "in session - " : "no in session - "));
  }
  
  if(!td->transactionData.do_rollback && td->transactionData.branchExecuted){
    if (stat_async == 1) {
      pCON->executeAsynchPrepare( Commit , T5_Callback_3, td);
    } else {
      int result = pCON->execute( Commit );
      T5_Callback_3(result, pCON, (void*)td);
      return;
    }//if
  } else {
    if (stat_async == 1) {
      pCON->executeAsynchPrepare( Rollback , T5_Callback_3, td);
    } else {
      int result = pCON->execute( Rollback );
      T5_Callback_3(result, pCON, (void*)td);
      return;
    }//if
  }
}

void
T5_Callback_3(int result, NdbConnection * pCON, void * threadData){
  ThreadData * td = (ThreadData *)threadData;  
  if (result == -1) {
    CHECK_ALLOWED_ERROR("T5-3: Commit", td, pCON->getNdbError());
    td->pNDB->closeTransaction(pCON);
    start_T5(td->pNDB, td, stat_async);
    return;
  }//if
  
  DEBUG3("T5(%.*s, %.2d): - Completing", 
	 SUBSCRIBER_NUMBER_LENGTH, 
	 td->transactionData.number, 
	 td->transactionData.server_id);
  
  td->pNDB->closeTransaction(pCON);
  complete_T5(td);
}
