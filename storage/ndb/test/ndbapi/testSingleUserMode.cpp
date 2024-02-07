/*
  Copyright (c) 2008, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <HugoTransactions.hpp>
#include <NDBT.hpp>
#include <NDBT_Test.hpp>
#include <NdbRestarter.hpp>
#include <UtilTransactions.hpp>

int runClearTable(NDBT_Context *ctx, NDBT_Step *step) {
  int records = ctx->getNumRecords();

  UtilTransactions utilTrans(*ctx->getTab());
  if (utilTrans.clearTable2(GETNDB(step), records) != 0) {
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int create_index_on_pk(Ndb *pNdb, const char *tabName) {
  int result = NDBT_OK;

  const NdbDictionary::Table *tab =
      NDBT_Table::discoverTableFromDb(pNdb, tabName);
  // Create index
  const char *idxName = "IDX_ON_PK";
  ndbout << "Create: " << idxName << "( ";
  NdbDictionary::Index pIdx(idxName);
  pIdx.setTable(tabName);
  pIdx.setType(NdbDictionary::Index::UniqueHashIndex);
  for (int c = 0; c < tab->getNoOfPrimaryKeys(); c++) {
    pIdx.addIndexColumn(tab->getPrimaryKey(c));
    ndbout << tab->getPrimaryKey(c) << " ";
  }

  ndbout << ") ";
  if (pNdb->getDictionary()->createIndex(pIdx) != 0) {
    ndbout << "FAILED!" << endl;
    const NdbError err = pNdb->getDictionary()->getNdbError();
    NDB_ERR(err);
    result = NDBT_FAILED;
  } else {
    ndbout << "OK!" << endl;
  }
  return result;
}

int drop_index_on_pk(Ndb *pNdb, const char *tabName) {
  int result = NDBT_OK;
  const char *idxName = "IDX_ON_PK";
  ndbout << "Drop: " << idxName;
  if (pNdb->getDictionary()->dropIndex(idxName, tabName) != 0) {
    ndbout << "FAILED!" << endl;
    const NdbError err = pNdb->getDictionary()->getNdbError();
    NDB_ERR(err);
    result = NDBT_FAILED;
  } else {
    ndbout << "OK!" << endl;
  }
  return result;
}

#define CHECK(b)                                                          \
  if (!(b)) {                                                             \
    g_err << "ERR: " << step->getName() << " failed on line " << __LINE__ \
          << endl;                                                        \
    result = NDBT_FAILED;                                                 \
    continue;                                                             \
  }

int runTestSingleUserMode(NDBT_Context *ctx, NDBT_Step *step) {
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  Ndb *pNdb = GETNDB(step);
  NdbRestarter restarter;
  char tabName[255];
  strncpy(tabName, ctx->getTab()->getName(), sizeof(tabName) - 1);
  tabName[sizeof(tabName) - 1] = '\0';
  ndbout << "tabName=" << tabName << endl;

  int i = 0;
  int count;
  HugoTransactions hugoTrans(*ctx->getTab());
  UtilTransactions utilTrans(*ctx->getTab());
  while (i < loops && result == NDBT_OK) {
    g_info << i << ": ";
    int timeout = 120;
    int nodeId = restarter.getMasterNodeId();
    // Test that it's not possible to restart one node in single user mode
    CHECK(restarter.enterSingleUserMode(pNdb->getNodeId()) == 0);
    CHECK(restarter.waitClusterSingleUser(timeout) == 0);
    CHECK(restarter.restartOneDbNode(nodeId) != 0)
    CHECK(restarter.exitSingleUserMode() == 0);
    CHECK(restarter.waitClusterStarted(timeout) == 0);
    CHK_NDB_READY(pNdb);

    // Test that the single user mode api can do everything
    CHECK(restarter.enterSingleUserMode(pNdb->getNodeId()) == 0);
    CHECK(restarter.waitClusterSingleUser(timeout) == 0);
    CHECK(hugoTrans.loadTable(pNdb, records, 128) == 0);
    CHECK(hugoTrans.pkReadRecords(pNdb, records) == 0);
    CHECK(hugoTrans.pkUpdateRecords(pNdb, records) == 0);
    CHECK(utilTrans.selectCount(pNdb, 64, &count) == 0);
    CHECK(count == records);
    CHECK(hugoTrans.pkDelRecords(pNdb, records / 2) == 0);
    CHECK(hugoTrans.scanReadRecords(pNdb, records / 2, 0, 64) == 0);
    CHECK(utilTrans.selectCount(pNdb, 64, &count) == 0);
    CHECK(count == (records / 2));
    CHECK(utilTrans.clearTable(pNdb, records / 2) == 0);
    CHECK(restarter.exitSingleUserMode() == 0);
    CHECK(restarter.waitClusterStarted(timeout) == 0);
    CHK_NDB_READY(pNdb);

    // Test create index in single user mode
    CHECK(restarter.enterSingleUserMode(pNdb->getNodeId()) == 0);
    CHECK(restarter.waitClusterSingleUser(timeout) == 0);
    CHECK(create_index_on_pk(pNdb, tabName) == 0);
    CHECK(hugoTrans.loadTable(pNdb, records, 128) == 0);
    CHECK(hugoTrans.pkReadRecords(pNdb, records) == 0);
    CHECK(hugoTrans.pkUpdateRecords(pNdb, records) == 0);
    CHECK(utilTrans.selectCount(pNdb, 64, &count) == 0);
    CHECK(count == records);
    CHECK(hugoTrans.pkDelRecords(pNdb, records / 2) == 0);
    CHECK(drop_index_on_pk(pNdb, tabName) == 0);
    CHECK(restarter.exitSingleUserMode() == 0);
    CHECK(restarter.waitClusterStarted(timeout) == 0);
    CHK_NDB_READY(pNdb);

    // Test recreate index in single user mode
    CHECK(create_index_on_pk(pNdb, tabName) == 0);
    CHECK(hugoTrans.loadTable(pNdb, records, 128) == 0);
    CHECK(utilTrans.selectCount(pNdb, 64, &count) == 0);
    CHECK(restarter.enterSingleUserMode(pNdb->getNodeId()) == 0);
    CHECK(restarter.waitClusterSingleUser(timeout) == 0);
    CHECK(drop_index_on_pk(pNdb, tabName) == 0);
    CHECK(utilTrans.selectCount(pNdb, 64, &count) == 0);
    CHECK(create_index_on_pk(pNdb, tabName) == 0);
    CHECK(restarter.exitSingleUserMode() == 0);
    CHECK(restarter.waitClusterStarted(timeout) == 0);
    CHK_NDB_READY(pNdb);
    CHECK(drop_index_on_pk(pNdb, tabName) == 0);

    CHECK(utilTrans.clearTable(GETNDB(step), records) == 0);

    ndbout << "Restarting cluster" << endl;
    CHECK(restarter.restartAll() == 0);
    CHECK(restarter.waitClusterStarted(timeout) == 0);
    CHECK(pNdb->waitUntilReady(timeout) == 0);

    i++;
  }
  return result;
}

NDBT_TESTSUITE(testSingleUserMode);
TESTCASE("SingleUserMode", "Test single user mode") {
  INITIALIZER(runTestSingleUserMode);
  FINALIZER(runClearTable);
}
NDBT_TESTSUITE_END(testSingleUserMode)

int main(int argc, const char **argv) {
  ndb_init();
  NDBT_TESTSUITE_INSTANCE(testSingleUserMode);
  return testSingleUserMode.execute(argc, argv);
}
