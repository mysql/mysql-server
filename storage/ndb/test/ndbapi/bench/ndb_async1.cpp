/*
   Copyright (c) 2005, 2024, Oracle and/or its affiliates.
    Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

//#define DEBUG_ON

#include "userInterface.h"

#include "macros.h"
#include "ndb_error.hpp"
#include "ndb_schema.hpp"

#include <NdbApi.hpp>

inline NdbConnection *startTransaction(Ndb *pNDB, ServerId inServerId,
                                       const SubscriberNumber inNumber) {
  const int keyDataLenBytes = sizeof(ServerId) + SUBSCRIBER_NUMBER_LENGTH;
  const int keyDataLen_64Words = keyDataLenBytes >> 3;

  Uint64 keyDataBuf[keyDataLen_64Words + 1];  // The "+1" is for rounding...

  char *keyDataBuf_charP = (char *)&keyDataBuf[0];
  Uint32 *keyDataBuf_wo32P = (Uint32 *)&keyDataBuf[0];

  // Server Id comes first
  keyDataBuf_wo32P[0] = inServerId;
  // Then subscriber number
  memcpy(&keyDataBuf_charP[sizeof(ServerId)], inNumber,
         SUBSCRIBER_NUMBER_LENGTH);

  return pNDB->startTransaction(0, keyDataBuf_charP, keyDataLenBytes);
}

void T1_Callback(int result, NdbConnection *pCon, void *threadData);
void T2_Callback(int result, NdbConnection *pCon, void *threadData);
void T3_Callback_1(int result, NdbConnection *pCon, void *threadData);
void T3_Callback_2(int result, NdbConnection *pCon, void *threadData);
void T3_Callback_3(int result, NdbConnection *pCon, void *threadData);
void T4_Callback_1(int result, NdbConnection *pCon, void *threadData);
void T4_Callback_2(int result, NdbConnection *pCon, void *threadData);
void T4_Callback_3(int result, NdbConnection *pCon, void *threadData);
void T5_Callback_1(int result, NdbConnection *pCon, void *threadData);
void T5_Callback_2(int result, NdbConnection *pCon, void *threadData);
void T5_Callback_3(int result, NdbConnection *pCon, void *threadData);

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
void start_T1(Ndb *pNDB, ThreadData *td) {
  DEBUG2("T1(%.*s): - Starting\n", SUBSCRIBER_NUMBER_LENGTH,
         td->transactionData.number);

  int check;
  NdbConnection *pCON = pNDB->startTransaction();
  if (pCON != NULL) {
    NdbOperation *MyOp = pCON->getNdbOperation(SUBSCRIBER_TABLE);
    if (MyOp != NULL) {
      MyOp->updateTuple();
      MyOp->equal(IND_SUBSCRIBER_NUMBER, td->transactionData.number);
      MyOp->setValue(IND_SUBSCRIBER_LOCATION,
                     (char *)&td->transactionData.location);
      MyOp->setValue(IND_SUBSCRIBER_CHANGED_BY, td->transactionData.changed_by);
      MyOp->setValue(IND_SUBSCRIBER_CHANGED_TIME,
                     td->transactionData.changed_time);
      pCON->executeAsynchPrepare(Commit, T1_Callback, td);
    } else {
      CHECK_NULL(MyOp, "T1: getNdbOperation", pCON);
    }  // if
  } else {
    error_handler("T1-1: startTranscation", pNDB->getNdbErrorString(),
                  pNDB->getNdbError());
  }  // if
}

void T1_Callback(int result, NdbConnection *pCON, void *threadData) {
  ThreadData *td = (ThreadData *)threadData;

  DEBUG2("T1(%.*s): - Completing\n", SUBSCRIBER_NUMBER_LENGTH,
         td->transactionData.number);

  CHECK_MINUS_ONE(result, "T1: Commit", pCON);
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
void start_T2(Ndb *pNDB, ThreadData *td) {
  DEBUG3("T2(%.*s, %p): - Starting\n", SUBSCRIBER_NUMBER_LENGTH,
         td->transactionData.number, td->transactionData.location);

  int check;
  NdbRecAttr *check2;

  NdbConnection *pCON = pNDB->startTransaction();
  if (pCON == NULL)
    error_handler("T2-1: startTransaction", pNDB->getNdbErrorString(),
                  pNDB->getNdbError());

  NdbOperation *MyOp = pCON->getNdbOperation(SUBSCRIBER_TABLE);
  CHECK_NULL(MyOp, "T2: getNdbOperation", pCON);

  MyOp->readTuple();
  MyOp->equal(IND_SUBSCRIBER_NUMBER, td->transactionData.number);
  MyOp->getValue(IND_SUBSCRIBER_LOCATION,
                 (char *)&td->transactionData.location);
  MyOp->getValue(IND_SUBSCRIBER_CHANGED_BY, td->transactionData.changed_by);
  MyOp->getValue(IND_SUBSCRIBER_CHANGED_TIME, td->transactionData.changed_time);
  MyOp->getValue(IND_SUBSCRIBER_NAME, td->transactionData.name);
  pCON->executeAsynchPrepare(Commit, T2_Callback, td);
}

void T2_Callback(int result, NdbConnection *pCON, void *threadData) {
  ThreadData *td = (ThreadData *)threadData;
  DEBUG3("T2(%.*s, %p): - Completing\n", SUBSCRIBER_NUMBER_LENGTH,
         td->transactionData.number, td->transactionData.location);

  CHECK_MINUS_ONE(result, "T2: Commit", pCON);
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
void start_T3(Ndb *pNDB, ThreadData *td) {
  DEBUG3("T3(%.*s, %.2d): - Starting\n", SUBSCRIBER_NUMBER_LENGTH,
         td->transactionData.number, td->transactionData.server_id);

  int check;
  NdbRecAttr *check2;

  NdbConnection *pCON = startTransaction(pNDB, td->transactionData.server_id,
                                         td->transactionData.number);
  if (pCON == NULL)
    error_handler("T3-1: startTranscation", pNDB->getNdbErrorString(),
                  pNDB->getNdbError());

  NdbOperation *MyOp = pCON->getNdbOperation(SUBSCRIBER_TABLE);
  CHECK_NULL(MyOp, "T3-1: getNdbOperation", pCON);

  MyOp->readTuple();
  MyOp->equal(IND_SUBSCRIBER_NUMBER, td->transactionData.number);
  MyOp->getValue(IND_SUBSCRIBER_LOCATION,
                 (char *)&td->transactionData.location);
  MyOp->getValue(IND_SUBSCRIBER_CHANGED_BY, td->transactionData.changed_by);
  MyOp->getValue(IND_SUBSCRIBER_CHANGED_TIME, td->transactionData.changed_time);
  MyOp->getValue(IND_SUBSCRIBER_GROUP, (char *)&td->transactionData.group_id);
  MyOp->getValue(IND_SUBSCRIBER_SESSIONS,
                 (char *)&td->transactionData.sessions);
  pCON->executeAsynchPrepare(NoCommit, T3_Callback_1, td);
}

void T3_Callback_1(int result, NdbConnection *pCON, void *threadData) {
  ThreadData *td = (ThreadData *)threadData;
  DEBUG3("T3(%.*s, %.2d): - Callback 1\n", SUBSCRIBER_NUMBER_LENGTH,
         td->transactionData.number, td->transactionData.server_id);

  CHECK_MINUS_ONE(result, "T3-1: NoCommit", pCON);

  NdbOperation *MyOp = pCON->getNdbOperation(GROUP_TABLE);
  CHECK_NULL(MyOp, "T3-2: getNdbOperation", pCON);

  MyOp->readTuple();
  MyOp->equal(IND_GROUP_ID, (char *)&td->transactionData.group_id);
  MyOp->getValue(IND_GROUP_ALLOW_READ, (char *)&td->transactionData.permission);
  pCON->executeAsynchPrepare(NoCommit, T3_Callback_2, td);
}

void T3_Callback_2(int result, NdbConnection *pCON, void *threadData) {
  ThreadData *td = (ThreadData *)threadData;

  CHECK_MINUS_ONE(result, "T3-2: NoCommit", pCON);

  Uint32 permission = td->transactionData.permission;
  Uint32 sessions = td->transactionData.sessions;
  Uint32 server_bit = td->transactionData.server_bit;

  if (((permission & server_bit) == server_bit) &&
      ((sessions & server_bit) == server_bit)) {
    memcpy(td->transactionData.suffix,
           &td->transactionData.number[SUBSCRIBER_NUMBER_LENGTH -
                                       SUBSCRIBER_NUMBER_SUFFIX_LENGTH],
           SUBSCRIBER_NUMBER_SUFFIX_LENGTH);
    DEBUG5("T3(%.*s, %.2d): - Callback 2 - reading(%.*s)\n",
           SUBSCRIBER_NUMBER_LENGTH, td->transactionData.number,
           td->transactionData.server_id, SUBSCRIBER_NUMBER_SUFFIX_LENGTH,
           td->transactionData.suffix);

    /* Operation 3 */
    NdbOperation *MyOp = pCON->getNdbOperation(SESSION_TABLE);
    CHECK_NULL(MyOp, "T3-3: getNdbOperation", pCON);

    MyOp->simpleRead();
    MyOp->equal(IND_SESSION_SUBSCRIBER, (char *)td->transactionData.number);
    MyOp->equal(IND_SESSION_SERVER, (char *)&td->transactionData.server_id);
    MyOp->getValue(IND_SESSION_DATA,
                   (char *)td->transactionData.session_details);

    /* Operation 4 */
    MyOp = pCON->getNdbOperation(SERVER_TABLE);
    CHECK_NULL(MyOp, "T3-4: getNdbOperation", pCON);

    MyOp->interpretedUpdateTuple();
    MyOp->equal(IND_SERVER_ID, (char *)&td->transactionData.server_id);
    MyOp->equal(IND_SERVER_SUBSCRIBER_SUFFIX,
                (char *)td->transactionData.suffix);
    MyOp->incValue(IND_SERVER_READS, (uint32)1);
    td->transactionData.branchExecuted = 1;
  } else {
    DEBUG3("T3(%.*s, %.2d): - Callback 2 - no read\n", SUBSCRIBER_NUMBER_LENGTH,
           td->transactionData.number, td->transactionData.server_id);
    td->transactionData.branchExecuted = 0;
  }
  pCON->executeAsynchPrepare(Commit, T3_Callback_3, td);
}

