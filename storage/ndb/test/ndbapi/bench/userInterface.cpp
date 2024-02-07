/*
   Copyright (c) 2005, 2024, Oracle and/or its affiliates.

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

/***************************************************************
 * I N C L U D E D   F I L E S                                  *
 ***************************************************************/

#include <ndb_global.h>
#ifndef _WIN32
#include <sys/time.h>
#endif

#include <NdbMutex.h>
#include <NdbSleep.h>
#include <NdbThread.h>
#include <NdbTick.h>
#include <NDBT.hpp>
#include <NdbSchemaCon.hpp>
#include "ndb_error.hpp"
#include "ndb_schema.hpp"
#include "userInterface.h"

/***************************************************************
 * L O C A L   C O N S T A N T S                                *
 ***************************************************************/

/***************************************************************
 * L O C A L   D A T A   S T R U C T U R E S                    *
 ***************************************************************/

/***************************************************************
 * L O C A L   F U N C T I O N S                                *
 ***************************************************************/

extern int localDbPrepare(UserHandle *uh);

/***************************************************************
 * L O C A L   D A T A                                          *
 ***************************************************************/

/***************************************************************
 * P U B L I C   D A T A                                        *
 ***************************************************************/

/***************************************************************
****************************************************************
* L O C A L   F U N C T I O N S   C O D E   S E C T I O N      *
****************************************************************
***************************************************************/

/***************************************************************
****************************************************************
* P U B L I C   F U N C T I O N S   C O D E   S E C T I O N    *
****************************************************************
***************************************************************/

/*-----------------------------------*/
/* Time related Functions            */
/*                                   */
/* Returns a double value in seconds */
/*-----------------------------------*/
static NDB_TICKS initTicks;
double userGetTimeSync(void) {
  double timeValue = 0;

  if (!NdbTick_IsValid(initTicks)) {
    initTicks = NdbTick_getCurrentTicks();
    timeValue = 0.0;
  } else {
    const NDB_TICKS now = NdbTick_getCurrentTicks();
    const Uint64 elapsedMicro = NdbTick_Elapsed(initTicks, now).microSec();

    timeValue = ((double)elapsedMicro) / 1000000.0;
  }
  return timeValue;
}

// 0 - OK
// 1 - Retry transaction
// 2 - Permanent
int userDbCommit(UserHandle *uh) {
  if (uh->pCurrTrans != 0) {
    /* int check = */ uh->pCurrTrans->execute(Commit);
    NdbError err = uh->pCurrTrans->getNdbError();
    uh->pNDB->closeTransaction(uh->pCurrTrans);
    uh->pCurrTrans = 0;

    if (err.status != NdbError::Success) ndbout << err << endl;

    if (err.status == NdbError::TemporaryError &&
        err.classification == NdbError::OverloadError) {
      NdbSleep_SecSleep(3);
    }

    return err.status;
  }
  return 2;
}

/**
 * true - Normal table
 * false - Table w.o. checkpointing and logging
 */
extern int useTableLogging;

