/*
   Copyright (c) 2003, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include <NDBT.hpp>
#include <NDBT_Test.hpp>
#include <HugoTransactions.hpp>
#include <UtilTransactions.hpp>
#include <NdbBackup.hpp>
#include <NdbMgmd.hpp>
#include <signaldata/DumpStateOrd.hpp>
#include <NdbHistory.hpp>

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

  if (ctx->getProperty("SnapshotStart") == 0)
  {
    if (backup.start(backupId) == -1)
    {
      return NDBT_FAILED;
    }
  }
  else
  {
    if (backup.start(backupId,
                     2,  // Wait for backup completion
                     0,
                     1) == -1)
    {
      return NDBT_FAILED;
    }
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




class DbVersion
{
public:
  NdbHistory::Version* m_version;
  
  DbVersion(): m_version(NULL)
  {};

  ~DbVersion()
  {
    delete m_version;
    m_version = NULL;
  }
};

/**
 * readVersionForRange
 * 
 * Method will read version info for the given range
 * from the database
 * Each logical row in the range is read by PK, and
 * its values (if present) are checked using
 * Hugo.
 * The passed in Version object is modified to describe
 * the versions present
 */
int readVersionForRange(Ndb* pNdb,
                        const NdbDictionary::Table* table,
                        const RecordRange range,
                        DbVersion& dbVersion)
{
  assert(range.m_len > 0);
  
  /* Initialise Version */
  dbVersion.m_version = new NdbHistory::Version(range);
  
  HugoCalculator hugoCalc(*table);
  
  for (Uint32 i=0; i < range.m_len; i++)
  {
    HugoOperations hugoOps(*table);
    const Uint32 r = range.m_start + i;
    NdbHistory::RecordState* recordState = dbVersion.m_version->m_states + i;
    
    if (hugoOps.startTransaction(pNdb) != 0)
    {
      g_err << "Failed to start transaction " << hugoOps.getNdbError() << endl;
      return NDBT_FAILED;
    }
  
    if (hugoOps.pkReadRecord(pNdb,
                             r,
                             1) != 0)
    {
      g_err << "Failed to define read " << hugoOps.getNdbError() << endl;
    }

    bool exists = true;
    
    int execError = hugoOps.execute_Commit(pNdb);
    if (execError != 0)
    {
      if (execError == 626)
      {
        /* Row does not exist */
        exists = false;
      }
      else
      {
        g_err << "Failed to execute pk read" << hugoOps.getNdbError() << endl;
        return NDBT_FAILED;
      }
    }

    if (exists)
    {
      NDBT_ResultRow& row = hugoOps.get_row(0);
      
      /* Check row itself */
      if (hugoCalc.verifyRowValues(&row) != 0)
      {
        g_err << "Row inconsistent at record " << r << endl;
        return NDBT_FAILED;
      }
      
      recordState->m_state = NdbHistory::RecordState::RS_EXISTS;
      recordState->m_updatesValue = hugoCalc.getUpdatesValue(&row);
    }
    else
    {
      recordState->m_state = NdbHistory::RecordState::RS_NOT_EXISTS;
      recordState->m_updatesValue = 0;
    }

    hugoOps.closeTransaction(pNdb);
  }

  return NDBT_OK;
}




// TODO 
//   Test restore epoch
//     Currently seems atrt has a problem with
//     ndb_apply_status not existing
//
//   Error insert for stalled GCI
//     Improve from timing-based testing
//
//   Vary transaction size
//   Vary ordering as pk order == insert order == page order?
//
//   Make debug logging more configurable

  
/* Used to subdivide range amongst steps */  
static WorkerIdentifier g_workers;  

int
initWorkerIds(NDBT_Context* ctx, NDBT_Step* step)
{
  const Uint32 numWorkers = ctx->getProperty("NumWorkers");
  g_workers.init(numWorkers);
  return NDBT_OK;
}


/* Set of version histories recorded for later verification */
static MutexVector<NdbHistory*>* g_rangeHistories = NULL;

int
initHistoryList(NDBT_Context* ctx, NDBT_Step* step)
{
  assert(g_rangeHistories == NULL);
  g_rangeHistories = new MutexVector<NdbHistory*>();
  
  return NDBT_OK;
}

