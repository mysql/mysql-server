/*
 Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#include <HugoTransactions.hpp>
#include <NDBT_Test.hpp>
#include <NdbRestarter.hpp>
#include <SqlClient.hpp>

/**
 * Drop table at NdbApi level
 */
static int dropTableApi(NDBT_Context *ctx, NDBT_Step *step) {
  const NdbDictionary::Table *table = ctx->getTab();

  GETNDB(step)->getDictionary()->dropTable(table->getName());
  return NDBT_OK;
}

/**
 * Create database at SQL level
 */
static int createDatabaseSql(NDBT_Context *ctx, NDBT_Step *step) {
  /* Create a client for talking to MySQLD */
  SqlClient sqlClient;

  if (!sqlClient.waitConnected()) {
    ndbout << "Failed to connect to SQL" << endl;
    return NDBT_FAILED;
  }

  ndbout << "Connected to MySQLD" << endl;

  if (!sqlClient.doQuery("DROP DATABASE IF EXISTS TEST_DB")) {
    ndbout << "Failed to drop DB" << endl;
    return NDBT_FAILED;
  }

  if (!sqlClient.doQuery("CREATE DATABASE TEST_DB")) {
    ndbout << "Failed to create DB" << endl;
    return NDBT_FAILED;
  }
  ndbout << "Database TEST_DB created" << endl;

  return NDBT_OK;
}

/**
 * Create table at SQL level
 */
static int createT1Sql(NDBT_Context *ctx, NDBT_Step *step) {
  /* Create a client for talking to MySQLD */
  SqlClient sqlClient;

  if (!sqlClient.waitConnected()) {
    ndbout << "Failed to connect to SQL" << endl;
    return NDBT_FAILED;
  }

  ndbout << "Connected to MySQLD" << endl;

  if (!sqlClient.doQuery("CREATE TABLE TEST_DB.T1 (a int unsigned primary key,"
                         "b int unsigned,"
                         "c int unsigned,"
                         "d int unsigned,"
                         "e varbinary(100))"
                         " engine=ndb")) {
    ndbout << "Failed to create table" << endl;
    return NDBT_FAILED;
  }

  ndbout << "T1 created via SQL" << endl;

  return NDBT_OK;
}

/**
 * Drop database via SQL
 */
static int dropT1Sql(NDBT_Context *ctx, NDBT_Step *step) {
  /* Create a client for talking to MySQLD 1 */
  SqlClient sqlClient("TEST_DB");

  if (!sqlClient.waitConnected()) {
    ndbout << "Failed to connect to SQL" << endl;
    return NDBT_FAILED;
  }

  ndbout << "Connected to MySQLD" << endl;

  sqlClient.doQuery("DROP DATABASE IF EXISTS TEST_DB");

  ndbout << "TEST_DB dropped via SQL" << endl;
  return NDBT_OK;
}

/**
 * Refresh NDBT NdbApi table object
 */
static int refreshT1Ctx(NDBT_Context *ctx, NDBT_Step *step) {
  Ndb *pNdb = GETNDB(step);
  NdbDictionary::Dictionary *pDict = pNdb->getDictionary();

  pDict->invalidateTable("T1");

  const NdbDictionary::Table *tab = pDict->getTable("T1");
  if (!tab) {
    ndbout << "Failed to get table, error " << pDict->getNdbError() << endl;
    return NDBT_FAILED;
  }

  ctx->setTab(tab);
  return NDBT_OK;
}