int create_table_server(Ndb *pNdb) {
  int check;
  NdbSchemaCon *MySchemaTransaction = NdbSchemaCon::startSchemaTrans(pNdb);
  if (MySchemaTransaction == NULL)
    error_handler("startSchemaTransaction", pNdb->getNdbError(), 0);

  NdbSchemaOp *MySchemaOp = MySchemaTransaction->getNdbSchemaOp();
  if (MySchemaOp == NULL)
    error_handler("getNdbSchemaOp", MySchemaTransaction->getNdbError(), 0);

  // Create table
  check =
      MySchemaOp->createTable(SERVER_TABLE,
                              8,         // Table size
                              TupleKey,  // Key Type
                              1          // Nr of Pages
                              ,
                              DistributionGroup, 6, 78, 80, 1, useTableLogging);
  if (check == -1)
    error_handler("createTable", MySchemaTransaction->getNdbError(), 0);

  check = MySchemaOp->createAttribute(
      SERVER_SUBSCRIBER_SUFFIX, TupleKey, sizeof(char) << 3,
      SUBSCRIBER_NUMBER_SUFFIX_LENGTH, NdbSchemaOp::String, MMBased,
      NotNullAttribute, 0, 0, 1, 16);
  if (check == -1)
    error_handler("createAttribute (subscriber suffix)",
                  MySchemaTransaction->getNdbError(), 0);

  // Create first column, primary key
  check = MySchemaOp->createAttribute(
      SERVER_ID, TupleKey, sizeof(ServerId) << 3, 1, NdbSchemaOp::UnSigned,
      MMBased, NotNullAttribute);
  if (check == -1)
    error_handler("createAttribute (serverid)",
                  MySchemaTransaction->getNdbError(), 0);

  check = MySchemaOp->createAttribute(SERVER_NAME, NoKey, sizeof(char) << 3,
                                      SERVER_NAME_LENGTH, NdbSchemaOp::String,
                                      MMBased, NotNullAttribute);
  if (check == -1)
    error_handler("createAttribute (server name)",
                  MySchemaTransaction->getNdbError(), 0);

  check = MySchemaOp->createAttribute(SERVER_READS, NoKey, sizeof(Counter) << 3,
                                      1, NdbSchemaOp::UnSigned, MMBased,
                                      NotNullAttribute);
  if (check == -1)
    error_handler("createAttribute (server reads)",
                  MySchemaTransaction->getNdbError(), 0);

  check = MySchemaOp->createAttribute(
      SERVER_INSERTS, NoKey, sizeof(Counter) << 3, 1, NdbSchemaOp::UnSigned,
      MMBased, NotNullAttribute);
  if (check == -1)
    error_handler("createAttribute (server inserts)",
                  MySchemaTransaction->getNdbError(), 0);

  check = MySchemaOp->createAttribute(
      SERVER_DELETES, NoKey, sizeof(Counter) << 3, 1, NdbSchemaOp::UnSigned,
      MMBased, NotNullAttribute);
  if (check == -1)
    error_handler("createAttribute (server deletes)",
                  MySchemaTransaction->getNdbError(), 0);

  if (MySchemaTransaction->execute() == -1) {
    error_handler("schemaTransaction->execute()",
                  MySchemaTransaction->getNdbError(), 0);
  }
  NdbSchemaCon::closeSchemaTrans(MySchemaTransaction);
  return 0;
}