int
clearHistoryList(NDBT_Context* ctx, NDBT_Step* step)
{
  if (g_rangeHistories != NULL)
  {
    delete g_rangeHistories;
    g_rangeHistories = NULL;
  }

  return NDBT_OK;
}
      
int
runUpdatesWithHistory(NDBT_Context* ctx, NDBT_Step* step)
{
  /**
   * Iterate over a range of records, applying updates 
   * + increasing the updates value, recording the changes
   * in a History until told to stop.
   */
  Ndb* pNdb = GETNDB(step);
  const Uint32 totalRecords = (Uint32) ctx->getNumRecords();
  const Uint32 stepNo = step->getStepNo();
  const Uint32 workerId = g_workers.getNextWorkerId();
  Uint32 maxTransactionSize = ctx->getProperty("MaxTransactionSize");
  const bool AdjustRangeOverTime = ctx->getProperty("AdjustRangeOverTime");

  if (totalRecords < g_workers.getTotalWorkers())
  {
    g_err << "Too few records " 
          << totalRecords 
          << " / " 
          << g_workers.getTotalWorkers()
          << endl;
    return NDBT_FAILED;
  }
  if (maxTransactionSize == 0)
  {
    maxTransactionSize = 1;
  }
  
  /* Determine my subrange */
  const Uint32 numRecords = totalRecords / g_workers.getTotalWorkers();
  const Uint32 startRecord = workerId * numRecords;
  const Uint32 endRecord = startRecord + numRecords;

  const RecordRange range(startRecord, numRecords);
  
  /**
   * Create a history and add it to the list for verification
   * I am interested in GCI boundary states for this
   * test, no need to record more than that unless debugging
   */
  NdbHistory* history = 
    new NdbHistory(NdbHistory::GR_LATEST_GCI, /* Record latest + Gcis */
                   range);
  g_rangeHistories->push_back(history);
  
  Uint32 recId = startRecord;
  Uint32 updatesVal = 1;
  
  g_err << stepNo << " : runUpdatesWithHistory AdjustRangeOverTime " 
        << AdjustRangeOverTime << endl;
  g_err << stepNo << " : running updates on range " 
        << startRecord
        << " -> "
        << endRecord
        << endl;

  Uint64 totalUpdates = 0;
  Uint64 lastCommitGci = 0;
  Uint32 recordLimit = endRecord;

  if (AdjustRangeOverTime)
  {
    /* Start small, build up range over time */
    recordLimit = startRecord + 1;
  }

  /* A version which we will use to describe our changes */
  NdbHistory::Version transaction(range);

  /* Initial version reflects the 'table load' step */
  transaction.setRows(startRecord,
                      0,
                      numRecords);
  history->commitVersion(&transaction,
                         0); /* Dummy commit epoch */

  
  while(ctx->isTestStopped() == false &&
        ctx->getProperty("StopUpdates") == 0)
  {
    HugoOperations hugoOps(*ctx->getTab());
    if (hugoOps.startTransaction(pNdb) != 0)
    {
      g_err << "Failed to start transaction " << hugoOps.getNdbError() << endl;
      return NDBT_FAILED;
    }
    
    /* Vary transaction size... */
    Uint32 recordsInTrans = 1;
    if (maxTransactionSize > 1)
    {
      const Uint32 remain = (recordLimit - recId) -1;
      if (remain)
      {
        recordsInTrans += (rand() % remain);
      }
    }
    
    if (hugoOps.pkUpdateRecord(pNdb,
                               recId,
                               recordsInTrans,
                               updatesVal) != 0)
    {
      g_err << "Failed to define PK updates " << hugoOps.getNdbError() << endl;
      return NDBT_FAILED;
    }
    transaction.setRows(recId, updatesVal, recordsInTrans);

    recId+= recordsInTrans;
    totalUpdates += recordsInTrans;

    if (hugoOps.execute_Commit(pNdb) != 0)
    {
      g_err << "Failed to commit pk updates " << hugoOps.getNdbError() << endl;
      return NDBT_FAILED;
    }

    Uint64 commitGci;
    hugoOps.getTransaction()->getGCI(&commitGci);
    if (commitGci == ~Uint64(0))
    {
      g_err << "Failed to get commit epoch " << endl;
      return NDBT_FAILED;
    }

    /* Update history with the committed version */
    history->commitVersion(&transaction,
                          commitGci);


    if (AdjustRangeOverTime &&
        commitGci != lastCommitGci)
    {
      /**
       * We use observed epoch increments to track
       * the passage of time, and increase the updatesValue
       * TODO : Use actual time to reduce confusion / coupling
       */
      recordLimit++;
      if (recordLimit == endRecord)
      {
        recordLimit = startRecord + 1;
      }
      if((recordLimit % 100) == 0)
      {
        g_err << stepNo << " : range upperbound moves to " << recordLimit << endl;
      }
    }

    lastCommitGci = commitGci;
    
    hugoOps.closeTransaction(pNdb);

    if (recId >= recordLimit)
    {
      recId = startRecord;
      updatesVal++;
      if ((updatesVal % 100) == 0)
      {
        g_err << stepNo << " : updates value moves to " << updatesVal << endl;
      }
    }
  }

  g_err << stepNo << " : finished after " << totalUpdates 
        << " updates applied" << endl;

  g_err << stepNo << " : history summary " << endl;

  history->dump();

  return NDBT_OK;
}

