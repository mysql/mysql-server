/*
   Copyright (c) 2003, 2016, Oracle and/or its affiliates. All rights reserved.

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

#include <NDBT.hpp>
#include <NDBT_Test.hpp>
#include <HugoTransactions.hpp>
#include <UtilTransactions.hpp>
#include <NdbBackup.hpp>
#include <NdbMgmd.hpp>
#include <signaldata/DumpStateOrd.hpp>

int runDropTable(NDBT_Context* ctx, NDBT_Step* step);

#define CHECK(b) if (!(b)) { \
  g_err << "ERR: "<< step->getName() \
         << " failed on line " << __LINE__ << endl; \
  result = NDBT_FAILED; \
  continue; } 

char tabname[1000];

int
clearOldBackups(NDBT_Context* ctx, NDBT_Step* step)
{
  strcpy(tabname, ctx->getTab()->getName());
  NdbBackup backup;
  backup.clearOldBackups();
  return NDBT_OK;
}

int runLoadTable(NDBT_Context* ctx, NDBT_Step* step){

  int records = ctx->getNumRecords();
  HugoTransactions hugoTrans(*ctx->getTab());
  if (hugoTrans.loadTable(GETNDB(step), records) != 0){
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int runLoadTable10000(NDBT_Context* ctx, NDBT_Step* step)
{
  int records = 10000;
  HugoTransactions hugoTrans(*ctx->getTab());
  if (hugoTrans.loadTable(GETNDB(step), records) != 0){
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

bool testMaster = true;
bool testSlave = false;

int setMaster(NDBT_Context* ctx, NDBT_Step* step){
  testMaster = true;
  testSlave = false;
  return NDBT_OK;
}
int setMasterAsSlave(NDBT_Context* ctx, NDBT_Step* step){
  testMaster = true;
  testSlave = true;
  return NDBT_OK;
}
int setSlave(NDBT_Context* ctx, NDBT_Step* step){
  testMaster = false;
  testSlave = true;
  return NDBT_OK;
}

struct scan_holder_type
{
  NdbScanOperation *pOp;
  NdbTransaction *pTrans;
};

int createOrderedPkIndex(NDBT_Context* ctx, NDBT_Step* step)
{
  char orderedPkIdxName[255];
  const NdbDictionary::Table* pTab = ctx->getTab();
  Ndb* pNdb = GETNDB(step);

  // Create index
  BaseString::snprintf(orderedPkIdxName, sizeof(orderedPkIdxName),
                       "IDC_O_PK_%s", pTab->getName());
  NdbDictionary::Index pIdx(orderedPkIdxName);
  pIdx.setTable(pTab->getName());
  pIdx.setType(NdbDictionary::Index::OrderedIndex);
  pIdx.setLogging(false);

  for (int c = 0; c< pTab->getNoOfColumns(); c++)
  {
    const NdbDictionary::Column * col = pTab->getColumn(c);
    if (col->getPrimaryKey())
    {
      pIdx.addIndexColumn(col->getName());
    }
  }
  if (pNdb->getDictionary()->createIndex(pIdx) != 0)
  {
    ndbout << "FAILED! to create index" << endl;
    const NdbError err = pNdb->getDictionary()->getNdbError();
    NDB_ERR(err);
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int start_scan_no_close(NDBT_Context *ctx,
                        Ndb *pNdb,
                        scan_holder_type *scan_holder,
                        int scan_flags,
                        int i)
{
  const NdbDictionary::Table *tab = ctx->getTab();
  scan_holder->pTrans = pNdb->startTransaction();
  if (scan_holder->pTrans == NULL)
  {
    g_err << "Failed to start transaction, line: "
          << __LINE__ << " i = " << i << endl;
    return NDBT_FAILED;
  }
  if (scan_flags != NdbScanOperation::SF_OrderBy)
  {
    scan_holder->pOp =
      scan_holder->pTrans->getNdbScanOperation(tab->getName());
  }
  else
  {
    char pkIdxName[255];
    BaseString::snprintf(pkIdxName, 255, "IDC_O_PK_%s", tab->getName());
    scan_holder->pOp =
      scan_holder->pTrans->getNdbIndexScanOperation(pkIdxName, tab->getName());
  }
  if (scan_holder->pOp == NULL)
  {
    g_err << "Failed to get scan op, line: "
          << __LINE__ << " i = " << i << endl;
    return NDBT_FAILED;
  }
  if (scan_holder->pOp->readTuples(NdbOperation::LM_CommittedRead,
                                   scan_flags,
                                   240))
  {
    g_err << "Failed call to readTuples, line: "
          << __LINE__ << " i = " << i << endl;
    return NDBT_FAILED;
  }
  for (int j = 0; j < tab->getNoOfColumns(); j++)
  {
    if (scan_holder->pOp->getValue(tab->getColumn(j)->getName()) == 0)
    {
      g_err << "Failed to get value, line: "
            << __LINE__ << " i = " << i << " j = " << j << endl;
      return NDBT_FAILED;
    }
  }
  if (scan_holder->pTrans->execute(NoCommit, AbortOnError) == -1)
  {
    g_err << "Failed to exec scan op, line: "
          << __LINE__ << " i = " << i << endl;
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int outOfScanRecordsInLDM(NDBT_Context *ctx, NDBT_Step *step)
{
  NdbBackup backup;
  unsigned backupId = 0;
  int i;
  Ndb* pNdb;
  scan_holder_type scan_holder_array[256];
  const static int NUM_ACC_SCANS = 12;
  const static int NUM_TUX_SCANS = 122;
  const static int NUM_TUP_SCANS = 119;
  
  pNdb = GETNDB(step);
  for (i = 0; i < NUM_ACC_SCANS; i++)
  {
    /**
     * We start 12 ACC scans, we can have at most 12 ACC scans, if more
     * are used we will queue up the scans. Here we use up all of them
     * but don't queue up any.
     */
    if (start_scan_no_close(ctx,
                            pNdb,
                            &scan_holder_array[i],
                            0,
                            i)
                            == NDBT_FAILED)
    {
      return NDBT_FAILED;
    }
  }
  for (; i < NUM_ACC_SCANS + NUM_TUX_SCANS; i++)
  {
    /**
     * In the default config which we assume in this test case, we can
     * start up 122 parallel range scans on a fragment. Here we use up
     * all of those slots, so no queueing will occur.
     */
    if (start_scan_no_close(ctx,
                            pNdb,
                            &scan_holder_array[i],
                            NdbScanOperation::SF_OrderBy,
                            i)
                            == NDBT_FAILED)
    {
      return NDBT_FAILED;
    }
  }
  for (; i < NUM_ACC_SCANS + NUM_TUX_SCANS + NUM_TUP_SCANS + 1; i++)
  {
    /**
     * In the default config we can have up to 119 Tup scans without queueing.
     * Here we will attempt to start 120 Tup scans. The last one will be
     * queued. This runs some code where queued scans are handled from
     * close scan which aborted. This found a bug which we ensure gets
     * retested by this overallocation.
     */
    if (start_scan_no_close(ctx,
                            pNdb,
                            &scan_holder_array[i],
                            NdbScanOperation::SF_TupScan,
                            i)
                            == NDBT_FAILED)
    {
      return NDBT_FAILED;
    }
  }

  /**
   * Start an LCP to ensure that we test LCP scans while grabbing all
   * scan number resources.
   */
  NdbRestarter restarter;
  int dumpCode = 7099;
  restarter.dumpStateAllNodes(&dumpCode, 1);

  /**
   * At this point we have allocated all scan numbers, so no more scan
   * numbers are available. Backup should still function since it uses
   * a reserved scan number, we verify this here.
   */
  if (backup.start(backupId) == -1)
  {
    return NDBT_FAILED;
  }
  ndbout << "Started backup " << backupId << endl;
  ctx->setProperty("BackupId", backupId);

  /**
   * We sleep for 5 seconds which randomly leads to execution of LCP
   * scans. This also uses reserved scan number. To decrease randomness
   * we programmatically start an LCP above.
   */
  NdbSleep_SecSleep(5);

  /**
   * Close down all connections.
   */
  for (i = 0; i < NUM_ACC_SCANS + NUM_TUX_SCANS + NUM_TUP_SCANS + 1; i++)
  {
    scan_holder_array[i].pTrans->execute(NdbTransaction::Rollback);
    pNdb->closeTransaction(scan_holder_array[i].pTrans);
    scan_holder_array[i].pTrans = NULL;
  }
  return NDBT_OK;
}