int create_table_group(Ndb *pNdb) {
  int check;

  NdbSchemaCon *MySchemaTransaction = NdbSchemaCon::startSchemaTrans(pNdb);
  if (MySchemaTransaction == NULL)
    error_handler("startSchemaTransaction", pNdb->getNdbError(), 0);

  NdbSchemaOp *MySchemaOp = MySchemaTransaction->getNdbSchemaOp();
  if (MySchemaOp == NULL)
    error_handler("getNdbSchemaOp", MySchemaTransaction->getNdbError(), 0);

  // Create table
  check = MySchemaOp->createTable(GROUP_TABLE,
                                  8,         // Table size
                                  TupleKey,  // Key Type
                                  1          // Nr of Pages
                                  ,
                                  All, 6, 78, 80, 1, useTableLogging);

  if (check == -1)
    error_handler("createTable", MySchemaTransaction->getNdbError(), 0);

  // Create first column, primary key
  check = MySchemaOp->createAttribute(GROUP_ID, TupleKey, sizeof(GroupId) << 3,
                                      1, NdbSchemaOp::UnSigned, MMBased,
                                      NotNullAttribute);
  if (check == -1)
    error_handler("createAttribute (group id)",
                  MySchemaTransaction->getNdbError(), 0);

  check = MySchemaOp->createAttribute(NDB_GROUP_NAME, NoKey, sizeof(char) << 3,
                                      GROUP_NAME_LENGTH, NdbSchemaOp::String,
                                      MMBased, NotNullAttribute);
  if (check == -1)
    error_handler("createAttribute (group name)",
                  MySchemaTransaction->getNdbError(), 0);

  check = MySchemaOp->createAttribute(
      GROUP_ALLOW_READ, NoKey, sizeof(Permission) << 3, 1, NdbSchemaOp::String,
      MMBased, NotNullAttribute);
  if (check == -1)
    error_handler("createAttribute (group read)",
                  MySchemaTransaction->getNdbError(), 0);

  check = MySchemaOp->createAttribute(
      GROUP_ALLOW_INSERT, NoKey, sizeof(Permission) << 3, 1,
      NdbSchemaOp::UnSigned, MMBased, NotNullAttribute);
  if (check == -1)
    error_handler("createAttribute (group insert)",
                  MySchemaTransaction->getNdbError(), 0);

  check = MySchemaOp->createAttribute(
      GROUP_ALLOW_DELETE, NoKey, sizeof(Permission) << 3, 1,
      NdbSchemaOp::UnSigned, MMBased, NotNullAttribute);
  if (check == -1)
    error_handler("createAttribute (group delete)",
                  MySchemaTransaction->getNdbError(), 0);

  if (MySchemaTransaction->execute() == -1) {
    error_handler("schemaTransaction->execute()",
                  MySchemaTransaction->getNdbError(), 0);
  }
  NdbSchemaCon::closeSchemaTrans(MySchemaTransaction);
  return 0;
}

int create_table_subscriber(Ndb *pNdb) {
  int check;
  NdbSchemaCon *MySchemaTransaction = NdbSchemaCon::startSchemaTrans(pNdb);
  if (MySchemaTransaction == NULL)
    error_handler("startSchemaTransaction", pNdb->getNdbError(), 0);

  NdbSchemaOp *MySchemaOp = MySchemaTransaction->getNdbSchemaOp();
  if (MySchemaOp == NULL)
    error_handler("getNdbSchemaOp", MySchemaTransaction->getNdbError(), 0);

  // Create table
  check =
      MySchemaOp->createTable(SUBSCRIBER_TABLE,
                              8,         // Table size
                              TupleKey,  // Key Type
                              1          // Nr of Pages
                              ,
                              DistributionGroup, 6, 78, 80, 1, useTableLogging);
  if (check == -1)
    error_handler("createTable", MySchemaTransaction->getNdbError(), 0);

  // Create first column, primary key
  check = MySchemaOp->createAttribute(
      SUBSCRIBER_NUMBER, TupleKey, sizeof(char) << 3, SUBSCRIBER_NUMBER_LENGTH,
      NdbSchemaOp::String, MMBased, NotNullAttribute, 0, 0, 1, 16);
  if (check == -1)
    error_handler("createAttribute (subscriber number)",
                  MySchemaTransaction->getNdbError(), 0);

  check = MySchemaOp->createAttribute(
      SUBSCRIBER_NAME, NoKey, sizeof(char) << 3, SUBSCRIBER_NAME_LENGTH,
      NdbSchemaOp::String, MMBased, NotNullAttribute);
  if (check == -1)
    error_handler("createAttribute (subscriber name)",
                  MySchemaTransaction->getNdbError(), 0);

  check = MySchemaOp->createAttribute(
      SUBSCRIBER_GROUP, NoKey, sizeof(GroupId) << 3, 1, NdbSchemaOp::UnSigned,
      MMBased, NotNullAttribute);
  if (check == -1)
    error_handler("createAttribute (subscriber_group)",
                  MySchemaTransaction->getNdbError(), 0);

  check = MySchemaOp->createAttribute(
      SUBSCRIBER_LOCATION, NoKey, sizeof(Location) << 3, 1,
      NdbSchemaOp::UnSigned, MMBased, NotNullAttribute);
  if (check == -1)
    error_handler("createAttribute (server reads)",
                  MySchemaTransaction->getNdbError(), 0);

  check = MySchemaOp->createAttribute(
      SUBSCRIBER_SESSIONS, NoKey, sizeof(ActiveSessions) << 3, 1,
      NdbSchemaOp::UnSigned, MMBased, NotNullAttribute);
  if (check == -1)
    error_handler("createAttribute (subscriber_sessions)",
                  MySchemaTransaction->getNdbError(), 0);

  check = MySchemaOp->createAttribute(
      SUBSCRIBER_CHANGED_BY, NoKey, sizeof(char) << 3, CHANGED_BY_LENGTH,
      NdbSchemaOp::String, MMBased, NotNullAttribute);
  if (check == -1)
    error_handler("createAttribute (subscriber_changed_by)",
                  MySchemaTransaction->getNdbError(), 0);

  check = MySchemaOp->createAttribute(
      SUBSCRIBER_CHANGED_TIME, NoKey, sizeof(char) << 3, CHANGED_TIME_LENGTH,
      NdbSchemaOp::String, MMBased, NotNullAttribute);
  if (check == -1)
    error_handler("createAttribute (subscriber_changed_time)",
                  MySchemaTransaction->getNdbError(), 0);

  if (MySchemaTransaction->execute() == -1) {
    error_handler("schemaTransaction->execute()",
                  MySchemaTransaction->getNdbError(), 0);
  }
  NdbSchemaCon::closeSchemaTrans(MySchemaTransaction);
  return 0;
}

