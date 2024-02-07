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

extern "C" {
#include "user_transaction.h"
};

#include "macros.h"
#include "ndb_error.hpp"
#include "ndb_schema.hpp"

#include <time.h>
#include <NdbApi.hpp>

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
int T1(void *obj, const SubscriberNumber number, const Location new_location,
       const ChangedBy changed_by, const ChangedTime changed_time,
       BenchmarkTime *transaction_time) {
  Ndb *pNDB = (Ndb *)obj;

  BenchmarkTime start;
  get_time(&start);

  int check;
  NdbRecAttr *check2;

  NdbConnection *MyTransaction = pNDB->startTransaction();
  if (MyTransaction == NULL)
    error_handler("T1: startTranscation", pNDB->getNdbErrorString(), 0);

  NdbOperation *MyOperation = MyTransaction->getNdbOperation(SUBSCRIBER_TABLE);
  CHECK_NULL(MyOperation, "T1: getNdbOperation", MyTransaction);

  check = MyOperation->updateTuple();
  CHECK_MINUS_ONE(check, "T1: updateTuple", MyTransaction);

  check = MyOperation->equal(IND_SUBSCRIBER_NUMBER, number);
  CHECK_MINUS_ONE(check, "T1: equal subscriber", MyTransaction);

  check = MyOperation->setValue(IND_SUBSCRIBER_LOCATION, (char *)&new_location);
  CHECK_MINUS_ONE(check, "T1: setValue location", MyTransaction);

  check = MyOperation->setValue(IND_SUBSCRIBER_CHANGED_BY, changed_by);
  CHECK_MINUS_ONE(check, "T1: setValue changed_by", MyTransaction);

  check = MyOperation->setValue(IND_SUBSCRIBER_CHANGED_TIME, changed_time);
  CHECK_MINUS_ONE(check, "T1: setValue changed_time", MyTransaction);

  check = MyTransaction->execute(Commit);
  CHECK_MINUS_ONE(check, "T1: Commit", MyTransaction);

  pNDB->closeTransaction(MyTransaction);

  get_time(transaction_time);
  time_diff(transaction_time, &start);
  return 0;
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
int T2(void *obj, const SubscriberNumber number, Location *readLocation,
       ChangedBy changed_by, ChangedTime changed_time,
       SubscriberName subscriberName, BenchmarkTime *transaction_time) {
  Ndb *pNDB = (Ndb *)obj;

  BenchmarkTime start;
  get_time(&start);

  int check;
  NdbRecAttr *check2;

  NdbConnection *MyTransaction = pNDB->startTransaction();
  if (MyTransaction == NULL)
    error_handler("T2: startTranscation", pNDB->getNdbErrorString(), 0);

  NdbOperation *MyOperation = MyTransaction->getNdbOperation(SUBSCRIBER_TABLE);
  CHECK_NULL(MyOperation, "T2: getNdbOperation", MyTransaction);

  check = MyOperation->readTuple();
  CHECK_MINUS_ONE(check, "T2: readTuple", MyTransaction);

  check = MyOperation->equal(IND_SUBSCRIBER_NUMBER, number);
  CHECK_MINUS_ONE(check, "T2: equal subscriber", MyTransaction);

  check2 = MyOperation->getValue(IND_SUBSCRIBER_LOCATION, (char *)readLocation);
  CHECK_NULL(check2, "T2: getValue location", MyTransaction);

  check2 = MyOperation->getValue(IND_SUBSCRIBER_CHANGED_BY, changed_by);
  CHECK_NULL(check2, "T2: getValue changed_by", MyTransaction);

  check2 = MyOperation->getValue(IND_SUBSCRIBER_CHANGED_TIME, changed_time);
  CHECK_NULL(check2, "T2: getValue changed_time", MyTransaction);

  check2 = MyOperation->getValue(IND_SUBSCRIBER_NAME, subscriberName);
  CHECK_NULL(check2, "T2: getValue name", MyTransaction);

  check = MyTransaction->execute(Commit);
  CHECK_MINUS_ONE(check, "T2: Commit", MyTransaction);

  pNDB->closeTransaction(MyTransaction);

  get_time(transaction_time);
  time_diff(transaction_time, &start);
  return 0;
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
int T3(void *obj, const SubscriberNumber inNumber,
       const SubscriberSuffix inSuffix, const ServerId inServerId,
       const ServerBit inServerBit, SessionDetails outSessionDetails,
       ChangedBy outChangedBy, ChangedTime outChangedTime,
       Location *outLocation, BranchExecuted *outBranchExecuted,
       BenchmarkTime *outTransactionTime) {
  Ndb *pNDB = (Ndb *)obj;

  GroupId groupId;
  ActiveSessions sessions;
  Permission permission;

  BenchmarkTime start;
  get_time(&start);

  int check;
  NdbRecAttr *check2;

  NdbConnection *MyTransaction = pNDB->startTransaction();
  if (MyTransaction == NULL)
    error_handler("T3-1: startTranscation", pNDB->getNdbErrorString(), 0);

  NdbOperation *MyOperation = MyTransaction->getNdbOperation(SUBSCRIBER_TABLE);
  CHECK_NULL(MyOperation, "T3-1: getNdbOperation", MyTransaction);

  check = MyOperation->readTuple();
  CHECK_MINUS_ONE(check, "T3-1: readTuple", MyTransaction);

  check = MyOperation->equal(IND_SUBSCRIBER_NUMBER, inNumber);
  CHECK_MINUS_ONE(check, "T3-1: equal subscriber", MyTransaction);

  check2 = MyOperation->getValue(IND_SUBSCRIBER_LOCATION, (char *)outLocation);
  CHECK_NULL(check2, "T3-1: getValue location", MyTransaction);

  check2 = MyOperation->getValue(IND_SUBSCRIBER_CHANGED_BY, outChangedBy);
  CHECK_NULL(check2, "T3-1: getValue changed_by", MyTransaction);

  check2 = MyOperation->getValue(IND_SUBSCRIBER_CHANGED_TIME, outChangedTime);
  CHECK_NULL(check2, "T3-1: getValue changed_time", MyTransaction);

  check2 = MyOperation->getValue(IND_SUBSCRIBER_GROUP, (char *)&groupId);
  CHECK_NULL(check2, "T3-1: getValue group", MyTransaction);

  check2 = MyOperation->getValue(IND_SUBSCRIBER_SESSIONS, (char *)&sessions);
  CHECK_NULL(check2, "T3-1: getValue sessions", MyTransaction);

  check = MyTransaction->execute(NoCommit);
  CHECK_MINUS_ONE(check, "T3-1: NoCommit", MyTransaction);

  /* Operation 2 */

  MyOperation = MyTransaction->getNdbOperation(GROUP_TABLE);
  CHECK_NULL(MyOperation, "T3-2: getNdbOperation", MyTransaction);

  check = MyOperation->readTuple();
  CHECK_MINUS_ONE(check, "T3-2: readTuple", MyTransaction);

  check = MyOperation->equal(IND_GROUP_ID, (char *)&groupId);
  CHECK_MINUS_ONE(check, "T3-2: equal group", MyTransaction);

  check2 = MyOperation->getValue(IND_GROUP_ALLOW_READ, (char *)&permission);
  CHECK_NULL(check2, "T3-2: getValue allow_read", MyTransaction);

  check = MyTransaction->execute(NoCommit);
  CHECK_MINUS_ONE(check, "T3-2: NoCommit", MyTransaction);

  DEBUG3("T3(%.*s, %.2d): ", SUBSCRIBER_NUMBER_LENGTH, inNumber, inServerId);

  if (((permission & inServerBit) == inServerBit) &&
      ((sessions & inServerBit) == inServerBit)) {
    DEBUG("reading - ");

    /* Operation 3 */

    MyOperation = MyTransaction->getNdbOperation(SESSION_TABLE);
    CHECK_NULL(MyOperation, "T3-3: getNdbOperation", MyTransaction);

    check = MyOperation->readTuple();
    CHECK_MINUS_ONE(check, "T3-3: readTuple", MyTransaction);

    check = MyOperation->equal(IND_SESSION_SUBSCRIBER, (char *)inNumber);
    CHECK_MINUS_ONE(check, "T3-3: equal number", MyTransaction);

    check = MyOperation->equal(IND_SESSION_SERVER, (char *)&inServerId);
    CHECK_MINUS_ONE(check, "T3-3: equal server id", MyTransaction);

    check2 = MyOperation->getValue(IND_SESSION_DATA, (char *)outSessionDetails);
    CHECK_NULL(check2, "T3-3: getValue session details", MyTransaction);

    check = MyTransaction->execute(NoCommit);
    CHECK_MINUS_ONE(check, "T3-3: NoCommit", MyTransaction);

    /* Operation 4 */

    MyOperation = MyTransaction->getNdbOperation(SERVER_TABLE);
    CHECK_NULL(MyOperation, "T3-4: getNdbOperation", MyTransaction);

    check = MyOperation->interpretedUpdateTuple();
    CHECK_MINUS_ONE(check, "T3-4: interpretedUpdateTuple", MyTransaction);

    check = MyOperation->equal(IND_SERVER_ID, (char *)&inServerId);
    CHECK_MINUS_ONE(check, "T3-4: equal serverId", MyTransaction);

    check = MyOperation->equal(IND_SERVER_SUBSCRIBER_SUFFIX, (char *)inSuffix);
    CHECK_MINUS_ONE(check, "T3-4: equal suffix", MyTransaction);

    check = MyOperation->incValue(IND_SERVER_READS, (uint32)1);
    CHECK_MINUS_ONE(check, "T3-4: inc value", MyTransaction);

    check = MyTransaction->execute(NoCommit);
    CHECK_MINUS_ONE(check, "T3-4: NoCommit", MyTransaction);

    (*outBranchExecuted) = 1;
  } else {
    (*outBranchExecuted) = 0;
  }
  DEBUG("commit\n");
  check = MyTransaction->execute(Commit);
  CHECK_MINUS_ONE(check, "T3: Commit", MyTransaction);

  pNDB->closeTransaction(MyTransaction);

  get_time(outTransactionTime);
  time_diff(outTransactionTime, &start);
  return 0;
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
int T4(void *obj, const SubscriberNumber inNumber,
       const SubscriberSuffix inSuffix, const ServerId inServerId,
       const ServerBit inServerBit, const SessionDetails inSessionDetails,
       ChangedBy outChangedBy, ChangedTime outChangedTime,
       Location *outLocation, DoRollback inDoRollback,
       BranchExecuted *outBranchExecuted, BenchmarkTime *outTransactionTime) {
  Ndb *pNDB = (Ndb *)obj;

  GroupId groupId;
  ActiveSessions sessions;
  Permission permission;

  BenchmarkTime start;
  get_time(&start);

  int check;
  NdbRecAttr *check2;

  NdbConnection *MyTransaction = pNDB->startTransaction();
  if (MyTransaction == NULL)
    error_handler("T4-1: startTranscation", pNDB->getNdbErrorString(), 0);

  DEBUG3("T4(%.*s, %.2d): ", SUBSCRIBER_NUMBER_LENGTH, inNumber, inServerId);

  NdbOperation *MyOperation = 0;

  MyOperation = MyTransaction->getNdbOperation(SUBSCRIBER_TABLE);
  CHECK_NULL(MyOperation, "T4-1: getNdbOperation", MyTransaction);

  check = MyOperation->readTupleExclusive();
  CHECK_MINUS_ONE(check, "T4-1: readTuple", MyTransaction);

  check = MyOperation->equal(IND_SUBSCRIBER_NUMBER, inNumber);
  CHECK_MINUS_ONE(check, "T4-1: equal subscriber", MyTransaction);

  check2 = MyOperation->getValue(IND_SUBSCRIBER_LOCATION, (char *)outLocation);
  CHECK_NULL(check2, "T4-1: getValue location", MyTransaction);

  check2 = MyOperation->getValue(IND_SUBSCRIBER_CHANGED_BY, outChangedBy);
  CHECK_NULL(check2, "T4-1: getValue changed_by", MyTransaction);

  check2 = MyOperation->getValue(IND_SUBSCRIBER_CHANGED_TIME, outChangedTime);
  CHECK_NULL(check2, "T4-1: getValue changed_time", MyTransaction);

  check2 = MyOperation->getValue(IND_SUBSCRIBER_GROUP, (char *)&groupId);
  CHECK_NULL(check2, "T4-1: getValue group", MyTransaction);

  check2 = MyOperation->getValue(IND_SUBSCRIBER_SESSIONS, (char *)&sessions);
  CHECK_NULL(check2, "T4-1: getValue sessions", MyTransaction);

  check = MyTransaction->execute(NoCommit);
  CHECK_MINUS_ONE(check, "T4-1: NoCommit", MyTransaction);

  /* Operation 2 */
  MyOperation = MyTransaction->getNdbOperation(GROUP_TABLE);
  CHECK_NULL(MyOperation, "T4-2: getNdbOperation", MyTransaction);

  check = MyOperation->readTuple();
  CHECK_MINUS_ONE(check, "T4-2: readTuple", MyTransaction);

  check = MyOperation->equal(IND_GROUP_ID, (char *)&groupId);
  CHECK_MINUS_ONE(check, "T4-2: equal group", MyTransaction);

  check2 = MyOperation->getValue(IND_GROUP_ALLOW_INSERT, (char *)&permission);
  CHECK_NULL(check2, "T4-2: getValue allow_insert", MyTransaction);

  check = MyTransaction->execute(NoCommit);
  CHECK_MINUS_ONE(check, "T4-2: NoCommit", MyTransaction);

  if (((permission & inServerBit) == inServerBit) &&
      ((sessions & inServerBit) == 0)) {
    DEBUG("inserting - ");

    /* Operation 3 */
    MyOperation = MyTransaction->getNdbOperation(SESSION_TABLE);
    CHECK_NULL(MyOperation, "T4-3: getNdbOperation", MyTransaction);

    check = MyOperation->insertTuple();
    CHECK_MINUS_ONE(check, "T4-3: insertTuple", MyTransaction);

    check = MyOperation->equal(IND_SESSION_SUBSCRIBER, (char *)inNumber);
    CHECK_MINUS_ONE(check, "T4-3: equal number", MyTransaction);

    check = MyOperation->equal(IND_SESSION_SERVER, (char *)&inServerId);
    CHECK_MINUS_ONE(check, "T4-3: equal server id", MyTransaction);

    check = MyOperation->setValue(SESSION_DATA, (char *)inSessionDetails);
    CHECK_MINUS_ONE(check, "T4-3: setValue session details", MyTransaction);

    check = MyTransaction->execute(NoCommit);
    CHECK_MINUS_ONE(check, "T4-3: NoCommit", MyTransaction);

    /* Operation 4 */
    MyOperation = MyTransaction->getNdbOperation(SUBSCRIBER_TABLE);
    CHECK_NULL(MyOperation, "T4-4: getNdbOperation", MyTransaction);

    check = MyOperation->interpretedUpdateTuple();
    CHECK_MINUS_ONE(check, "T4-4: interpretedUpdateTuple", MyTransaction);

    check = MyOperation->equal(IND_SUBSCRIBER_NUMBER, (char *)inNumber);
    CHECK_MINUS_ONE(check, "T4-4: equal number", MyTransaction);

    check = MyOperation->incValue(IND_SUBSCRIBER_SESSIONS, (uint32)inServerBit);
    CHECK_MINUS_ONE(check, "T4-4: inc value", MyTransaction);

    check = MyTransaction->execute(NoCommit);
    CHECK_MINUS_ONE(check, "T4-4: NoCommit", MyTransaction);

    /* Operation 5 */
    MyOperation = MyTransaction->getNdbOperation(SERVER_TABLE);
    CHECK_NULL(MyOperation, "T4-5: getNdbOperation", MyTransaction);

    check = MyOperation->interpretedUpdateTuple();
    CHECK_MINUS_ONE(check, "T4-5: interpretedUpdateTuple", MyTransaction);

    check = MyOperation->equal(IND_SERVER_ID, (char *)&inServerId);
    CHECK_MINUS_ONE(check, "T4-5: equal serverId", MyTransaction);

    check = MyOperation->equal(IND_SERVER_SUBSCRIBER_SUFFIX, (char *)inSuffix);
    CHECK_MINUS_ONE(check, "T4-5: equal suffix", MyTransaction);

    check = MyOperation->incValue(IND_SERVER_INSERTS, (uint32)1);
    CHECK_MINUS_ONE(check, "T4-5: inc value", MyTransaction);

    check = MyTransaction->execute(NoCommit);
    CHECK_MINUS_ONE(check, "T4-5: NoCommit", MyTransaction);

    (*outBranchExecuted) = 1;
  } else {
    DEBUG1("%s",
           ((permission & inServerBit) ? "permission - " : "no permission - "));
    DEBUG1("%s",
           ((sessions & inServerBit) ? "in session - " : "no in session - "));
    (*outBranchExecuted) = 0;
  }

  if (!inDoRollback) {
    DEBUG("commit\n");
    check = MyTransaction->execute(Commit);
    CHECK_MINUS_ONE(check, "T4: Commit", MyTransaction);
  } else {
    DEBUG("rollback\n");
    check = MyTransaction->execute(Rollback);
    CHECK_MINUS_ONE(check, "T4:Rollback", MyTransaction);
  }

  pNDB->closeTransaction(MyTransaction);

  get_time(outTransactionTime);
  time_diff(outTransactionTime, &start);
  return 0;
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
int T5(void *obj, const SubscriberNumber inNumber,
       const SubscriberSuffix inSuffix, const ServerId inServerId,
       const ServerBit inServerBit, ChangedBy outChangedBy,
       ChangedTime outChangedTime, Location *outLocation,
       DoRollback inDoRollback, BranchExecuted *outBranchExecuted,
       BenchmarkTime *outTransactionTime) {
  Ndb *pNDB = (Ndb *)obj;
  NdbConnection *MyTransaction = 0;
  NdbOperation *MyOperation = 0;

  GroupId groupId;
  ActiveSessions sessions;
  Permission permission;

  BenchmarkTime start;
  get_time(&start);

  int check;
  NdbRecAttr *check2;

  MyTransaction = pNDB->startTransaction();
  if (MyTransaction == NULL)
    error_handler("T5-1: startTranscation", pNDB->getNdbErrorString(), 0);

  MyOperation = MyTransaction->getNdbOperation(SUBSCRIBER_TABLE);
  CHECK_NULL(MyOperation, "T5-1: getNdbOperation", MyTransaction);

  check = MyOperation->readTupleExclusive();
  CHECK_MINUS_ONE(check, "T5-1: readTuple", MyTransaction);

  check = MyOperation->equal(IND_SUBSCRIBER_NUMBER, inNumber);
  CHECK_MINUS_ONE(check, "T5-1: equal subscriber", MyTransaction);

  check2 = MyOperation->getValue(IND_SUBSCRIBER_LOCATION, (char *)outLocation);
  CHECK_NULL(check2, "T5-1: getValue location", MyTransaction);

  check2 = MyOperation->getValue(IND_SUBSCRIBER_CHANGED_BY, outChangedBy);
  CHECK_NULL(check2, "T5-1: getValue changed_by", MyTransaction);

  check2 = MyOperation->getValue(IND_SUBSCRIBER_CHANGED_TIME, outChangedTime);
  CHECK_NULL(check2, "T5-1: getValue changed_time", MyTransaction);

  check2 = MyOperation->getValue(IND_SUBSCRIBER_GROUP, (char *)&groupId);
  CHECK_NULL(check2, "T5-1: getValue group", MyTransaction);

  check2 = MyOperation->getValue(IND_SUBSCRIBER_SESSIONS, (char *)&sessions);
  CHECK_NULL(check2, "T5-1: getValue sessions", MyTransaction);

  check = MyTransaction->execute(NoCommit);
  CHECK_MINUS_ONE(check, "T5-1: NoCommit", MyTransaction);

  /* Operation 2 */

  MyOperation = MyTransaction->getNdbOperation(GROUP_TABLE);
  CHECK_NULL(MyOperation, "T5-2: getNdbOperation", MyTransaction);

  check = MyOperation->readTuple();
  CHECK_MINUS_ONE(check, "T5-2: readTuple", MyTransaction);

  check = MyOperation->equal(IND_GROUP_ID, (char *)&groupId);
  CHECK_MINUS_ONE(check, "T5-2: equal group", MyTransaction);

  check2 = MyOperation->getValue(IND_GROUP_ALLOW_DELETE, (char *)&permission);
  CHECK_NULL(check2, "T5-2: getValue allow_delete", MyTransaction);

  check = MyTransaction->execute(NoCommit);
  CHECK_MINUS_ONE(check, "T5-2: NoCommit", MyTransaction);

  DEBUG3("T5(%.*s, %.2d): ", SUBSCRIBER_NUMBER_LENGTH, inNumber, inServerId);

  if (((permission & inServerBit) == inServerBit) &&
      ((sessions & inServerBit) == inServerBit)) {
    DEBUG("deleting - ");

    /* Operation 3 */
    MyOperation = MyTransaction->getNdbOperation(SESSION_TABLE);
    CHECK_NULL(MyOperation, "T5-3: getNdbOperation", MyTransaction);

    check = MyOperation->deleteTuple();
    CHECK_MINUS_ONE(check, "T5-3: deleteTuple", MyTransaction);

    check = MyOperation->equal(IND_SESSION_SUBSCRIBER, (char *)inNumber);
    CHECK_MINUS_ONE(check, "T5-3: equal number", MyTransaction);

    check = MyOperation->equal(IND_SESSION_SERVER, (char *)&inServerId);
    CHECK_MINUS_ONE(check, "T5-3: equal server id", MyTransaction);

    check = MyTransaction->execute(NoCommit);
    CHECK_MINUS_ONE(check, "T5-3: NoCommit", MyTransaction);

    /* Operation 4 */
    MyOperation = MyTransaction->getNdbOperation(SUBSCRIBER_TABLE);
    CHECK_NULL(MyOperation, "T5-4: getNdbOperation", MyTransaction);

    check = MyOperation->interpretedUpdateTuple();
    CHECK_MINUS_ONE(check, "T5-4: interpretedUpdateTuple", MyTransaction);

    check = MyOperation->equal(IND_SUBSCRIBER_NUMBER, (char *)inNumber);
    CHECK_MINUS_ONE(check, "T5-4: equal number", MyTransaction);

    check = MyOperation->subValue(IND_SUBSCRIBER_SESSIONS, (uint32)inServerBit);
    CHECK_MINUS_ONE(check, "T5-4: dec value", MyTransaction);

    check = MyTransaction->execute(NoCommit);
    CHECK_MINUS_ONE(check, "T5-4: NoCommit", MyTransaction);

    /* Operation 5 */
    MyOperation = MyTransaction->getNdbOperation(SERVER_TABLE);
    CHECK_NULL(MyOperation, "T5-5: getNdbOperation", MyTransaction);

    check = MyOperation->interpretedUpdateTuple();
    CHECK_MINUS_ONE(check, "T5-5: interpretedUpdateTuple", MyTransaction);

    check = MyOperation->equal(IND_SERVER_ID, (char *)&inServerId);
    CHECK_MINUS_ONE(check, "T5-5: equal serverId", MyTransaction);

    check = MyOperation->equal(IND_SERVER_SUBSCRIBER_SUFFIX, (char *)inSuffix);
    CHECK_MINUS_ONE(check, "T5-5: equal suffix", MyTransaction);

    check = MyOperation->incValue(IND_SERVER_DELETES, (uint32)1);
    CHECK_MINUS_ONE(check, "T5-5: inc value", MyTransaction);

    check = MyTransaction->execute(NoCommit);
    CHECK_MINUS_ONE(check, "T5-5: NoCommit", MyTransaction);

    (*outBranchExecuted) = 1;
  } else {
    DEBUG1("%s",
           ((permission & inServerBit) ? "permission - " : "no permission - "));
    DEBUG1("%s",
           ((sessions & inServerBit) ? "in session - " : "no in session - "));
    (*outBranchExecuted) = 0;
  }

  if (!inDoRollback) {
    DEBUG("commit\n");
    check = MyTransaction->execute(Commit);
    CHECK_MINUS_ONE(check, "T5: Commit", MyTransaction);
  } else {
    DEBUG("rollback\n");
    check = MyTransaction->execute(Rollback);
    CHECK_MINUS_ONE(check, "T5:Rollback", MyTransaction);
  }

  pNDB->closeTransaction(MyTransaction);

  get_time(outTransactionTime);
  time_diff(outTransactionTime, &start);
  return 0;
}