int
runDelayedBackup(NDBT_Context* ctx, NDBT_Step* step)
{
  /**
   * Idea is to have a backup while other activity
   * is occurring in the cluster
   * Plan : 
   *   Wait a while
   *   Run a backup
   *   Wait a while
   */
  const Uint32 stepNo = step->getStepNo();

  g_err << stepNo << " : runDelayedBackup" << endl;
  g_err << stepNo << " : sleeping a while" << endl;

  NdbSleep_SecSleep(3);

  g_err << stepNo << " : starting a backup" << endl;
  
  if (runBackupOne(ctx, step) != NDBT_OK)
  {
    return NDBT_FAILED;
  }

  g_err << stepNo << " : backup completed" << endl;
  
  g_err << stepNo << " : sleeping a while" << endl;
  
  NdbSleep_SecSleep(3);

  /* Stop updates now */
  g_err << stepNo << " : stopping updates" << endl;
  
  ctx->setProperty("StopUpdates", 1);
  
  g_err << stepNo << " : done" << endl;

  return NDBT_OK;
}


int
verifyDbVsHistories(NDBT_Context* ctx, NDBT_Step* step)
{
  /**
   * This VERIFIER step takes a set of range-histories
   * produced by earlier steps, and for each 
   * range-history:
   *  - Reads the current data for the range from the db.
   *  - Checks row-level self consistency using HugoCalc
   *    and determines the row's logical 'update value'
   *  - Searches the range history for matching versions
   *    according to the update values.
   *  - Maps the matching versions in the range-history
   *    to a set of matching commit epoch ranges
   *
   * Then each range-history's matching epoch ranges 
   * are compared to the other range-history's matching
   * epoch ranges to find a common set of epoch ranges
   * which are present in each range-history.
   *
   * Finally, the common set of epoch ranges is checked 
   * to ensure that is describes a consistent GCI 
   * boundary.
   *
   * More visually:
   * 
   *      Range1           Range2       ...       RangeN
   *
   * (Earlier step)
   *   
   *     Update DB        Update DB              Update DB
   *     and History      and History            and History
   *     /    \           /    \                 /    \
   *    DB   History     DB   History           DB   History
   *     |     |          |     |                |     |            
   *     |     |          |     |                |     |
   *     *     .          *     .                *     .
   *     *     .          *     .                *     .
   *     *     .          *     .                *     .
   *     |     |          |     |                |     |
   *     |     |          |     |                |     |
   * (Verifier)|          |     |                |     |
   *    DB   History     DB   History            DB   History
   *     \    /           \    /                  \    /
   *       Find             Find                    Find
   *     matching         matching                matching
   *      epoch            epoch                   epoch
   *      ranges           ranges                  ranges
   *        |                |                       |
   *        ------------     |    ----- ... ----------
   *                    |    |    |
   *                  Find common epoch
   *                       ranges
   *                         |
   *                         |
   *                    Check for epoch
   *                   range representing
   *                        GCI
   *
   *  * Represents something interesting happening to the
   *    database which we want to verify the consistency of
   *
   *    Separate ranges exist to simplify testing with 
   *    multithreaded/concurrent modifications
   */
  // TODO : Pull out some of the EpochRangeSet juggling into
  // reusable code, as it is reused
  Ndb* pNdb = GETNDB(step);
  const NdbDictionary::Table* tab = pNdb->getDictionary()->getTable(tabname);
  Vector<EpochRangeSet> matchingEpochRangeSets;
  bool verifyOk = true;

  g_err << "verifyDbVsHistories" << endl;
  g_err << " : History count " << g_rangeHistories->size() << endl;
  
  for (Uint32 h=0; h < g_rangeHistories->size(); h++)
  {
    g_err << " : History " << h << endl;
    const NdbHistory& history = *(*g_rangeHistories)[h];
    
    g_err << " : Reading version info from DB for range "
          << history.m_range.m_start 
          << "->"
          << history.m_range.m_start + history.m_range.m_len
          << endl;

    DbVersion dbVersion;
    if (readVersionForRange(pNdb,
                            tab,
                            history.m_range,
                            dbVersion) != NDBT_OK)
    {
      verifyOk = false;
      continue;
    }

    g_err << " : searching for matching versions in history" << endl;

    EpochRangeSet epochRanges;
    NdbHistory::MatchingEpochRangeIterator mri(history,
                                               dbVersion.m_version);
    EpochRange er;
    while (mri.next(er))
    {
      epochRanges.addEpochRange(er);
    }

    const Uint32 rangeCount = epochRanges.m_ranges.size();

    g_err << " : found " << rangeCount
          << " matching version ranges." << endl;
    epochRanges.dump();

    if (rangeCount == 0)
    {
      g_err << " : No match found - failed" << endl;
      verifyOk = false;
      
      /* Debugging : Dump DB + History content for this range */
      g_err << " : DB VERSION : " << endl;
      dbVersion.m_version->dump(false, "    ");
      
      g_err << " : HISTORY VERSIONS : " << endl;
      history.dump(true);
      history.dumpClosestMatch(dbVersion.m_version);
      /* Continue with matching to get more info */
    }

    matchingEpochRangeSets.push_back(epochRanges);
  }

  if (!verifyOk)
  {
    /* Bail out now */
    return NDBT_FAILED;
  }

  g_err << " : checking that history matches agree on "
        << "common epochs" << endl;

  /**
   * Check that the matching epoch range[s] from each History
   * intersect on some common epoch range[s]
   */
  EpochRangeSet commonRanges(matchingEpochRangeSets[0]);
  
  for (Uint32 i=1; i < matchingEpochRangeSets.size(); i++)
  {
    commonRanges = 
      EpochRangeSet::intersect(commonRanges,
                               matchingEpochRangeSets[i]);
  }

  if (commonRanges.isEmpty())
  {
    g_err << "ERROR : No common epoch range between histories" << endl;
    verifyOk = false;
  }
  else
  {
    g_err << " : found "
          << commonRanges.m_ranges.size() 
          << " common epoch range[s] between histories" << endl;
    
    commonRanges.dump();

    g_err << " : checking that common range[s] span a GCI boundary" << endl;
    
    bool foundGciBoundary = false;
    for (Uint32 i=0; i < commonRanges.m_ranges.size(); i++)
    {
      const EpochRange& er = commonRanges.m_ranges[i];
      if (er.spansGciBoundary())
      {
        ndbout_c("  OK - found range spanning GCI boundary");
        er.dump();
        foundGciBoundary = true;
      }
    }
    
    if (!foundGciBoundary)
    {
      g_err << "ERROR : No common GCI boundary span found" << endl;
      verifyOk = false;
    }
  }

  return (verifyOk? NDBT_OK : NDBT_FAILED);
}
  
  