int create_table_session(Ndb *pNdb) {
  int check;
  NdbSchemaCon *MySchemaTransaction = NdbSchemaCon::startSchemaTrans(pNdb);
  if (MySchemaTransaction == NULL)
    error_handler("startSchemaTransaction", pNdb->getNdbError(), 0);

  NdbSchemaOp *MySchemaOp = MySchemaTransaction->getNdbSchemaOp();
  if (MySchemaOp == NULL)
    error_handler("getNdbSchemaOp", MySchemaTransaction->getNdbError(), 0);

  // Create table
  check =
      MySchemaOp->createTable(SESSION_TABLE,
                              8,         // Table size
                              TupleKey,  // Key Type
                              1          // Nr of Pages
                              ,
                              DistributionGroup, 6, 78, 80, 1, useTableLogging);
  if (check == -1)
    error_handler("createTable", MySchemaTransaction->getNdbError(), 0);

  check = MySchemaOp->createAttribute(
      SESSION_SUBSCRIBER, TupleKey, sizeof(char) << 3, SUBSCRIBER_NUMBER_LENGTH,
      NdbSchemaOp::String, MMBased, NotNullAttribute, 0, 0, 1, 16);
  if (check == -1)
    error_handler("createAttribute (session_subscriber)",
                  MySchemaTransaction->getNdbError(), 0);

  // Create first column, primary key
  check = MySchemaOp->createAttribute(
      SESSION_SERVER, TupleKey, sizeof(ServerId) << 3, 1, NdbSchemaOp::UnSigned,
      MMBased, NotNullAttribute);
  if (check == -1)
    error_handler("createAttribute (session_server)",
                  MySchemaTransaction->getNdbError(), 0);

  check = MySchemaOp->createAttribute(
      SESSION_DATA, NoKey, sizeof(char) << 3, SESSION_DETAILS_LENGTH,
      NdbSchemaOp::String, MMBased, NotNullAttribute);
  if (check == -1)
    error_handler("createAttribute (session_data)",
                  MySchemaTransaction->getNdbError(), 0);

  if (MySchemaTransaction->execute() == -1) {
    error_handler("schemaTransaction->execute()",
                  MySchemaTransaction->getNdbError(), 0);
  }
  NdbSchemaCon::closeSchemaTrans(MySchemaTransaction);
  return 0;
}