int runAbort(NDBT_Context* ctx, NDBT_Step* step){
  NdbBackup backup;

  NdbRestarter restarter;

  if (restarter.getNumDbNodes() < 2){
    ctx->stopTest();
    return NDBT_OK;
  }

  if(restarter.waitClusterStarted(60) != 0){
    g_err << "Cluster failed to start" << endl;
    return NDBT_FAILED;
  }

  if (testMaster) {
    if (testSlave) {
      if (backup.NFMasterAsSlave(restarter) != NDBT_OK){
	return NDBT_FAILED;
      }
    } else {
      if (backup.NFMaster(restarter) != NDBT_OK){
	return NDBT_FAILED;
      }
    }
  } else {
    if (backup.NFSlave(restarter) != NDBT_OK){
      return NDBT_FAILED;
    }
  }
  
  return NDBT_OK;
}

int runFail(NDBT_Context* ctx, NDBT_Step* step){
  NdbBackup backup;

  NdbRestarter restarter;

  if (restarter.getNumDbNodes() < 2){
    ctx->stopTest();
    return NDBT_OK;
  }

  if(restarter.waitClusterStarted(60) != 0){
    g_err << "Cluster failed to start" << endl;
    return NDBT_FAILED;
  }

  if (testMaster) {
    if (testSlave) {
      if (backup.FailMasterAsSlave(restarter) != NDBT_OK){
	return NDBT_FAILED;
      }
    } else {
      if (backup.FailMaster(restarter) != NDBT_OK){
	return NDBT_FAILED;
      }
    }
  } else {
    if (backup.FailSlave(restarter) != NDBT_OK){
      return NDBT_FAILED;
    }
  }

  return NDBT_OK;
}