void T3_Callback_3(int result, NdbConnection *pCON, void *threadData) {
  ThreadData *td = (ThreadData *)threadData;
  DEBUG3("T3(%.*s, %.2d): - Completing\n", SUBSCRIBER_NUMBER_LENGTH,
         td->transactionData.number, td->transactionData.server_id);

  CHECK_MINUS_ONE(result, "T3-3: Commit", pCON);

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
void start_T4(Ndb *pNDB, ThreadData *td) {
  DEBUG3("T4(%.*s, %.2d): - Starting\n", SUBSCRIBER_NUMBER_LENGTH,
         td->transactionData.number, td->transactionData.server_id);

  int check;
  NdbRecAttr *check2;

  NdbConnection *pCON = startTransaction(pNDB, td->transactionData.server_id,
                                         td->transactionData.number);
  if (pCON == NULL)
    error_handler("T4-1: startTranscation", pNDB->getNdbErrorString(),
                  pNDB->getNdbError());

  NdbOperation *MyOp = pCON->getNdbOperation(SUBSCRIBER_TABLE);
  CHECK_NULL(MyOp, "T4-1: getNdbOperation", pCON);

  MyOp->interpretedUpdateTuple();
  MyOp->equal(IND_SUBSCRIBER_NUMBER, td->transactionData.number);
  MyOp->getValue(IND_SUBSCRIBER_LOCATION,
                 (char *)&td->transactionData.location);
  MyOp->getValue(IND_SUBSCRIBER_CHANGED_BY, td->transactionData.changed_by);
  MyOp->getValue(IND_SUBSCRIBER_CHANGED_TIME, td->transactionData.changed_time);
  MyOp->getValue(IND_SUBSCRIBER_GROUP, (char *)&td->transactionData.group_id);
  MyOp->getValue(IND_SUBSCRIBER_SESSIONS,
                 (char *)&td->transactionData.sessions);
  MyOp->incValue(IND_SUBSCRIBER_SESSIONS,
                 (uint32)td->transactionData.server_bit);
  pCON->executeAsynchPrepare(NoCommit, T4_Callback_1, td);
}

void T4_Callback_1(int result, NdbConnection *pCON, void *threadData) {
  CHECK_MINUS_ONE(result, "T4-1: NoCommit", pCON);
  ThreadData *td = (ThreadData *)threadData;

  DEBUG3("T4(%.*s, %.2d): - Callback 1\n", SUBSCRIBER_NUMBER_LENGTH,
         td->transactionData.number, td->transactionData.server_id);

  NdbOperation *MyOp = pCON->getNdbOperation(GROUP_TABLE);
  CHECK_NULL(MyOp, "T4-2: getNdbOperation", pCON);

  MyOp->readTuple();
  MyOp->equal(IND_GROUP_ID, (char *)&td->transactionData.group_id);
  MyOp->getValue(IND_GROUP_ALLOW_INSERT,
                 (char *)&td->transactionData.permission);
  pCON->executeAsynchPrepare(NoCommit, T4_Callback_2, td);
}

void T4_Callback_2(int result, NdbConnection *pCON, void *threadData) {
  CHECK_MINUS_ONE(result, "T4-2: NoCommit", pCON);
  ThreadData *td = (ThreadData *)threadData;

  Uint32 permission = td->transactionData.permission;
  Uint32 sessions = td->transactionData.sessions;
  Uint32 server_bit = td->transactionData.server_bit;

  if (((permission & server_bit) == server_bit) &&
      ((sessions & server_bit) == 0)) {
    memcpy(td->transactionData.suffix,
           &td->transactionData.number[SUBSCRIBER_NUMBER_LENGTH -
                                       SUBSCRIBER_NUMBER_SUFFIX_LENGTH],
           SUBSCRIBER_NUMBER_SUFFIX_LENGTH);

    DEBUG5("T4(%.*s, %.2d): - Callback 2 - inserting(%.*s)\n",
           SUBSCRIBER_NUMBER_LENGTH, td->transactionData.number,
           td->transactionData.server_id, SUBSCRIBER_NUMBER_SUFFIX_LENGTH,
           td->transactionData.suffix);

    /* Operation 3 */

    NdbOperation *MyOp = pCON->getNdbOperation(SESSION_TABLE);
    CHECK_NULL(MyOp, "T4-3: getNdbOperation", pCON);

    MyOp->insertTuple();
    MyOp->equal(IND_SESSION_SUBSCRIBER, (char *)td->transactionData.number);
    MyOp->equal(IND_SESSION_SERVER, (char *)&td->transactionData.server_id);
    MyOp->setValue(SESSION_DATA, (char *)td->transactionData.session_details);
    /* Operation 4 */

    /* Operation 5 */
    MyOp = pCON->getNdbOperation(SERVER_TABLE);
    CHECK_NULL(MyOp, "T4-5: getNdbOperation", pCON);

    MyOp->interpretedUpdateTuple();
    MyOp->equal(IND_SERVER_ID, (char *)&td->transactionData.server_id);
    MyOp->equal(IND_SERVER_SUBSCRIBER_SUFFIX,
                (char *)td->transactionData.suffix);
    MyOp->incValue(IND_SERVER_INSERTS, (uint32)1);
    td->transactionData.branchExecuted = 1;
  } else {
    td->transactionData.branchExecuted = 0;
    DEBUG5("T4(%.*s, %.2d): - Callback 2 - %s %s\n", SUBSCRIBER_NUMBER_LENGTH,
           td->transactionData.number, td->transactionData.server_id,
           ((permission & server_bit) ? "permission - " : "no permission - "),
           ((sessions & server_bit) ? "in session - " : "no in session - "));
  }

  if (!td->transactionData.do_rollback && td->transactionData.branchExecuted) {
    pCON->executeAsynchPrepare(Commit, T4_Callback_3, td);
  } else {
    pCON->executeAsynchPrepare(Rollback, T4_Callback_3, td);
  }
}

void T4_Callback_3(int result, NdbConnection *pCON, void *threadData) {
  CHECK_MINUS_ONE(result, "T4-3: Commit", pCON);
  ThreadData *td = (ThreadData *)threadData;

  DEBUG3("T4(%.*s, %.2d): - Completing\n", SUBSCRIBER_NUMBER_LENGTH,
         td->transactionData.number, td->transactionData.server_id);

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
void start_T5(Ndb *pNDB, ThreadData *td) {
  DEBUG3("T5(%.*s, %.2d): - Starting\n", SUBSCRIBER_NUMBER_LENGTH,
         td->transactionData.number, td->transactionData.server_id);

  int check;
  NdbRecAttr *check2;

  NdbConnection *pCON = pNDB->startTransaction();
  if (pCON == NULL)
    error_handler("T5-1: startTranscation", pNDB->getNdbErrorString(),
                  pNDB->getNdbError());

  NdbOperation *MyOp = pCON->getNdbOperation(SUBSCRIBER_TABLE);
  CHECK_NULL(MyOp, "T5-1: getNdbOperation", pCON);

  MyOp->interpretedUpdateTuple();
  MyOp->equal(IND_SUBSCRIBER_NUMBER, td->transactionData.number);
  MyOp->getValue(IND_SUBSCRIBER_LOCATION,
                 (char *)&td->transactionData.location);
  MyOp->getValue(IND_SUBSCRIBER_CHANGED_BY, td->transactionData.changed_by);
  MyOp->getValue(IND_SUBSCRIBER_CHANGED_TIME, td->transactionData.changed_time);
  MyOp->getValue(IND_SUBSCRIBER_GROUP, (char *)&td->transactionData.group_id);
  MyOp->getValue(IND_SUBSCRIBER_SESSIONS,
                 (char *)&td->transactionData.sessions);
  MyOp->subValue(IND_SUBSCRIBER_SESSIONS,
                 (uint32)td->transactionData.server_bit);
  pCON->executeAsynchPrepare(NoCommit, T5_Callback_1, td);
}

void T5_Callback_1(int result, NdbConnection *pCON, void *threadData) {
  CHECK_MINUS_ONE(result, "T5-1: NoCommit", pCON);
  ThreadData *td = (ThreadData *)threadData;

  DEBUG3("T5(%.*s, %.2d): - Callback 1\n", SUBSCRIBER_NUMBER_LENGTH,
         td->transactionData.number, td->transactionData.server_id);

  NdbOperation *MyOp = pCON->getNdbOperation(GROUP_TABLE);
  CHECK_NULL(MyOp, "T5-2: getNdbOperation", pCON);

  MyOp->readTuple();
  MyOp->equal(IND_GROUP_ID, (char *)&td->transactionData.group_id);
  MyOp->getValue(IND_GROUP_ALLOW_DELETE,
                 (char *)&td->transactionData.permission);
  pCON->executeAsynchPrepare(NoCommit, T5_Callback_2, td);
}

void T5_Callback_2(int result, NdbConnection *pCON, void *threadData) {
  CHECK_MINUS_ONE(result, "T5-2: NoCommit", pCON);
  ThreadData *td = (ThreadData *)threadData;

  Uint32 permission = td->transactionData.permission;
  Uint32 sessions = td->transactionData.sessions;
  Uint32 server_bit = td->transactionData.server_bit;

  if (((permission & server_bit) == server_bit) &&
      ((sessions & server_bit) == server_bit)) {
    memcpy(td->transactionData.suffix,
           &td->transactionData.number[SUBSCRIBER_NUMBER_LENGTH -
                                       SUBSCRIBER_NUMBER_SUFFIX_LENGTH],
           SUBSCRIBER_NUMBER_SUFFIX_LENGTH);

    DEBUG5("T5(%.*s, %.2d): - Callback 2 - deleting(%.*s)\n",
           SUBSCRIBER_NUMBER_LENGTH, td->transactionData.number,
           td->transactionData.server_id, SUBSCRIBER_NUMBER_SUFFIX_LENGTH,
           td->transactionData.suffix);

    /* Operation 3 */
    NdbOperation *MyOp = pCON->getNdbOperation(SESSION_TABLE);
    CHECK_NULL(MyOp, "T5-3: getNdbOperation", pCON);

    MyOp->deleteTuple();
    MyOp->equal(IND_SESSION_SUBSCRIBER, (char *)td->transactionData.number);
    MyOp->equal(IND_SESSION_SERVER, (char *)&td->transactionData.server_id);
    /* Operation 4 */

    /* Operation 5 */
    MyOp = pCON->getNdbOperation(SERVER_TABLE);
    CHECK_NULL(MyOp, "T5-5: getNdbOperation", pCON);

    MyOp->interpretedUpdateTuple();
    MyOp->equal(IND_SERVER_ID, (char *)&td->transactionData.server_id);
    MyOp->equal(IND_SERVER_SUBSCRIBER_SUFFIX,
                (char *)td->transactionData.suffix);
    MyOp->incValue(IND_SERVER_DELETES, (uint32)1);
    td->transactionData.branchExecuted = 1;
  } else {
    td->transactionData.branchExecuted = 0;

    DEBUG5("T5(%.*s, %.2d): - Callback 2 - no delete - %s %s\n",
           SUBSCRIBER_NUMBER_LENGTH, td->transactionData.number,
           td->transactionData.server_id,
           ((permission & server_bit) ? "permission - " : "no permission - "),
           ((sessions & server_bit) ? "in session - " : "no in session - "));
  }

  if (!td->transactionData.do_rollback && td->transactionData.branchExecuted) {
    pCON->executeAsynchPrepare(Commit, T5_Callback_3, td);
  } else {
    pCON->executeAsynchPrepare(Rollback, T5_Callback_3, td);
  }
}

void T5_Callback_3(int result, NdbConnection *pCON, void *threadData) {
  CHECK_MINUS_ONE(result, "T5-3: Commit", pCON);
  ThreadData *td = (ThreadData *)threadData;

  DEBUG3("T5(%.*s, %.2d): - Completing\n", SUBSCRIBER_NUMBER_LENGTH,
         td->transactionData.number, td->transactionData.server_id);

  td->pNDB->closeTransaction(pCON);
  complete_T5(td);
}