void create_table(const char *name, int (*function)(Ndb *pNdb), Ndb *pNdb) {
  printf("creating table %s...", name);
  if (pNdb->getDictionary()->getTable(name) != 0) {
    printf(" it already exists\n");
    return;
  } else {
    printf("\n");
  }
  function(pNdb);
  printf("creating table %s... done\n", name);
}

static int dbCreate(Ndb *pNdb) {
  create_table(SUBSCRIBER_TABLE, create_table_subscriber, pNdb);
  create_table(GROUP_TABLE, create_table_group, pNdb);
  create_table(SESSION_TABLE, create_table_session, pNdb);
  create_table(SERVER_TABLE, create_table_server, pNdb);
  return 0;
}

#ifndef _WIN32
#include <unistd.h>
#endif

UserHandle *userDbConnect(uint32 createDb, const char *dbName) {
  Ndb_cluster_connection *con = new Ndb_cluster_connection();
  con->configure_tls(opt_tls_search_path, opt_mgm_tls);
  if (con->connect(12, 5, 1) != 0) {
    ndbout << "Unable to connect to management server." << endl;
    return 0;
  }
  if (con->wait_until_ready(30, 0) < 0) {
    ndbout << "Cluster nodes not ready in 30 seconds." << endl;
    return 0;
  }

  Ndb *pNdb = new Ndb(con, dbName);

  // printf("Initializing...\n");
  pNdb->init();

  // printf("Waiting...");
  while (pNdb->waitUntilReady() != 0) {
    // printf("...");
  }
  //  printf("done\n");

  if (createDb) dbCreate(pNdb);

  UserHandle *uh = new UserHandle;
  uh->pNCC = con;
  uh->pNDB = pNdb;
  uh->pCurrTrans = 0;

  return uh;
}

void userDbDisconnect(UserHandle *uh) { delete uh; }

int userDbInsertServer(UserHandle *uh, ServerId serverId,
                       SubscriberSuffix suffix, ServerName name) {
  int check;

  uint32 noOfRead = 0;
  uint32 noOfInsert = 0;
  uint32 noOfDelete = 0;

  NdbConnection *MyTransaction = 0;
  if (uh->pCurrTrans != 0) {
    MyTransaction = uh->pCurrTrans;
  } else {
    uh->pCurrTrans = MyTransaction = uh->pNDB->startTransaction();
  }
  if (MyTransaction == NULL)
    error_handler("startTranscation", uh->pNDB->getNdbError(), 0);

  NdbOperation *MyOperation = MyTransaction->getNdbOperation(SERVER_TABLE);
  CHECK_NULL(MyOperation, "getNdbOperation", MyTransaction);

  check = MyOperation->insertTuple();
  CHECK_MINUS_ONE(check, "insert tuple", MyTransaction);

  check = MyOperation->equal(SERVER_ID, (char *)&serverId);
  CHECK_MINUS_ONE(check, "setValue id", MyTransaction);

  check = MyOperation->setValue(SERVER_SUBSCRIBER_SUFFIX, suffix);
  CHECK_MINUS_ONE(check, "setValue suffix", MyTransaction);

  check = MyOperation->setValue(SERVER_NAME, name);
  CHECK_MINUS_ONE(check, "setValue name", MyTransaction);

  check = MyOperation->setValue(SERVER_READS, (char *)&noOfRead);
  CHECK_MINUS_ONE(check, "setValue reads", MyTransaction);

  check = MyOperation->setValue(SERVER_INSERTS, (char *)&noOfInsert);
  CHECK_MINUS_ONE(check, "setValue inserts", MyTransaction);

  check = MyOperation->setValue(SERVER_DELETES, (char *)&noOfDelete);
  CHECK_MINUS_ONE(check, "setValue deletes", MyTransaction);

  return 0;
}