int outOfLDMRecords(NDBT_Context *ctx, NDBT_Step *step)
{
  int res;
  int row = 0;
  NdbBackup backup;
  NdbRestarter restarter;
  unsigned backupId = 0;
  HugoOperations hugoOps(*ctx->getTab());
  Ndb *pNdb = GETNDB(step);

  if (hugoOps.startTransaction(pNdb) != 0)
  {
    g_err << "Failed to start transaction, line: " << __LINE__ << endl;
    return NDBT_FAILED;
  }
  while (true)
  {
    if (hugoOps.pkInsertRecord(pNdb, row) != 0)
    {
      g_err << "Failed to define insert, line: " << __LINE__ << endl;
      return NDBT_FAILED;
    }
    res = hugoOps.execute_NoCommit(pNdb, AO_IgnoreError);
    if (res == 0)
    {
      row++;
    }
    else
    {
      break;
    }
  }
  /**
   * Here we always come with a failure, but we want the failure to
   * be out of operation records in LDM, any other error isn't
   * testing what we want, but we will pass the test even with
   * other error codes. The only indication of the test failing is
   * when executed as part of the autotest framework when a data node
   * failure will happen if the test fails. This is what the original
   * bug caused and what we verify that we actually fixed.
   *
   * Getting error code 1217 here means that at least 1 LDM thread is
   * out of operation records, this is sufficient to test since LCPs
   * always use all of the LDMs. Backups currently only use 1, so here
   * the test only is only temporary testing. We ensure that an LCP is
   * ongoing while we're out of operation records.
   */
  if (res == 1217)
  {
    ndbout << "Out of LDM operation records as desired" << endl;
  }
  else
  {
    ndbout << "Result code is " << res << endl;
    ndbout << "We will continue anyways although test isn't useful" << endl;
  }
  /* Ensure an LCP is executed in out of resource state. */
  int dumpCode=7099;
  restarter.dumpStateAllNodes(&dumpCode, 1);

  if (backup.start(backupId) == -1)
  {
    g_err << "Start backup failed: Line: " << __LINE__ << endl;
    return NDBT_FAILED;
  }
  ndbout << "Started backup " << backupId << endl;
  NdbSleep_SecSleep(5); /* Give LCP some time to execute */
  hugoOps.closeTransaction(pNdb);
  return NDBT_OK;
}

int runBackupOne(NDBT_Context* ctx, NDBT_Step* step){
  NdbBackup backup;
  unsigned backupId = 0;

  if (backup.start(backupId) == -1){
    return NDBT_FAILED;
  }
  ndbout << "Started backup " << backupId << endl;
  ctx->setProperty("BackupId", backupId);

  return NDBT_OK;
}

int runBackupRandom(NDBT_Context* ctx, NDBT_Step* step){
  NdbBackup backup;
  unsigned backupId = rand() % (MAX_BACKUPS);

  if (backup.start(backupId) == -1){
    return NDBT_FAILED;
  }
  ndbout << "Started backup " << backupId << endl;
  ctx->setProperty("BackupId", backupId);

  return NDBT_OK;
}

int
runBackupLoop(NDBT_Context* ctx, NDBT_Step* step){
  NdbBackup backup;
  
  int loops = ctx->getNumLoops();
  while(!ctx->isTestStopped() && loops--)
  {
    if (backup.start() == -1)
    {
      sleep(1);
      loops++;
    }
    else
    {
      sleep(3);
    }
  }

  ctx->stopTest();
  return NDBT_OK;
}

int
runDDL(NDBT_Context* ctx, NDBT_Step* step){
  Ndb* pNdb= GETNDB(step);
  NdbDictionary::Dictionary* pDict = pNdb->getDictionary();
  
  const int tables = NDBT_Tables::getNumTables();
  while(!ctx->isTestStopped())
  {
    const int tab_no = rand() % (tables);
    NdbDictionary::Table tab = *NDBT_Tables::getTable(tab_no);
    BaseString name= tab.getName();
    name.appfmt("-%d", step->getStepNo());
    tab.setName(name.c_str());
    if(pDict->createTable(tab) == 0)
    {
      HugoTransactions hugoTrans(* pDict->getTable(name.c_str()));
      if (hugoTrans.loadTable(pNdb, 10000) != 0){
	return NDBT_FAILED;
      }
      
      while(pDict->dropTable(tab.getName()) != 0 &&
	    pDict->getNdbError().code != 4009)
	g_err << pDict->getNdbError() << endl;
      
      sleep(1);

    }
  }
  return NDBT_OK;
}


int runDropTablesRestart(NDBT_Context* ctx, NDBT_Step* step){
  NdbRestarter restarter;

  if (runDropTable(ctx, step) != 0)
    return NDBT_FAILED;

  if (restarter.restartAll(false) != 0)
    return NDBT_FAILED;

  if (restarter.waitClusterStarted() != 0)
    return NDBT_FAILED;
  
  return NDBT_OK;
}

int runRestoreOne(NDBT_Context* ctx, NDBT_Step* step){
  NdbBackup backup;
  unsigned backupId = ctx->getProperty("BackupId"); 

  ndbout << "Restoring backup " << backupId << endl;

  if (backup.restore(backupId) == -1){
    return NDBT_FAILED;
  }

  Ndb* pNdb = GETNDB(step);
  pNdb->getDictionary()->invalidateTable(tabname);
  const NdbDictionary::Table* tab = pNdb->getDictionary()->getTable(tabname);
  
  if (tab)
  {
    ctx->setTab(tab);
    return NDBT_OK;
  }

  return NDBT_FAILED;
}