int
runGCPStallDuringBackup(NDBT_Context* ctx, NDBT_Step* step)
{
  const Uint32 stepNo = step->getStepNo();
  NdbRestarter restarter;

  g_err << stepNo << " : runGCPStallDuringBackup" << endl;
  
  /**
   * Plan is to stall backup scan, so that some time can
   * pass during the backup
   * We then wait to allow a number of GCIs to pass to
   * avoid Backup weirdness around 3 GCIs
   * We then cause GCP itself to stall
   * We then wait a little longer
   * We then unstall the backup scan and GCP stall
   */

  g_err << stepNo << " : stalling backup scan" << endl;
  const Uint32 StallBackupScanCode = 10039;  // BACKUP
  restarter.insertErrorInAllNodes(StallBackupScanCode);
  
  g_err << stepNo << " : waiting a while" << endl;

  /* TODO : Split backup into backup start + wait, and 
   * trigger this part on backup start
   */
  const Uint32 delay1Secs = 6 * 3;
  NdbSleep_SecSleep(delay1Secs);
  
  g_err << stepNo << " : stalling GCP" << endl;
  
  const Uint32 StallGCPSaveCode = 7237;  // DIH
  restarter.insertErrorInAllNodes(StallGCPSaveCode);

  g_err << stepNo << " : waiting a while" << endl;
  
  const Uint32 delay2Secs = 2 * 3;
  NdbSleep_SecSleep(delay2Secs);

  g_err << stepNo << " : Clearing error inserts" << endl;
  
  restarter.insertErrorInAllNodes(0);
  
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

static const int NumUpdateThreads = 5;

TESTCASE("ConsistencyUnderLoad",
         "Test backup SNAPSHOTEND consistency under load")
{
  TC_PROPERTY("AdjustRangeOverTime", Uint32(1));    // Written subparts of ranges change as updates run
  TC_PROPERTY("NumWorkers", Uint32(NumUpdateThreads));
  TC_PROPERTY("MaxTransactionSize", Uint32(100));
  INITIALIZER(clearOldBackups);
  INITIALIZER(runLoadTable);
  INITIALIZER(initWorkerIds);
  INITIALIZER(initHistoryList);

  STEPS(runUpdatesWithHistory, NumUpdateThreads);
  STEP(runDelayedBackup);

  VERIFIER(runDropTablesRestart);   // Drop tables
  VERIFIER(runRestoreOne);          // Restore backup
  VERIFIER(verifyDbVsHistories);    // Check restored data vs histories
// TODO : Check restore-epoch
  FINALIZER(clearHistoryList);
  FINALIZER(runClearTable);
}


TESTCASE("ConsistencyUnderLoadStallGCP",
         "Test backup consistency under load with GCP stall")
{
  TC_PROPERTY("AdjustRangeOverTime", Uint32(1));    // Written subparts of ranges change as updates run
  TC_PROPERTY("NumWorkers", Uint32(NumUpdateThreads));
  TC_PROPERTY("MaxTransactionSize", Uint32(2)); // Reduce test runtime
  INITIALIZER(clearOldBackups);
  INITIALIZER(runLoadTable);
  INITIALIZER(initWorkerIds);
  INITIALIZER(initHistoryList);

  STEPS(runUpdatesWithHistory, NumUpdateThreads);
  STEP(runDelayedBackup);
  STEP(runGCPStallDuringBackup);  // Backup adversary

  VERIFIER(runDropTablesRestart);   // Drop tables
  VERIFIER(runRestoreOne);          // Restore backup
  VERIFIER(verifyDbVsHistories);    // Check restored data vs histories
  FINALIZER(clearHistoryList);
  FINALIZER(runClearTable);
}


// Disabled pending fix for  Bug #27566346 NDB : BACKUP WITH SNAPSHOTSTART CONSISTENCY ISSUES
// TESTCASE("ConsistencyUnderLoadSnapshotStart",
//          "Test backup SNAPSHOTSTART consistency under load")
// {
// }
// TESTCASE("ConsistencyUnderLoadSnapshotStartStallGCP",
//          "Test backup consistency under load with GCP stall")
// {
// }



NDBT_TESTSUITE_END(testBackup);

int main(int argc, const char** argv){
  ndb_init();
  NDBT_TESTSUITE_INSTANCE(testBackup);
  return testBackup.execute(argc, argv);
}