static int setupT1Sql(NDBT_Context *ctx, NDBT_Step *step) {
  if ((dropTableApi(ctx, step) != NDBT_OK) ||
      (createDatabaseSql(ctx, step) != NDBT_OK) ||
      (createT1Sql(ctx, step) != NDBT_OK) ||
      (refreshT1Ctx(ctx, step) != NDBT_OK)) {
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

static int runLoad(NDBT_Context *ctx, NDBT_Step *step) {
  HugoTransactions hugoTrans(*ctx->getTab());
  int records = ctx->getNumRecords();

  ndbout << "Clearing" << endl;
  hugoTrans.clearTable(GETNDB(step), 0);

  ndbout << "Loading" << endl;
  if (hugoTrans.loadTable(GETNDB(step), records, 1, true, 1) != 0) {
    g_err << "FAIL " << __LINE__ << endl;
    return NDBT_FAILED;
  }

  return NDBT_OK;
}

static int runUpdates(NDBT_Context *ctx, NDBT_Step *step) {
  HugoTransactions hugoTrans(*ctx->getTab());
  hugoTrans.setThrInfo(step->getStepTypeCount(), step->getStepTypeNo());
  const int records = ctx->getNumRecords();

  ndbout << "runUpdates " << step->getStepTypeNo() << "/"
         << step->getStepTypeCount() << endl;

  while (!ctx->isTestStopped()) {
    if (hugoTrans.pkUpdateRecords(GETNDB(step), records, 10, 1) != 0) {
      g_err << "FAIL " << __LINE__ << endl;
      return NDBT_FAILED;
    }
  }

  return NDBT_OK;
}

static int runNodeRestarts(NDBT_Context *ctx, NDBT_Step *step) {
  NdbRestarter restarter;
  Uint32 count = 0;
  Uint32 numberNodeRestarts = ctx->getProperty("NodeRestartCount", Uint32(3));
  ndbout << "Restarting random data nodes " << numberNodeRestarts << " times."
         << endl;
  while ((!ctx->isTestStopped()) && (count < numberNodeRestarts)) {
    const int nodeId = restarter.getNode(NdbRestarter::NS_RANDOM);
    const bool abort = (((count++) % 2) == 1);
    ndbout << "Restarting data node " << nodeId << (abort ? " with abort" : "")
           << endl;

    if (restarter.restartOneDbNode(nodeId, false, /* initial */
                                   true,          /* nostart */
                                   abort)         /* abort   */
        != NDBT_OK) {
      ndbout << "Failed to restart node" << endl;
      return NDBT_FAILED;
    }

    if (restarter.waitNodesNoStart(&nodeId, 1) != NDBT_OK) {
      ndbout << "Failed waiting for NOT-STARTED" << endl;
      return NDBT_FAILED;
    }

    if (restarter.startNodes(&nodeId, 1) != NDBT_OK) {
      ndbout << "Failed to request start" << endl;
      return NDBT_FAILED;
    }

    if (restarter.waitNodesStarted(&nodeId, 1) != NDBT_OK) {
      ndbout << "Failed waiting for nodes to start" << endl;
      return NDBT_FAILED;
    }
  }
  ctx->stopTest();
  return NDBT_OK;
}

static int runMySQLDDisconnects(NDBT_Context *ctx, NDBT_Step *step) {
  int mysqldNodeid = 0;

  {
    /* Create a client for talking to MySQLD 1 */
    SqlClient sqlClient("TEST_DB");

    if (!sqlClient.waitConnected()) {
      ndbout << "Failed to connect to SQL" << endl;
      return NDBT_FAILED;
    }

    /* Determine MySQLD nodeid */
    {
      SqlResultSet rs;

      if (!sqlClient.doQuery("SELECT node_id from ndbinfo.processes "
                             "where process_name=\"mysqld\" "
                             "and service_URI LIKE \'%server-id=1\'",
                             rs)) {
        ndbout << "Failed to execute NdbInfo query" << endl;
        return NDBT_FAILED;
      }

      if (rs.numRows() != 1) {
        ndbout << "Incorrect number of rows : " << rs.numRows() << endl;
        return NDBT_FAILED;
      }

      mysqldNodeid = rs.columnAsInt("node_id");
    }
  }

  ndbout << "MySQLD node id is " << mysqldNodeid << endl;

  NdbRestarter restarter;
  const int dumpArgs[] = {900, mysqldNodeid};
  while (!ctx->isTestStopped()) {
    ndbout << "Disconnecting MySQLD" << endl;
    if (restarter.dumpStateAllNodes(dumpArgs, 2) != NDBT_OK) {
      return NDBT_FAILED;
    }

    ndbout << "Waiting" << endl;
    NdbSleep_SecSleep(20);
  }

  return NDBT_OK;
}

static int limitRuntime(NDBT_Context *ctx, NDBT_Step *step) {
  const Uint32 limit = ctx->getProperty("TestRuntimeLimitSeconds", Uint32(120));
  ndbout << "Limiting test runtime to " << limit << " seconds" << endl;

  const NDB_TICKS start = NdbTick_getCurrentTicks();

  while (!ctx->isTestStopped() &&
         NdbTick_Elapsed(start, NdbTick_getCurrentTicks()).seconds() < limit) {
    NdbSleep_SecSleep(1);
  }

  ndbout << "Test run for long enough, finishing." << endl;

  ctx->stopTest();
  return NDBT_OK;
}

static int setEventBufferMaxImpl(NDBT_Context *ctx, NDBT_Step *step,
                                 const Uint32 maxBytes) {
  /* Create a client for talking to MySQLD 1 */
  SqlClient sqlClient("TEST_DB");

  if (!sqlClient.waitConnected()) {
    ndbout << "Failed to connect to SQL" << endl;
    return NDBT_FAILED;
  }

  BaseString stmt;
  stmt.assfmt("SET GLOBAL ndb_eventbuffer_max_alloc=%u", maxBytes);

  if (!sqlClient.doQuery(stmt)) {
    ndbout << "Failed to execute change of eventbuffer size" << endl;
    return NDBT_FAILED;
  }

  ndbout_c("Set ndb_eventbuffer_max to %u bytes", maxBytes);

  return NDBT_OK;
}

static int setEventBufferMax(NDBT_Context *ctx, NDBT_Step *step) {
  const Uint32 maxBytes =
      ctx->getProperty("EventBufferMaxBytes", Uint32(10 * 1024 * 1024));

  return setEventBufferMaxImpl(ctx, step, maxBytes);
}

static int clearEventBufferMax(NDBT_Context *ctx, NDBT_Step *step) {
  return setEventBufferMaxImpl(ctx, step, 0);
}

static int runLockUnlockBinlogIndex(NDBT_Context *ctx, NDBT_Step *step) {
  /* Create a client for talking to MySQLD 1 */
  SqlClient sqlClient("TEST_DB");

  if (!sqlClient.waitConnected()) {
    ndbout << "Failed to connect to SQL" << endl;
    return NDBT_FAILED;
  }

  const Uint32 lockMillis = ctx->getProperty("LockMillis", (Uint32)1000);
  const Uint32 unlockMillis = ctx->getProperty("UnLockMillis", (Uint32)100);

  const char *tab_to_lock = "mysql.ndb_binlog_index";

  ndbout << "Performing lock(" << lockMillis << " millis) unlock ("
         << unlockMillis << " millis) cycle on table " << tab_to_lock
         << " until test stops." << endl;

  BaseString lockQuery;
  lockQuery.assfmt("LOCK TABLES %s WRITE", tab_to_lock);

  while (!ctx->isTestStopped()) {
    // ndbout << "Locking " << tab_to_lock << endl;
    if (!sqlClient.doQuery(lockQuery.c_str())) {
      ndbout << "Failed to lock tables with " << lockQuery << endl;
      return NDBT_FAILED;
    }
    ndbout << "Locked " << tab_to_lock << endl;

    NdbSleep_MilliSleep(lockMillis);

    // ndbout << "Unlocking " << tab_to_lock << endl;
    if (!sqlClient.doQuery("UNLOCK TABLES")) {
      ndbout << "Failed to unlock tables" << endl;
      return NDBT_FAILED;
    }
    ndbout << "Unlocked " << tab_to_lock << endl;

    NdbSleep_MilliSleep(unlockMillis);
  }

  return NDBT_OK;
}

static int runSqlDDL(NDBT_Context *ctx, NDBT_Step *step) {
  /* Create a client for talking to MySQLD 1 */
  SqlClient sqlClient("TEST_DB");

  const bool ignoreErrors =
      (ctx->getProperty("SqlDDLIgnoreErrors", Uint32(1)) == 1);

  while (!ctx->isTestStopped()) {
    ndbout << "Drop DDL_VICTIM" << endl;
    if (!sqlClient.doQuery("DROP TABLE IF EXISTS TEST_DB.DDL_VICTIM")) {
      ndbout << "Failed drop table" << endl;
      if (ignoreErrors) {
        continue;
      }
      return NDBT_FAILED;
    }

    ndbout << "Create DDL_VICTIM" << endl;
    if (!sqlClient.doQuery("CREATE TABLE TEST_DB.DDL_VICTIM ("
                           "a varchar(20), "
                           "b varchar(30), "
                           "c blob, "
                           "d text, "
                           "e int, "
                           "primary key(a,b), unique(e)) "
                           "engine=ndb")) {
      ndbout << "Failed to create table" << endl;
      if (ignoreErrors) {
        continue;
      }
      return NDBT_FAILED;
    }

    ndbout << "ALTER ADD COLUMN DDL_VICTIM" << endl;
    if (!sqlClient.doQuery("ALTER TABLE TEST_DB.DDL_VICTIM "
                           "ADD COLUMN f bigint DEFAULT 20")) {
      ndbout << "Failed ALTER add column" << endl;
      if (ignoreErrors) {
        continue;
      }
      return NDBT_FAILED;
    }

    ndbout << "ALTER DROP COLUMN DDL_VICTIM" << endl;
    if (!sqlClient.doQuery("ALTER TABLE TEST_DB.DDL_VICTIM "
                           "DROP COLUMN f")) {
      ndbout << "Failed ALTER drop column" << endl;
      if (ignoreErrors) {
        continue;
      }
      return NDBT_FAILED;
    }
  }

  return NDBT_OK;
}

NDBT_TESTSUITE(test_event_mysqld);

/**
 * MySQLDEvents* tests are intended to test MySQLD event
 * behaviour under stress
 * Assumption is that a MySQL Server with Binlogging
 * on is running with a my.cnf available at $MYSQL_HOME
 * Tests can be run against any cluster.
 * Tests can be invoked from MTR
 *
 * Variants so far
 *
 *   E : Events flowing
 *       Multithreaded Hugo updates to table
 *   R : Data node restarts
 *       Randome data node restarts, with + without abort
 *   D : Asynchronous MySQLD disconnects
 *       Binlogging MySQLD disconnected by data nodes
 *   O : Event buffer overflow
 *       Event buffer limited, lag built up causing discard
 *   S : Concurrent DDL
 *       DDL + schema distribution on separate table
 *
 * MySQLDEvents*
 *   Restarts                                   ER
 *   Disconnects                                ED
 *   RestartsDisconnects                        ERD
 *   EventBufferOverload                        EO
 *   EventBufferOverloadRestarts                EOR
 *   EventBufferOverloadDisconnects             EOD
 *   EventBufferOverloadRestartDisconnects      EORD
 *   EventBufferOverloadDDL                     EOS
 *
 * Todo
 *   - Have tests check that MySQLD has Binlogging
 *   - Check sanity of Binlog content
 *   - Variable 'load' table schema
 */
TESTCASE("MySQLDEventsRestarts",
         "Test event handling with data node restarts") {
  INITIALIZER(setupT1Sql);
  INITIALIZER(runLoad);
  STEPS(runUpdates, 10);
  STEP(runNodeRestarts);
  FINALIZER(dropT1Sql);
}
TESTCASE("MySQLDEventsDisconnects",
         "Test event handling with MySQLD Disconnects") {
  INITIALIZER(setupT1Sql);
  INITIALIZER(runLoad);
  STEPS(runUpdates, 10);
  STEP(runMySQLDDisconnects);
  STEP(limitRuntime);
  FINALIZER(dropT1Sql);
}
TESTCASE("MySQLDEventsRestartsDisconnects",
         "Test event handling with data node restarts and MySQLD Disconnects") {
  INITIALIZER(setupT1Sql);
  INITIALIZER(runLoad);
  STEPS(runUpdates, 10);
  STEP(runNodeRestarts);
  STEP(runMySQLDDisconnects);
  FINALIZER(dropT1Sql);
}
TESTCASE("MySQLDEventsEventBufferOverload",
         "Test event handling with event buffer overload") {
  INITIALIZER(setupT1Sql);
  INITIALIZER(runLoad);
  INITIALIZER(setEventBufferMax);
  STEPS(runUpdates, 10);
  STEP(runLockUnlockBinlogIndex);
  STEP(limitRuntime);
  FINALIZER(clearEventBufferMax);
  FINALIZER(dropT1Sql);
}
TESTCASE(
    "MySQLDEventsEventBufferOverloadRestarts",
    "Test event handling with event buffer overload and data node restarts ") {
  INITIALIZER(setupT1Sql);
  INITIALIZER(runLoad);
  INITIALIZER(setEventBufferMax);
  STEPS(runUpdates, 10);
  STEP(runLockUnlockBinlogIndex);
  STEP(runNodeRestarts);
  FINALIZER(clearEventBufferMax);
  FINALIZER(dropT1Sql);
}
TESTCASE(
    "MySQLDEventsEventBufferOverloadDisconnects",
    "Test event handling with event buffer overload and MySQLD Disconnects") {
  INITIALIZER(setupT1Sql);
  INITIALIZER(runLoad);
  INITIALIZER(setEventBufferMax);
  STEPS(runUpdates, 10);
  STEP(runLockUnlockBinlogIndex);
  STEP(runMySQLDDisconnects);
  STEP(limitRuntime);
  FINALIZER(clearEventBufferMax);
  FINALIZER(dropT1Sql);
}
TESTCASE("MySQLDEventsEventBufferOverloadRestartsDisconnects",
         "Test event handling with event buffer overload, data node restarts "
         "and MySQLD Disconnects") {
  INITIALIZER(setupT1Sql);
  INITIALIZER(runLoad);
  INITIALIZER(setEventBufferMax);
  STEPS(runUpdates, 10);
  STEP(runLockUnlockBinlogIndex);
  STEP(runNodeRestarts);
  STEP(runMySQLDDisconnects);
  FINALIZER(clearEventBufferMax);
  FINALIZER(dropT1Sql);
}
TESTCASE("MySQLDEventsEventBufferOverloadDDL",
         "Test event handling with event buffer overload and DDL") {
  INITIALIZER(setupT1Sql);
  INITIALIZER(runLoad);
  INITIALIZER(setEventBufferMax);
  STEPS(runUpdates, 10);
  STEP(runLockUnlockBinlogIndex);
  STEP(runSqlDDL);
  STEP(limitRuntime);
  FINALIZER(clearEventBufferMax);
  FINALIZER(dropT1Sql);
}

NDBT_TESTSUITE_END(test_event_mysqld)

int main(int argc, const char **argv) {
  ndb_init();
  NDBT_TESTSUITE_INSTANCE(test_event_mysqld);
  test_event_mysqld.setCreateTable(false);
  test_event_mysqld.setRunAllTables(true);
  return test_event_mysqld.execute(argc, argv);
}