int userDbInsertSubscriber(UserHandle *uh, SubscriberNumber number,
                           uint32 groupId, SubscriberName name) {
  int check;
  uint32 activeSessions = 0;
  Location l = 0;
  ChangedBy changedBy;
  ChangedTime changedTime;
  BaseString::snprintf(changedBy, sizeof(changedBy), "ChangedBy");
  BaseString::snprintf(changedTime, sizeof(changedTime), "ChangedTime");

  NdbConnection *MyTransaction = 0;
  if (uh->pCurrTrans != 0) {
    MyTransaction = uh->pCurrTrans;
  } else {
    uh->pCurrTrans = MyTransaction = uh->pNDB->startTransaction();
  }
  if (MyTransaction == NULL)
    error_handler("startTranscation", uh->pNDB->getNdbError(), 0);

  NdbOperation *MyOperation = MyTransaction->getNdbOperation(SUBSCRIBER_TABLE);
  CHECK_NULL(MyOperation, "getNdbOperation", MyTransaction);

  check = MyOperation->insertTuple();
  CHECK_MINUS_ONE(check, "insertTuple", MyTransaction);

  check = MyOperation->equal(SUBSCRIBER_NUMBER, number);
  CHECK_MINUS_ONE(check, "equal", MyTransaction);

  check = MyOperation->setValue(SUBSCRIBER_NAME, name);
  CHECK_MINUS_ONE(check, "setValue name", MyTransaction);

  check = MyOperation->setValue(SUBSCRIBER_GROUP, (char *)&groupId);
  CHECK_MINUS_ONE(check, "setValue group", MyTransaction);

  check = MyOperation->setValue(SUBSCRIBER_LOCATION, (char *)&l);
  CHECK_MINUS_ONE(check, "setValue location", MyTransaction);

  check = MyOperation->setValue(SUBSCRIBER_SESSIONS, (char *)&activeSessions);
  CHECK_MINUS_ONE(check, "setValue sessions", MyTransaction);

  check = MyOperation->setValue(SUBSCRIBER_CHANGED_BY, changedBy);
  CHECK_MINUS_ONE(check, "setValue changedBy", MyTransaction);

  check = MyOperation->setValue(SUBSCRIBER_CHANGED_TIME, changedTime);
  CHECK_MINUS_ONE(check, "setValue changedTime", MyTransaction);

  return 0;
}

int userDbInsertGroup(UserHandle *uh, GroupId groupId, GroupName name,
                      Permission allowRead, Permission allowInsert,
                      Permission allowDelete) {
  int check;

  NdbConnection *MyTransaction = 0;
  if (uh->pCurrTrans != 0) {
    MyTransaction = uh->pCurrTrans;
  } else {
    uh->pCurrTrans = MyTransaction = uh->pNDB->startTransaction();
  }
  if (MyTransaction == NULL)
    error_handler("startTranscation", uh->pNDB->getNdbError(), 0);

  NdbOperation *MyOperation = MyTransaction->getNdbOperation(GROUP_TABLE);
  CHECK_NULL(MyOperation, "getNdbOperation", MyTransaction);

  check = MyOperation->insertTuple();
  CHECK_MINUS_ONE(check, "insertTuple", MyTransaction);

  check = MyOperation->equal(GROUP_ID, (char *)&groupId);
  CHECK_MINUS_ONE(check, "equal", MyTransaction);

  check = MyOperation->setValue(NDB_GROUP_NAME, name);
  CHECK_MINUS_ONE(check, "setValue name", MyTransaction);

  check = MyOperation->setValue(GROUP_ALLOW_READ, (char *)&allowRead);
  CHECK_MINUS_ONE(check, "setValue allowRead", MyTransaction);

  check = MyOperation->setValue(GROUP_ALLOW_INSERT, (char *)&allowInsert);
  CHECK_MINUS_ONE(check, "setValue allowInsert", MyTransaction);

  check = MyOperation->setValue(GROUP_ALLOW_DELETE, (char *)&allowDelete);
  CHECK_MINUS_ONE(check, "setValue allowDelete", MyTransaction);

  return 0;
}
