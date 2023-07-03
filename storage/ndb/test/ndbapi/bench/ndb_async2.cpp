/*
   Copyright (c) 2004, 2023, Oracle and/or its affiliates.

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

//#define DEBUG_ON

#include <ndb_global.h>
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
  return pNDB->startTransaction();
#ifdef OLD_CODE
  return pNDB->startTransactionDGroup (0, 
				       &td->transactionData.number[SFX_START],
				       1);
#endif
}

// NdbRecord helper macros
#define SET_MASK(mask, attrId)                         \
  mask[attrId >> 3] |= (1 << (attrId & 7))

void
start_T1(Ndb * pNDB, ThreadData * td, int async){

  DEBUG2("T1(%.*s): - Starting", SUBSCRIBER_NUMBER_LENGTH, 
	 td->transactionData.number); 

  NdbConnection * pCON = 0;
  while((pCON = startTransaction(pNDB, td)) == 0){
    CHECK_ALLOWED_ERROR("T1: startTransaction", td, pNDB->getNdbError());
    NdbSleep_MilliSleep(10);
  }

  const NdbOperation* op= NULL;

  if (td->ndbRecordSharedData)
  {
    char* rowPtr= (char*) &td->transactionData;
    const NdbRecord* record= td->ndbRecordSharedData->
      subscriberTableNdbRecord;
    Uint32 m=0;
    unsigned char* mask= (unsigned char*) &m;

    //SET_MASK(mask, IND_SUBSCRIBER_NUMBER);
    SET_MASK(mask, IND_SUBSCRIBER_LOCATION);
    SET_MASK(mask, IND_SUBSCRIBER_CHANGED_BY);
    SET_MASK(mask, IND_SUBSCRIBER_CHANGED_TIME);
    
    op= pCON->updateTuple(record,
                          rowPtr,
                          record,
                          rowPtr,
                          mask);
  }
  else
  {
    NdbOperation *MyOp = pCON->getNdbOperation(SUBSCRIBER_TABLE);
    op= MyOp;
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
    }
  }

  if (op != NULL)
  {
    if (async == 1) {
      pCON->executeAsynchPrepare( Commit , T1_Callback, td);
    } else {
      int result = pCON->execute(Commit);
      T1_Callback(result, pCON, (void*)td);
      return;
    }//if
  } else {
    CHECK_NULL(NULL, "T1: getNdbOperation", td, pCON->getNdbError());
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

  if (td->ndbRecordSharedData)
  {
    char* rowPtr= (char*) &td->transactionData;
    const NdbRecord* record= td->ndbRecordSharedData->
      subscriberTableNdbRecord;
    Uint32 m=0;
    unsigned char* mask= (unsigned char*) &m;

    SET_MASK(mask, IND_SUBSCRIBER_LOCATION);
    SET_MASK(mask, IND_SUBSCRIBER_CHANGED_BY);
    SET_MASK(mask, IND_SUBSCRIBER_CHANGED_TIME);
    SET_MASK(mask, IND_SUBSCRIBER_NAME);

    const NdbOperation* MyOp= pCON->readTuple(record, rowPtr, record, rowPtr, 
                                              NdbOperation::LM_Read, mask);
    CHECK_NULL((void*) MyOp, "T2: readTuple", td,
               pCON->getNdbError());
  }
  else
  {
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
  }

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

  const NdbOperation* op;

  if (td->ndbRecordSharedData)
  {
    char* rowPtr= (char*) &td->transactionData;
    const NdbRecord* record= td->ndbRecordSharedData->
      subscriberTableNdbRecord;
    Uint32 m=0;
    unsigned char* mask= (unsigned char*) &m;

    SET_MASK(mask, IND_SUBSCRIBER_LOCATION);
    SET_MASK(mask, IND_SUBSCRIBER_CHANGED_BY);
    SET_MASK(mask, IND_SUBSCRIBER_CHANGED_TIME);
    SET_MASK(mask, IND_SUBSCRIBER_GROUP);
    SET_MASK(mask, IND_SUBSCRIBER_SESSIONS);

    op= pCON->readTuple(record, rowPtr, record, rowPtr,
                        NdbOperation::LM_Read, mask);
  }
  else
  {
    NdbOperation *MyOp= pCON->getNdbOperation(SUBSCRIBER_TABLE);
    op= MyOp;
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
  }

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

  const NdbOperation* op= NULL;

  if (td->ndbRecordSharedData)
  {
    char* rowPtr= (char*) &td->transactionData;
    const NdbRecord* record= td->ndbRecordSharedData->
      groupTableAllowReadNdbRecord;
    Uint32 m=0;
    unsigned char* mask= (unsigned char*) &m;
    
    SET_MASK(mask, IND_GROUP_ALLOW_READ);

    op= pCON->readTuple(record, rowPtr, record, rowPtr,
                        NdbOperation::LM_Read, mask);
  }
  else
  {
    NdbOperation * MyOp = pCON->getNdbOperation(GROUP_TABLE);
    op= MyOp;
    CHECK_NULL(MyOp, "T3-2: getNdbOperation", td,
               pCON->getNdbError());
    
    MyOp->readTuple();
    MyOp->equal(IND_GROUP_ID,
                (char*)&td->transactionData.group_id);
    MyOp->getValue(IND_GROUP_ALLOW_READ, 
                   (char *)&td->transactionData.permission);
  }

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
    
    /* Operations 3 + 4 */
    if (td->ndbRecordSharedData)
    {
      /* Op 3 */
      char* rowPtr= (char*) &td->transactionData;
      const NdbRecord* record= td->ndbRecordSharedData->
        sessionTableNdbRecord;
      Uint32 m=0;
      unsigned char* mask= (unsigned char*) &m;

      SET_MASK(mask, IND_SESSION_DATA);

      const NdbOperation* MyOp = pCON->readTuple(record, rowPtr, record, rowPtr,
                                                 NdbOperation::LM_SimpleRead,
                                                 mask);
      CHECK_NULL((void*) MyOp, "T3-3: readTuple", td,
                 pCON->getNdbError());

      /* Op 4 */
      record= td->ndbRecordSharedData->
        serverTableNdbRecord;
      m= 0;

      /* Attach interpreted program */
      NdbOperation::OperationOptions opts;
      opts.optionsPresent= NdbOperation::OperationOptions::OO_INTERPRETED;
      opts.interpretedCode= td->ndbRecordSharedData->incrServerReadsProg;

      MyOp= pCON->updateTuple(record, rowPtr, record, rowPtr, mask,
                              &opts,
                              sizeof(opts));
      CHECK_NULL((void*) MyOp, "T3-3: updateTuple", td,
                 pCON->getNdbError());
    }
    else
    {
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
      
      MyOp = pCON->getNdbOperation(SERVER_TABLE);
      CHECK_NULL(MyOp, "T3-4: getNdbOperation", td,
                 pCON->getNdbError());
      
      MyOp->interpretedUpdateTuple();
      MyOp->equal(IND_SERVER_ID,
                  (char*)&td->transactionData.server_id);
      MyOp->equal(IND_SERVER_SUBSCRIBER_SUFFIX,
                  (char*)td->transactionData.suffix);
      MyOp->incValue(IND_SERVER_READS, (uint32)1);
    }

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

  if (td->ndbRecordSharedData)
  {
    char* rowPtr= (char*) &td->transactionData;
    const NdbRecord* record= td->ndbRecordSharedData->
      subscriberTableNdbRecord;
    Uint32 m=0;
    unsigned char* mask= (unsigned char*) &m;

    SET_MASK(mask, IND_SUBSCRIBER_LOCATION);
    SET_MASK(mask, IND_SUBSCRIBER_CHANGED_BY);
    SET_MASK(mask, IND_SUBSCRIBER_CHANGED_TIME);
    SET_MASK(mask, IND_SUBSCRIBER_GROUP);
    SET_MASK(mask, IND_SUBSCRIBER_SESSIONS);
    
    const NdbOperation* MyOp= pCON->readTuple(record, rowPtr, record, rowPtr,
                                              NdbOperation::LM_Read,
                                              mask);
    CHECK_NULL((void*)MyOp, "T4-1: readTuple", td,
               pCON->getNdbError());

    m= 0;

    /* Create program to add something to the subscriber 
     * sessions column 
     */
    Uint32 codeBuf[20];

    for (Uint32 p=0; p<20; p++)
      codeBuf[p]= 0;

    NdbInterpretedCode program(pNDB->getDictionary()->
                               getTable(SUBSCRIBER_TABLE),
                               codeBuf,
                               20);

    if (program.add_val(IND_SUBSCRIBER_SESSIONS, 
                        (uint32)td->transactionData.server_bit) || 
        program.interpret_exit_ok() ||
        program.finalise())
    {
      CHECK_NULL(NULL , "T4-1: Program create failed", td,
                 program.getNdbError());
    }

    NdbOperation::OperationOptions opts;
    opts.optionsPresent= NdbOperation::OperationOptions::OO_INTERPRETED;
    opts.interpretedCode= &program;

    MyOp= pCON->updateTuple(record, rowPtr, record, rowPtr,
                            mask,
                            &opts,
                            sizeof(opts));
    CHECK_NULL((void*)MyOp, "T4-1: updateTuple", td,
               pCON->getNdbError());

  }
  else
  {
    /* Use old Api */
    if (td->useCombinedUpdate)
    {
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
    }
    else
    {
      /* Separate read op + update op 
       * Relies on relative ordering of operation execution on a single
       * row
       */
      NdbOperation *MyOp= pCON->getNdbOperation(SUBSCRIBER_TABLE);
      CHECK_NULL(MyOp, "T4-1: getNdbOperation (read)", td,
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
      MyOp= pCON->getNdbOperation(SUBSCRIBER_TABLE);
      CHECK_NULL(MyOp, "T4-1: getNdbOperation (update)", td,
                 pCON->getNdbError());
      MyOp->interpretedUpdateTuple();
      MyOp->equal(IND_SUBSCRIBER_NUMBER, 
                  td->transactionData.number);
      MyOp->incValue(IND_SUBSCRIBER_SESSIONS, 
                     (uint32)td->transactionData.server_bit);
    }
  }
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


  if (td->ndbRecordSharedData)
  {
    char* rowPtr= (char*) &td->transactionData;
    const NdbRecord* record= td->ndbRecordSharedData->
      groupTableAllowInsertNdbRecord;
    Uint32 m=0;
    unsigned char* mask= (unsigned char*) &m;

    SET_MASK(mask, IND_GROUP_ALLOW_INSERT);

    const NdbOperation* MyOp= pCON->readTuple(record, rowPtr, record, rowPtr,
                                              NdbOperation::LM_Read,
                                              mask);
    
    CHECK_NULL((void*)MyOp, "T4-2: readTuple", td,
               pCON->getNdbError());
  }
  else
  {
    NdbOperation * MyOp = pCON->getNdbOperation(GROUP_TABLE);
    CHECK_NULL(MyOp, "T4-2: getNdbOperation", td,
               pCON->getNdbError());
    
    MyOp->readTuple();
    MyOp->equal(IND_GROUP_ID,
                (char*)&td->transactionData.group_id);
    MyOp->getValue(IND_GROUP_ALLOW_INSERT, 
                   (char *)&td->transactionData.permission);
  }
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
    
    /* Operations 3 + 4 */
   
    if (td->ndbRecordSharedData)
    {
      char* rowPtr= (char*) &td->transactionData;
      const NdbRecord* record= td->ndbRecordSharedData->
        sessionTableNdbRecord;
      Uint32 m=0;
      unsigned char* mask= (unsigned char*) &m;

      SET_MASK(mask, IND_SESSION_SUBSCRIBER);
      SET_MASK(mask, IND_SESSION_SERVER);
      SET_MASK(mask, IND_SESSION_DATA);

      const NdbOperation* MyOp= pCON->insertTuple(record, rowPtr, mask);

      CHECK_NULL((void*)MyOp, "T4-3: insertTuple", td,
                 pCON->getNdbError());

      record= td->ndbRecordSharedData->
        serverTableNdbRecord;
      m= 0;
      
      NdbOperation::OperationOptions opts;
      opts.optionsPresent= NdbOperation::OperationOptions::OO_INTERPRETED;
      opts.interpretedCode= td->ndbRecordSharedData->incrServerInsertsProg;
      
      MyOp= pCON->updateTuple(record, rowPtr, record, rowPtr, mask,
                              &opts, sizeof(opts));

      CHECK_NULL((void*)MyOp, "T4-3: updateTuple", td,
                 pCON->getNdbError());
    }
    else
    {
      NdbOperation * MyOp = pCON->getNdbOperation(SESSION_TABLE);
      CHECK_NULL(MyOp, "T4-3: getNdbOperation", td,
                 pCON->getNdbError());
      
      MyOp->insertTuple();
      MyOp->equal(IND_SESSION_SUBSCRIBER,
                  (char*)td->transactionData.number);
      MyOp->equal(IND_SESSION_SERVER,
                  (char*)&td->transactionData.server_id);
      MyOp->setValue(IND_SESSION_DATA, 
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
    }
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
  
  if (td->ndbRecordSharedData)
  {    
    char* rowPtr= (char*) &td->transactionData;
    const NdbRecord* record= td->ndbRecordSharedData->
      subscriberTableNdbRecord;
    Uint32 m=0;
    unsigned char* mask= (unsigned char*) &m;

    SET_MASK(mask, IND_SUBSCRIBER_LOCATION);
    SET_MASK(mask, IND_SUBSCRIBER_CHANGED_BY);
    SET_MASK(mask, IND_SUBSCRIBER_CHANGED_TIME);
    SET_MASK(mask, IND_SUBSCRIBER_GROUP);
    SET_MASK(mask, IND_SUBSCRIBER_SESSIONS);

    const NdbOperation* MyOp= pCON->readTuple(record, rowPtr, record, rowPtr,
                                              NdbOperation::LM_Read,
                                              mask);
    CHECK_NULL((void*)MyOp, "T5-1: readTuple", td,
               pCON->getNdbError());

    m= 0;

    /* Create program to subtract something from the 
     * subscriber sessions column
     */
    Uint32 codeBuf[20];
    NdbInterpretedCode program(pNDB->getDictionary()->
                               getTable(SUBSCRIBER_TABLE),
                               codeBuf,
                               20);
    if (program.sub_val(IND_SUBSCRIBER_SESSIONS, 
                        (uint32)td->transactionData.server_bit) ||
        program.interpret_exit_ok() ||
        program.finalise())
    {
      CHECK_NULL(NULL , "T5: Program create failed", td,
                 program.getNdbError());
    }
    NdbOperation::OperationOptions opts;
    opts.optionsPresent= NdbOperation::OperationOptions::OO_INTERPRETED;
    opts.interpretedCode= &program;  

    MyOp= pCON->updateTuple(record, rowPtr, record, rowPtr,
                            mask,
                            &opts,
                            sizeof(opts));
    CHECK_NULL((void*)MyOp, "T5-1: updateTuple", td,
               pCON->getNdbError());
  }
  else
  {
    /* Use old Api */
    if (td->useCombinedUpdate)
    {
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
    }
    else
    {
      /* Use separate read and update operations
       * This relies on execution ordering between operations on
       * the same row
       */
      NdbOperation * MyOp= pCON->getNdbOperation(SUBSCRIBER_TABLE);
      CHECK_NULL(MyOp, "T5-1: getNdbOperation (readTuple)", td,
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

      MyOp= pCON->getNdbOperation(SUBSCRIBER_TABLE);
      CHECK_NULL(MyOp, "T5-1: getNdbOperation (updateTuple)", td,
                 pCON->getNdbError());
      MyOp->interpretedUpdateTuple();
      MyOp->equal(IND_SUBSCRIBER_NUMBER, 
                  td->transactionData.number);
      MyOp->subValue(IND_SUBSCRIBER_SESSIONS, 
                     (uint32)td->transactionData.server_bit);
    }
  }
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
  
  if (td->ndbRecordSharedData)
  {
    char* rowPtr= (char*) &td->transactionData;
    const NdbRecord* record= td->ndbRecordSharedData->
      groupTableAllowDeleteNdbRecord;
    Uint32 m=0;
    unsigned char* mask= (unsigned char*) &m;

    SET_MASK(mask, IND_GROUP_ALLOW_DELETE);

    const NdbOperation* MyOp= pCON->readTuple(record, rowPtr, record, rowPtr,
                                              NdbOperation::LM_Read,
                                              mask);

    CHECK_NULL((void*)MyOp, "T5-2: readTuple", td,
               pCON->getNdbError());
  }
  else
  {
    NdbOperation * MyOp = pCON->getNdbOperation(GROUP_TABLE);
    CHECK_NULL(MyOp, "T5-2: getNdbOperation", td,
               pCON->getNdbError());
    
    MyOp->readTuple();
    MyOp->equal(IND_GROUP_ID,
                (char*)&td->transactionData.group_id);
    MyOp->getValue(IND_GROUP_ALLOW_DELETE, 
                   (char *)&td->transactionData.permission);
  }

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
    
    if (td->ndbRecordSharedData)
    {
      char* rowPtr= (char*) &td->transactionData;
      const NdbRecord* record= td->ndbRecordSharedData->
        sessionTableNdbRecord;
      Uint32 m=0;
      unsigned char* mask= (unsigned char*) &m;
      
      const NdbOperation* MyOp= pCON->deleteTuple(record, rowPtr, record);
      CHECK_NULL((void*) MyOp, "T5-3: deleteTuple", td,
                 pCON->getNdbError());

      record= td->ndbRecordSharedData->
        serverTableNdbRecord;
      m= 0;

      NdbOperation::OperationOptions opts;
      opts.optionsPresent= NdbOperation::OperationOptions::OO_INTERPRETED;
      opts.interpretedCode= td->ndbRecordSharedData->incrServerDeletesProg;
      
      MyOp= pCON->updateTuple(record, rowPtr, record, rowPtr, mask,
                              &opts, sizeof(opts));

      CHECK_NULL((void*)MyOp, "T5-2: updateTuple", td,
                 pCON->getNdbError());
    }
    else
    {
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
    }
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