int runVerifyOne(NDBT_Context* ctx, NDBT_Step* step){
  int records = ctx->getNumRecords();
  Ndb* pNdb = GETNDB(step);
  int result = NDBT_OK;
  int count = 0;

  const NdbDictionary::Table* tab = ctx->getTab();
  if(tab == 0)
    return NDBT_FAILED;
  
  UtilTransactions utilTrans(* tab);
  HugoTransactions hugoTrans(* tab);

  do{

    // Check that there are as many records as we expected
    CHECK(utilTrans.selectCount(pNdb, 64, &count) == 0);

    g_err << "count = " << count;
    g_err << " records = " << records;
    g_err << endl;

    CHECK(count == records);

    // Read and verify every record
    CHECK(hugoTrans.pkReadRecords(pNdb, records) == 0);
    
  } while (false);

  return result;
}

int runClearTable(NDBT_Context* ctx, NDBT_Step* step){
  int records = ctx->getNumRecords();
  
  UtilTransactions utilTrans(*ctx->getTab());
  if (utilTrans.clearTable2(GETNDB(step),  records) != 0){
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int runDropTable(NDBT_Context* ctx, NDBT_Step* step)
{
  
  const NdbDictionary::Table *tab = ctx->getTab();
  GETNDB(step)->getDictionary()->dropTable(tab->getName());
  
  return NDBT_OK;
}

#include "bank/Bank.hpp"

int runCreateBank(NDBT_Context* ctx, NDBT_Step* step){
  Bank bank(ctx->m_cluster_connection);
  bool overWriteExisting = true;
  if (bank.createAndLoadBank(overWriteExisting) != NDBT_OK)
    return NDBT_FAILED;
  return NDBT_OK;
}

int runBankTimer(NDBT_Context* ctx, NDBT_Step* step){
  Bank bank(ctx->m_cluster_connection);
  int wait = 30; // Max seconds between each "day"
  int yield = 1; // Loops before bank returns 

  while (ctx->isTestStopped() == false) {
    bank.performIncreaseTime(wait, yield);
  }
  return NDBT_OK;
}

int runBankTransactions(NDBT_Context* ctx, NDBT_Step* step){
  Bank bank(ctx->m_cluster_connection);
  int wait = 10; // Max ms between each transaction
  int yield = 100; // Loops before bank returns 

  while (ctx->isTestStopped() == false) {
    bank.performTransactions(wait, yield);
  }
  return NDBT_OK;
}

int runBankGL(NDBT_Context* ctx, NDBT_Step* step){
  Bank bank(ctx->m_cluster_connection);
  int yield = 20; // Loops before bank returns 
  int result = NDBT_OK;

  while (ctx->isTestStopped() == false) {
    if (bank.performMakeGLs(yield) != NDBT_OK){
      ndbout << "bank.performMakeGLs FAILED" << endl;
      result = NDBT_FAILED;
    }
  }
  return NDBT_OK;
}

int runBankSum(NDBT_Context* ctx, NDBT_Step* step){
  Bank bank(ctx->m_cluster_connection);
  int wait = 2000; // Max ms between each sum of accounts
  int yield = 1; // Loops before bank returns 
  int result = NDBT_OK;

  while (ctx->isTestStopped() == false) {
    if (bank.performSumAccounts(wait, yield) != NDBT_OK){
      ndbout << "bank.performSumAccounts FAILED" << endl;
      result = NDBT_FAILED;
    }
  }
  return result ;
}

int runDropBank(NDBT_Context* ctx, NDBT_Step* step){
  Bank bank(ctx->m_cluster_connection);
  if (bank.dropBank() != NDBT_OK)
    return NDBT_FAILED;
  return NDBT_OK;
}

int runBackupBank(NDBT_Context* ctx, NDBT_Step* step){
  int loops = ctx->getNumLoops();
  int l = 0;
  int maxSleep = 30; // Max seconds between each backup
  Ndb* pNdb = GETNDB(step);
  NdbBackup backup;
  unsigned minBackupId = ~0;
  unsigned maxBackupId = 0;
  unsigned backupId = 0;
  int result = NDBT_OK;

  while (l < loops && result != NDBT_FAILED){

    if (pNdb->waitUntilReady() != 0){
      result = NDBT_FAILED;
      continue;
    }

    // Sleep for a while
    NdbSleep_SecSleep(maxSleep);
    
    // Perform backup
    if (backup.start(backupId) != 0){
      ndbout << "backup.start failed" << endl;
      result = NDBT_FAILED;
      continue;
    }
    ndbout << "Started backup " << backupId << endl;

    // Remember min and max backupid
    if (backupId < minBackupId)
      minBackupId = backupId;

    if (backupId > maxBackupId)
      maxBackupId = backupId;
    
    ndbout << " maxBackupId = " << maxBackupId 
	   << ", minBackupId = " << minBackupId << endl;
    ctx->setProperty("MinBackupId", minBackupId);    
    ctx->setProperty("MaxBackupId", maxBackupId);    
    
    l++;
  }

  ctx->stopTest();

  return result;
}

int runRestoreBankAndVerify(NDBT_Context* ctx, NDBT_Step* step){
  NdbRestarter restarter;
  NdbBackup backup;
  unsigned minBackupId = ctx->getProperty("MinBackupId");
  unsigned maxBackupId = ctx->getProperty("MaxBackupId");
  unsigned backupId = minBackupId;
  int result = NDBT_OK;
  int errSumAccounts = 0;
  int errValidateGL = 0;

  ndbout << " maxBackupId = " << maxBackupId << endl;
  ndbout << " minBackupId = " << minBackupId << endl;
  
  while (backupId <= maxBackupId){

    // TEMPORARY FIX
    // To erase all tables from cache(s)
    // To be removed, maybe replaced by ndb.invalidate();
    {
      Bank bank(ctx->m_cluster_connection);
      
      if (bank.dropBank() != NDBT_OK){
	result = NDBT_FAILED;
	break;
      }
    }
    // END TEMPORARY FIX

    ndbout << "Performing restart" << endl;
    if (restarter.restartAll(false) != 0)
      return NDBT_FAILED;

    if (restarter.waitClusterStarted() != 0)
      return NDBT_FAILED;
    CHK_NDB_READY(GETNDB(step));

    ndbout << "Dropping " << tabname << endl;
    NdbDictionary::Dictionary* pDict = GETNDB(step)->getDictionary();
    pDict->dropTable(tabname);

    ndbout << "Restoring backup " << backupId << endl;
    if (backup.restore(backupId) == -1){
      return NDBT_FAILED;
    }
    ndbout << "Backup " << backupId << " restored" << endl;

    // Let bank verify
    Bank bank(ctx->m_cluster_connection);

    int wait = 0;
    int yield = 1;
    if (bank.performSumAccounts(wait, yield) != 0){
      ndbout << "bank.performSumAccounts FAILED" << endl;
      ndbout << "  backupId = " << backupId << endl << endl;
      result = NDBT_FAILED;
      errSumAccounts++;
    }

    if (bank.performValidateAllGLs() != 0){
      ndbout << "bank.performValidateAllGLs FAILED" << endl;
      ndbout << "  backupId = " << backupId << endl << endl;
      result = NDBT_FAILED;
      errValidateGL++;
    }

    backupId++;
  }
  
  if (result != NDBT_OK){
    ndbout << "Verification of backup failed" << endl
	   << "  errValidateGL="<<errValidateGL<<endl
	   << "  errSumAccounts="<<errSumAccounts<<endl << endl;
  }
  
  return result;
}
int runBackupUndoWaitStarted(NDBT_Context* ctx, NDBT_Step* step){
  NdbBackup backup;
  unsigned backupId = 0;
  int undoError = 10041;
  NdbRestarter restarter;

  if(restarter.waitClusterStarted(60)){
    g_err << "waitClusterStarted failed"<< endl;
    return NDBT_FAILED;
  }

  if (restarter.insertErrorInAllNodes(undoError) != 0) {
    g_err << "Error insert failed" << endl;
    return NDBT_FAILED;
  } 
  // start backup wait started
  if (backup.start(backupId, 1, 0, 1) == -1){
    return NDBT_FAILED;
  }
  ndbout << "Started backup " << backupId << endl;
  ctx->setProperty("BackupId", backupId);

  return NDBT_OK;
}
int runChangeUndoDataDuringBackup(NDBT_Context* ctx, NDBT_Step* step){
  Ndb* pNdb= GETNDB(step);

  int records = ctx->getNumRecords();
  int num = 5;
  if (records - 5 < 0)
    num = 1;
  
  HugoTransactions hugoTrans(*ctx->getTab());

  //update all rows
  if(hugoTrans.pkUpdateRecords(pNdb, records) != 0) {
    g_err << "Can't update all the records" << endl;
    return NDBT_FAILED;
  }

  //delete first 10 rows
  if(hugoTrans.pkDelRecords(pNdb, num*2) != 0) {
    g_err << "Can't delete first 5 rows" << endl;
    return NDBT_FAILED;
  }

  //add 5 new rows at the first(0 ~ 4)
  NdbTransaction *pTransaction= pNdb->startTransaction();
  if (pTransaction == NULL) {
    g_err << "Can't get transaction pointer" << endl;
    return NDBT_FAILED;
  }
  if(hugoTrans.setTransaction(pTransaction) != 0) {
    g_err << "Set transaction error" << endl;
    pNdb->closeTransaction(pTransaction);
    return NDBT_FAILED;
  }
  if(hugoTrans.pkInsertRecord(pNdb, 0, num, 2) != 0) {
    g_err << "pkInsertRecord error" << endl;
    pNdb->closeTransaction(pTransaction);
    return NDBT_FAILED;
  }   
  if(pTransaction->execute(Commit ) != 0) {
    g_err << "Can't commit transaction delete" << endl;
    return NDBT_FAILED;
  }
  hugoTrans.closeTransaction(pNdb);

  // make sure backup have finish
  NdbBackup backup;

  // start log event
  if(backup.startLogEvent() != 0) {
    g_err << "Can't create log event" << endl;
    return NDBT_FAILED;
  }
  NdbSleep_SecSleep(15);
  int i = 0;
  while (1) {
    if (backup.checkBackupStatus() == 2) //complete
      break;
    else if (i == 15) {
      g_err << "Backup timeout" << endl;
      return NDBT_FAILED;
    } else
      NdbSleep_SecSleep(2);
    i++;
  }

  return NDBT_OK;
}

int runVerifyUndoData(NDBT_Context* ctx, NDBT_Step* step){
  int records = ctx->getNumRecords();
  Ndb* pNdb = GETNDB(step);
  int count = 0;
  int num = 5;
  if (records - 5 < 0)
    num = 1;

  const NdbDictionary::Table* tab = 
    GETNDB(step)->getDictionary()->getTable(ctx->getTab()->getName());

  if(tab == 0) {
    g_err << " Can't find table" << endl;
    return NDBT_FAILED;
  }
  
  UtilTransactions utilTrans(* tab);
  HugoTransactions hugoTrans(* tab);

  // Check that there are as many records as we expected
  if(utilTrans.selectCount(pNdb, 64, &count) != 0) {
    g_err << "Can't get records count" << endl;
    return NDBT_FAILED;
  }

  g_err << "count = " << count;
  g_err << " records = " << records;
  g_err << endl;

  if (count != records) {
    g_err << "The records count is not correct" << endl;
    return NDBT_FAILED;
  }

  // make sure all the update data is there
  NdbTransaction *pTransaction= pNdb->startTransaction();
  if (pTransaction == NULL) {
    g_err << "Can't get transaction pointer" << endl;
    return NDBT_FAILED;
  }
  if(hugoTrans.setTransaction(pTransaction) != 0) {
    g_err << "Set transaction error" << endl;
    pNdb->closeTransaction(pTransaction);
    return NDBT_FAILED;
  }
   if(hugoTrans.pkReadRecord(pNdb, 0, records, NdbOperation::LM_Read) != 0) {
      g_err << "Can't read record" << endl;
      return NDBT_FAILED;
    }
  if(hugoTrans.verifyUpdatesValue(0, records) != 0) {
    g_err << "The records restored with undo log is not correct" << endl;
    return NDBT_FAILED;
  }
  hugoTrans.closeTransaction(pNdb);

  return NDBT_OK;
}

int
runBug57650(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbBackup backup;
  NdbRestarter res;

  int node0 = res.getNode(NdbRestarter::NS_RANDOM);
  res.insertErrorInNode(node0, 5057);

  unsigned backupId = 0;
  if (backup.start(backupId) == -1)
    return NDBT_FAILED;

  res.insertErrorInAllNodes(5057);
  int val2[] = { 7099 }; // Force LCP
  res.dumpStateAllNodes(val2, 1);

  NdbSleep_SecSleep(5);
  res.waitClusterStarted();

  res.insertErrorInAllNodes(0);

  return NDBT_OK;
}

int
runBug14019036(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbBackup backup;
  NdbRestarter res;
  NdbMgmd mgmd;

  res.insertErrorInAllNodes(5073); // slow down backup

  if (!mgmd.connect()) {
    g_err << "Cannot connect to mgmd server" << endl;
    return NDBT_FAILED;
  }
  if (!mgmd.subscribe_to_events()) {
    g_err << "Cannot subscribe to mgmd server logevents" << endl;
    return NDBT_FAILED;
  }
  Uint64 maxWaitSeconds = 10;
  Uint64 endTime = NdbTick_CurrentMillisecond() +
                   (maxWaitSeconds * 1000);

  int val2[] = { 100000 }; // all dump 100000 
  unsigned backupId = 0;
  if (backup.start(backupId, 1, 0, 1) == -1) {
    g_err << "Failed to start backup nowait" << endl;
    return NDBT_FAILED;
  }
  int records = 0, data = 0;
  int result = NDBT_OK;
  while (NdbTick_CurrentMillisecond() < endTime)
  {
    char buff[512];
    char tmp[512];

    // dump backup status in mgmd log 
    res.dumpStateAllNodes(val2, 1);

    // read backup status logevent from mgmd
    if (!mgmd.get_next_event_line(buff, sizeof(buff), 10 * 1000)) {
      g_err << "Failed to read logevent from mgmd" << endl;
      return NDBT_FAILED;
    }
    if(strstr(buff, "#Records")) 
       sscanf(buff, "%s %d", tmp, &records);
    if(strstr(buff, "Data")) {
      sscanf(buff, "%s %d", tmp, &data);
      if(records == 0 && data > 0) {
        g_err << "Inconsistent backup status: ";
        g_err << "Data written = " << data << " bytes, Record count = 0" << endl;
        result = NDBT_FAILED;
        break;
      }
      else if(records > 0 && data > 0)
        break;
    }
  }    

  res.insertErrorInAllNodes(0);

  return result;
}
int
runBug16656639(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbBackup backup;
  NdbRestarter res;

  res.insertErrorInAllNodes(10032); 

  g_err << "Dumping schema state." << endl;

  int dump1 = DumpStateOrd::SchemaResourceSnapshot;
  int dump2 = DumpStateOrd::SchemaResourceCheckLeak;
  res.dumpStateAllNodes(&dump1, 1);

  g_err << "Starting backup." << endl;
  unsigned backupId = 0;
  if (backup.start(backupId, 1, 0, 1) == -1) {
    g_err << "Failed to start backup." << endl;
    return NDBT_FAILED;
  }

  g_err << "Waiting 1 sec for frag scans to start." << endl;
  NdbSleep_SecSleep(1);

  g_err << "Aborting backup." << endl;
  if(backup.abort(backupId) == -1) {
    g_err << "Failed to abort backup." << endl;
    return NDBT_FAILED;
  }

  g_err << "Checking backup status." << endl;
  if(backup.startLogEvent() != 0) {
    g_err << "Can't create log event." << endl;
    return NDBT_FAILED;
  }
  if(backup.checkBackupStatus() != 3) {
    g_err << "Backup not aborted." << endl;
    return NDBT_FAILED;
  }

  res.insertErrorInAllNodes(0);
  if(res.dumpStateAllNodes(&dump2, 1) != 0) {
    g_err << "Schema leak." << endl;
    return NDBT_FAILED;
  }
  return NDBT_OK;
}


int makeTmpTable(NdbDictionary::Table& tab, NdbDictionary::Index &idx, const char *tableName, const char *columnName)
{
  tab.setName(tableName);
  tab.setLogging(true);
  {
    // create column
    NdbDictionary::Column col(columnName);
    col.setType(NdbDictionary::Column::Unsigned);
    col.setPrimaryKey(true);
    tab.addColumn(col);

    // create index on column
    idx.setTable(tableName);
    idx.setName("idx1");
    idx.setType(NdbDictionary::Index::OrderedIndex);
    idx.setLogging(false);
    idx.addColumnName(columnName);
  }
  NdbError error;
  return tab.validate(error);
} 

int
runBug17882305(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbBackup backup;
  NdbRestarter res;
  NdbDictionary::Table tab;
  NdbDictionary::Index idx;
  const char *tablename = "#sql-dummy"; 
  const char *colname = "_id"; 
  
  Ndb* const ndb = GETNDB(step);
  NdbDictionary::Dictionary* const dict = ndb->getDictionary();

  // create "#sql-dummy" table
  if(makeTmpTable(tab, idx, tablename, colname) == -1) {
    g_err << "Validation of #sql table failed" << endl;
    return NDBT_FAILED;
  }
  if (dict->createTable(tab) == -1) {
    g_err << "Failed to create #sql table." << endl;
    return NDBT_FAILED;
  }
  if (dict->createIndex(idx) == -1) {
    g_err << "Failed to create index, error: " << dict->getNdbError() << endl;
    return NDBT_FAILED;
  }

  /**
   * Test CONTINUEB in get table fragmentation while at it.
   */
  res.insertErrorInAllNodes(10046);

  // start backup which will contain "#sql-dummy"
  g_err << "Starting backup." << endl;
  unsigned backupId = 0;
  if (backup.start(backupId, 2, 0, 1) == -1) {
    g_err << "Failed to start backup." << endl;
    return NDBT_FAILED;
  }

  // drop "#sql-dummy"
  if (dict->dropTable(tablename) == -1) {
    g_err << "Failed to drop #sql-dummy table." << endl;
    return NDBT_FAILED;
  }

  // restore from backup, data only.
  // backup contains data for #sql-dummy, which should 
  // cause an error as the table doesn't exist, but will 
  // not cause an error as the default value for 
  // --exclude-intermediate-sql-tables is 1
  if (backup.restore(backupId, false) != 0) {
    g_err << "Failed to restore from backup." << endl;
    return NDBT_FAILED;
  }

  return NDBT_OK;
}   

int
runBug19202654(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbBackup backup;
  NdbDictionary::Dictionary* const dict = GETNDB(step)->getDictionary();

  g_err << "Creating 35 ndb tables." << endl;
  for(int i=0; i<35; i++)
  {
    char tablename[10];
    sprintf(tablename, "t%d", i);
    const char *colname = "id";
    NdbDictionary::Table tab;
    NdbDictionary::Index idx;

    if(makeTmpTable(tab, idx, tablename, colname) == -1) {
      g_err << "Failed to validate table " << tablename << ", error: " << dict->getNdbError() << endl;
      return NDBT_FAILED;
    }
    // Create large number of dictionary objects
    if (dict->createTable(tab) == -1) {
      g_err << "Failed to create table " << tablename << ", error: " << dict->getNdbError() << endl;
      return NDBT_FAILED;
    }
    // Create an index per table to double the number of dictionary objects
    if (dict->createIndex(idx) == -1) {
      g_err << "Failed to create index, error: " << dict->getNdbError() << endl;
      return NDBT_FAILED;
    }
  }
  
  g_err << "Starting backup." << endl;
  unsigned backupId = 0;
  if (backup.start(backupId) == -1) {
    g_err << "Failed to start backup." << endl;
    return NDBT_FAILED;
  }

  g_err << "Dropping 35 ndb tables." << endl;
  for(int i=0; i<35; i++)
  {
    char tablename[10];
    sprintf(tablename, "t%d", i);
    if (dict->dropTable(tablename) == -1) {
      g_err << "Failed to drop table " << tablename << ", error: " << dict->getNdbError() << endl;
      return NDBT_FAILED;
    }
  }

  g_err << "Restoring from backup with error insert and no metadata or data restore." << endl;
  // just load metadata and exit
  if (backup.restore(backupId, false, false, 1) != 0) {
    g_err << "Failed to restore from backup." << endl;
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

NDBT_TESTSUITE(testBackup);
TESTCASE("BackupOne", 
	 "Test that backup and restore works on one table \n"
	 "1. Load table\n"
	 "2. Backup\n"
	 "3. Drop tables and restart \n"
	 "4. Restore\n"
	 "5. Verify count and content of table\n"){
  INITIALIZER(clearOldBackups);
  INITIALIZER(runLoadTable);
  INITIALIZER(runBackupOne);
  INITIALIZER(runDropTablesRestart);
  INITIALIZER(runRestoreOne);
  VERIFIER(runVerifyOne);
  FINALIZER(runClearTable);
}
TESTCASE("BackupWhenOutOfLDMRecords",
      "Test that backup works also when we have no LDM records available\n")
{
  INITIALIZER(outOfLDMRecords);
  FINALIZER(runClearTable);
}
TESTCASE("BackupRandom", 
	 "Test that backup n and restore works on one table \n"
	 "1. Load table\n"
	 "2. Backup\n"
	 "3. Drop tables and restart \n"
	 "4. Restore\n"
	 "5. Verify count and content of table\n"){
  INITIALIZER(clearOldBackups);
  INITIALIZER(runLoadTable);
  INITIALIZER(runBackupRandom);
  INITIALIZER(runDropTablesRestart);
  INITIALIZER(runRestoreOne);
  VERIFIER(runVerifyOne);
  FINALIZER(runClearTable);
}
TESTCASE("BackupDDL", 
	 "Test that backup and restore works on with DDL ongoing\n"
	 "1. Backups and DDL (create,drop,table.index)"){
  INITIALIZER(clearOldBackups);
  INITIALIZER(runLoadTable);
  STEP(runBackupLoop);
  STEP(runDDL);
  STEP(runDDL);
  FINALIZER(runClearTable);
}
TESTCASE("BackupBank", 
	 "Test that backup and restore works during transaction load\n"
	 " by backing up the bank"
	 "1.  Create bank\n"
	 "2a. Start bank and let it run\n"
	 "2b. Perform loop number of backups of the bank\n"
	 "    when backups are finished tell bank to close\n"
	 "3.  Restart ndb -i and reload each backup\n"
	 "    let bank verify that the backup is consistent\n"
	 "4.  Drop bank\n"){
  INITIALIZER(clearOldBackups);
  INITIALIZER(runCreateBank);
  STEP(runBankTimer);
  STEP(runBankTransactions);
  STEP(runBankTransactions);
  STEP(runBankTransactions);
  STEP(runBankTransactions);
  STEP(runBankTransactions);
  STEP(runBankTransactions);
  STEP(runBankTransactions);
  STEP(runBankTransactions);
  STEP(runBankTransactions);
  STEP(runBankTransactions);
  STEP(runBankGL);
  // TODO  STEP(runBankSum);
  STEP(runBackupBank);
  VERIFIER(runRestoreBankAndVerify);
  FINALIZER(runDropBank);
}
TESTCASE("BackupUndoLog", 
	 "Test for backup happen at start time\n"
	 "1. Load table\n"
	 "2. Start backup with wait started\n"
	 "3. Insert, delete, update data during backup\n"
	 "4. Drop tables and restart \n"
	 "5. Restore\n"
	 "6. Verify records of table\n"
	 "7. Clear tables\n"){
  INITIALIZER(runLoadTable);
  INITIALIZER(runBackupUndoWaitStarted);
  INITIALIZER(runChangeUndoDataDuringBackup);
  INITIALIZER(runDropTablesRestart);
  INITIALIZER(runRestoreOne);
  VERIFIER(runVerifyUndoData);
  FINALIZER(runClearTable);
}
TESTCASE("NFMaster", 
	 "Test that backup behaves during node failiure\n"){
  INITIALIZER(clearOldBackups);
  INITIALIZER(setMaster);
  STEP(runAbort);

}
TESTCASE("NFMasterAsSlave", 
	 "Test that backup behaves during node failiure\n"){
  INITIALIZER(clearOldBackups);
  INITIALIZER(setMasterAsSlave);
  STEP(runAbort);

}
TESTCASE("NFSlave", 
	 "Test that backup behaves during node failiure\n"){
  INITIALIZER(clearOldBackups);
  INITIALIZER(setSlave);
  STEP(runAbort);

}
TESTCASE("FailMaster", 
	 "Test that backup behaves during node failiure\n"){
  INITIALIZER(clearOldBackups);
  INITIALIZER(setMaster);
  STEP(runFail);

}
TESTCASE("FailMasterAsSlave", 
	 "Test that backup behaves during node failiure\n"){
  INITIALIZER(clearOldBackups);
  INITIALIZER(setMasterAsSlave);
  STEP(runFail);

}
TESTCASE("FailSlave", 
	 "Test that backup behaves during node failiure\n"){
  INITIALIZER(clearOldBackups);
  INITIALIZER(setSlave);
  STEP(runFail);

}
TESTCASE("Bug57650", "")
{
  INITIALIZER(runBug57650);
}
TESTCASE("Bug14019036", "")
{
  INITIALIZER(runBug14019036);
}
TESTCASE("OutOfScanRecordsInLDM",
         "Test that uses up all scan slots before starting backup")
{
  INITIALIZER(createOrderedPkIndex);
  INITIALIZER(runLoadTable10000);
  INITIALIZER(outOfScanRecordsInLDM);
}
TESTCASE("Bug16656639", "")
{
  INITIALIZER(runBug16656639);
}
TESTCASE("Bug17882305", "")
{
  INITIALIZER(runBug17882305);
}
TESTCASE("Bug19202654", 
         "Test restore with a large number of tables")
{
  INITIALIZER(runBug19202654);
}
NDBT_TESTSUITE_END(testBackup);

int main(int argc, const char** argv){
  ndb_init();
  NDBT_TESTSUITE_INSTANCE(testBackup);
  return testBackup.execute(argc, argv);
}


