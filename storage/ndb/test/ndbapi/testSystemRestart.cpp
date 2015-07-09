/*
   Copyright (c) 2003, 2015, Oracle and/or its affiliates. All rights reserved.

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
#include <NdbRestarter.hpp>
#include <Vector.hpp>
#include <signaldata/DumpStateOrd.hpp>
#include <NdbBackup.hpp>
#include <Bitmask.hpp>
#include <DbUtil.hpp>

int runLoadTable(NDBT_Context* ctx, NDBT_Step* step){

  int records = ctx->getNumRecords();
  HugoTransactions hugoTrans(*ctx->getTab());
  if (hugoTrans.loadTable(GETNDB(step), records) != 0){
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int runFillTable(NDBT_Context* ctx, NDBT_Step* step){
  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Table tab(*ctx->getTab());

  /* fill table until its full */
  HugoTransactions hugoTrans(tab);
  if(hugoTrans.fillTable(pNdb) != 0){
    return NDBT_FAILED;
  }

  /* store the number of rows */
  int cnt;
  UtilTransactions utilTrans(tab);
  if(utilTrans.selectCount(pNdb, 0, &cnt) != 0){
    g_err << "Select count failed." << endl;
    return NDBT_FAILED;
  }
  ctx->setProperty("recordCount", cnt);
  return NDBT_OK;
}

int runVerifyFilledTables(NDBT_Context* ctx, NDBT_Step* step)
{
  /* verify the number of rows is intact */
  Ndb* pNdb = GETNDB(step);
  int countOld= ctx->getProperty("recordCount");
  if (countOld == 0){
    /* table was not filled using fillTable */
    g_err << "Table initial row count not available" << endl;
    return NDBT_FAILED;
  }
  /* ctx's tab gets invalidated in alter table reorganize partition
          Hence reloading table again to verify */
  const char *tableName= ctx->getTableName(0);
  const NdbDictionary::Table* pTab =
      NDBT_Table::discoverTableFromDb(pNdb, tableName);
  if (pTab == NULL){
    g_err << tableName << " was lost during the test." << endl;
    return NDBT_FAILED;
  }

  /* compare new record count with old */
  int cnt;
  UtilTransactions utilTrans(*pTab);
  if(utilTrans.selectCount(pNdb, 0, &cnt) != 0){
    g_err << "Select count failed." << endl;
    return NDBT_FAILED;
  }
  if(cnt != countOld){
    g_err << "Number of rows in result table different from expected" << endl;
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int
clearOldBackups(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbBackup backup;
  backup.clearOldBackups();
  return NDBT_OK;
}

#define CHECK(b) if (!(b)) { \
  g_err << "ERR: "<< step->getName() \
         << " failed on line " << __LINE__ << endl; \
  result = NDBT_FAILED; \
  continue; } 

int runSystemRestart1(NDBT_Context* ctx, NDBT_Step* step){
  Ndb* pNdb = GETNDB(step);
  int result = NDBT_OK;
  int timeout = 300;
  Uint32 loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  int count;
  NdbRestarter restarter;
  Uint32 i = 1;

  UtilTransactions utilTrans(*ctx->getTab());
  HugoTransactions hugoTrans(*ctx->getTab());
  while(i<=loops && result != NDBT_FAILED){

    ndbout << "Loop " << i << "/"<< loops <<" started" << endl;
    /*
      1. Load data
      2. Restart cluster and verify records
      3. Update records
      4. Restart cluster and verify records
      5. Delete half of the records
      6. Restart cluster and verify records
      7. Delete all records
      8. Restart cluster and verify records
      9. Insert, update, delete records
      10. Restart cluster and verify records
      11. Insert, update, delete records
      12. Restart cluster with error insert 5020 and verify records
    */
    ndbout << "Loading records..." << endl;
    CHECK(hugoTrans.loadTable(pNdb, records) == 0);

    ndbout << "Restarting cluster" << endl;
    CHECK(restarter.restartAll() == 0);
    CHECK(restarter.waitClusterStarted(timeout) == 0);
    CHECK(pNdb->waitUntilReady(timeout) == 0);

    ndbout << "Verifying records..." << endl;
    CHECK(hugoTrans.pkReadRecords(pNdb, records) == 0);
    CHECK(utilTrans.selectCount(pNdb, 64, &count) == 0);
    CHECK(count == records);

    ndbout << "Updating records..." << endl;
    CHECK(hugoTrans.pkUpdateRecords(pNdb, records) == 0);

    ndbout << "Restarting cluster..." << endl;
    CHECK(restarter.restartAll() == 0);
    CHECK(restarter.waitClusterStarted(timeout) == 0);
    CHECK(pNdb->waitUntilReady(timeout) == 0);

    ndbout << "Verifying records..." << endl;
    CHECK(hugoTrans.pkReadRecords(pNdb, records) == 0);
    CHECK(utilTrans.selectCount(pNdb, 64, &count) == 0);
    CHECK(count == records);
    
    ndbout << "Deleting 50% of records..." << endl;
    CHECK(hugoTrans.pkDelRecords(pNdb, records/2) == 0);

    ndbout << "Restarting cluster..." << endl;
    CHECK(restarter.restartAll() == 0);
    CHECK(restarter.waitClusterStarted(timeout) == 0);
    CHECK(pNdb->waitUntilReady(timeout) == 0);

    ndbout << "Verifying records..." << endl;
    CHECK(hugoTrans.scanReadRecords(pNdb, records/2, 0, 64) == 0);
    CHECK(utilTrans.selectCount(pNdb, 64, &count) == 0);
    CHECK(count == (records/2));

    ndbout << "Deleting all records..." << endl;
    CHECK(utilTrans.clearTable(pNdb, records/2) == 0);

    ndbout << "Restarting cluster..." << endl;
    CHECK(restarter.restartAll() == 0);
    CHECK(restarter.waitClusterStarted(timeout) == 0);
    CHECK(pNdb->waitUntilReady(timeout) == 0);

    ndbout << "Verifying records..." << endl;
    CHECK(utilTrans.selectCount(pNdb, 64, &count) == 0);
    CHECK(count == 0);

    ndbout << "Doing it all..." << endl;
    CHECK(hugoTrans.loadTable(pNdb, records) == 0);
    CHECK(hugoTrans.pkUpdateRecords(pNdb, records) == 0);
    CHECK(hugoTrans.pkDelRecords(pNdb, records/2) == 0);
    CHECK(hugoTrans.scanUpdateRecords(pNdb, records/2) == 0);
    CHECK(utilTrans.clearTable(pNdb, records) == 0);
    CHECK(hugoTrans.loadTable(pNdb, records) == 0);
    CHECK(utilTrans.clearTable(pNdb, records) == 0);
    CHECK(hugoTrans.loadTable(pNdb, records) == 0);
    CHECK(hugoTrans.pkUpdateRecords(pNdb, records) == 0);
    CHECK(utilTrans.clearTable(pNdb, records) == 0);

    ndbout << "Restarting cluster..." << endl;
    CHECK(restarter.restartAll() == 0);
    CHECK(restarter.waitClusterStarted(timeout) == 0);
    CHECK(pNdb->waitUntilReady(timeout) == 0);

    ndbout << "Verifying records..." << endl;
    CHECK(utilTrans.selectCount(pNdb, 64, &count) == 0);
    CHECK(count == 0);

    ndbout << "Doing it all..." << endl;
    CHECK(hugoTrans.loadTable(pNdb, records) == 0);
    CHECK(hugoTrans.pkUpdateRecords(pNdb, records) == 0);
    CHECK(hugoTrans.pkDelRecords(pNdb, records/2) == 0);
    CHECK(hugoTrans.scanUpdateRecords(pNdb, records/2) == 0);
    CHECK(utilTrans.clearTable(pNdb, records) == 0);
    CHECK(hugoTrans.loadTable(pNdb, records) == 0);
    CHECK(utilTrans.clearTable(pNdb, records) == 0);

    ndbout << "Restarting cluster with error insert 5020..." << endl;
    CHECK(restarter.restartAll(false, true) == 0);
    CHECK(restarter.waitClusterNoStart(timeout) == 0);
    CHECK(restarter.insertErrorInAllNodes(5020) == 0);
    CHECK(restarter.startAll() == 0);
    CHECK(restarter.waitClusterStarted(timeout) == 0);
    CHECK(pNdb->waitUntilReady(timeout) == 0);
    
    i++;
  }

  ndbout << "runSystemRestart1 finished" << endl;  

  return result;
}

int runSystemRestart2(NDBT_Context* ctx, NDBT_Step* step){
  Ndb* pNdb = GETNDB(step);
  int result = NDBT_OK;
///  int timeout = 300;
  int timeout = 120;
  Uint32 loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  int count;
  NdbRestarter restarter;
  Uint32 i = 1;

  UtilTransactions utilTrans(*ctx->getTab());
  HugoTransactions hugoTrans(*ctx->getTab());
  while(i<=loops && result != NDBT_FAILED && !ctx->isTestStopped()){

    ndbout << "Loop " << i << "/"<< loops <<" started" << endl;
    /* Use error 7070 to set time between LCP to it's min value
      1. Load data
      2. Restart cluster and verify records
      3. Update records
      4. Restart cluster and verify records
      5. Delete half of the records
      6. Restart cluster and verify records
      7. Delete all records
      8. Restart cluster and verify records
      9. Insert, update, delete records
      10. Restart cluster and verify records
    */
    int val = DumpStateOrd::DihMinTimeBetweenLCP;
    CHECK(restarter.dumpStateAllNodes(&val, 1) == 0);

    ndbout << "Loading records..." << endl;
    CHECK(hugoTrans.loadTable(pNdb, records) == 0);

    ndbout << "Restarting cluster" << endl;
    CHECK(restarter.restartAll() == 0);
    CHECK(restarter.waitClusterStarted(timeout) == 0);
    {
      int val = DumpStateOrd::DihMinTimeBetweenLCP;
      CHECK(restarter.dumpStateAllNodes(&val, 1) == 0);
    }
    CHECK(pNdb->waitUntilReady(timeout) == 0);

    ndbout << "Verifying records..." << endl;
    CHECK(hugoTrans.pkReadRecords(pNdb, records) == 0);
    CHECK(utilTrans.selectCount(pNdb, 64, &count) == 0);
    CHECK(count == records);

    ndbout << "Updating records..." << endl;
    CHECK(hugoTrans.pkUpdateRecords(pNdb, records) == 0);

    ndbout << "Restarting cluster..." << endl;
    CHECK(restarter.restartAll() == 0);
    CHECK(restarter.waitClusterStarted(timeout) == 0);
    {
      int val = DumpStateOrd::DihMinTimeBetweenLCP;
      CHECK(restarter.dumpStateAllNodes(&val, 1) == 0);
    }
    CHECK(pNdb->waitUntilReady(timeout) == 0);

    ndbout << "Verifying records..." << endl;
    CHECK(hugoTrans.pkReadRecords(pNdb, records) == 0);
    CHECK(utilTrans.selectCount(pNdb, 64, &count) == 0);
    CHECK(count == records);
    
    ndbout << "Deleting 50% of records..." << endl;
    CHECK(hugoTrans.pkDelRecords(pNdb, records/2) == 0);

    ndbout << "Restarting cluster..." << endl;
    CHECK(restarter.restartAll() == 0);
    CHECK(restarter.waitClusterStarted(timeout) == 0);
    {
      int val = DumpStateOrd::DihMinTimeBetweenLCP;
      CHECK(restarter.dumpStateAllNodes(&val, 1) == 0);
    }
    CHECK(pNdb->waitUntilReady(timeout) == 0);

    ndbout << "Verifying records..." << endl;
    CHECK(hugoTrans.scanReadRecords(pNdb, records/2, 0, 64) == 0);
    CHECK(utilTrans.selectCount(pNdb, 64, &count) == 0);
    CHECK(count == (records/2));

    ndbout << "Deleting all records..." << endl;
    CHECK(utilTrans.clearTable(pNdb, records/2) == 0);

    ndbout << "Restarting cluster..." << endl;
    CHECK(restarter.restartAll() == 0);
    CHECK(restarter.waitClusterStarted(timeout) == 0);
    {
      int val = DumpStateOrd::DihMinTimeBetweenLCP;
      CHECK(restarter.dumpStateAllNodes(&val, 1) == 0);
    }
    CHECK(pNdb->waitUntilReady(timeout) == 0);

    ndbout << "Verifying records..." << endl;
    CHECK(utilTrans.selectCount(pNdb, 64, &count) == 0);
    CHECK(count == 0);

    ndbout << "Doing it all..." << endl;
    CHECK(hugoTrans.loadTable(pNdb, records) == 0);
    CHECK(hugoTrans.pkUpdateRecords(pNdb, records) == 0);
    CHECK(hugoTrans.pkDelRecords(pNdb, records/2) == 0);
    CHECK(hugoTrans.scanUpdateRecords(pNdb, records/2) == 0);
    CHECK(utilTrans.clearTable(pNdb, records) == 0);
    CHECK(hugoTrans.loadTable(pNdb, records) == 0);
    CHECK(utilTrans.clearTable(pNdb, records) == 0);
    CHECK(hugoTrans.loadTable(pNdb, records) == 0);
    CHECK(hugoTrans.pkUpdateRecords(pNdb, records) == 0);
    CHECK(utilTrans.clearTable(pNdb, records) == 0);

    ndbout << "Restarting cluster..." << endl;
    CHECK(restarter.restartAll() == 0);
    CHECK(restarter.waitClusterStarted(timeout) == 0);
    {
      int val = DumpStateOrd::DihMinTimeBetweenLCP;
      CHECK(restarter.dumpStateAllNodes(&val, 1) == 0);
    }
    CHECK(pNdb->waitUntilReady(timeout) == 0);

    ndbout << "Verifying records..." << endl;
    CHECK(utilTrans.selectCount(pNdb, 64, &count) == 0);
    CHECK(count == 0);

    i++;
  }

  ndbout << "runSystemRestart2 finished" << endl;  

  return result;
}

int runSystemRestartTestUndoLog(NDBT_Context* ctx, NDBT_Step* step){
  Ndb* pNdb = GETNDB(step);
  int result = NDBT_OK;
  int timeout = 300;
  Uint32 loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  int count;
  NdbRestarter restarter;
  Uint32 i = 1;

  int dump7080[2];
  dump7080[0] = 7080;
  dump7080[1] = ctx->getTab()->getTableId();

  UtilTransactions utilTrans(*ctx->getTab());
  HugoTransactions hugoTrans(*ctx->getTab());
  while(i<=loops && result != NDBT_FAILED){

    ndbout << "Loop " << i << "/"<< loops <<" started" << endl;
    /*
      1. Start LCP, turn on undologging but delay write of datapages.
      2. Insert, update, delete records
      3. Complete writing of data pages and finish LCP.
      4. Restart cluster and verify records
    */
    // Use dump state 7080 to delay writing of datapages
    // for the current table
    ndbout << "Dump state: "<<dump7080[0]<<", "<<dump7080[1]<<endl;
    CHECK(restarter.dumpStateAllNodes(dump7080, 2) == 0);    
    NdbSleep_SecSleep(10);

    ndbout << "Doing it all..." << endl;
    CHECK(hugoTrans.loadTable(pNdb, records) == 0);
    CHECK(hugoTrans.pkUpdateRecords(pNdb, records) == 0);
    CHECK(hugoTrans.pkDelRecords(pNdb, records/2) == 0);
    CHECK(hugoTrans.scanUpdateRecords(pNdb, records/2) == 0);
    CHECK(utilTrans.clearTable(pNdb, records) == 0);
    CHECK(hugoTrans.loadTable(pNdb, records) == 0);
    CHECK(utilTrans.clearTable(pNdb, records) == 0);

    // Reset error and let LCP continue
    CHECK(restarter.insertErrorInAllNodes(0) == 0);
    NdbSleep_SecSleep(60);

    ndbout << "Restarting cluster..." << endl;
    CHECK(restarter.restartAll() == 0);
    CHECK(restarter.waitClusterStarted(timeout) == 0);
    CHECK(pNdb->waitUntilReady(timeout) == 0);

    ndbout << "Verifying records..." << endl;
    CHECK(utilTrans.selectCount(pNdb, 64, &count) == 0);
    CHECK(count == 0);

    // Use dump state 7080 to delay writing of datapages
    // for the current table
    ndbout << "Dump state: "<<dump7080[0]<<", "<<dump7080[1]<<endl;
    CHECK(restarter.dumpStateAllNodes(dump7080, 2) == 0);
    NdbSleep_SecSleep(10);

    ndbout << "Doing it all, delete 50%..." << endl;
    CHECK(hugoTrans.loadTable(pNdb, records) == 0);
    CHECK(hugoTrans.pkUpdateRecords(pNdb, records) == 0);
    CHECK(hugoTrans.pkDelRecords(pNdb, records/2) == 0);

    // Reset error and let LCP continue
    CHECK(restarter.insertErrorInAllNodes(0) == 0);
    NdbSleep_SecSleep(20);

    ndbout << "Restarting cluster..." << endl;
    CHECK(restarter.restartAll() == 0);
    CHECK(restarter.waitClusterStarted(timeout) == 0);
    CHECK(pNdb->waitUntilReady(timeout) == 0);

    ndbout << "Verifying records..." << endl;
    CHECK(hugoTrans.scanReadRecords(pNdb, records/2, 0, 64) == 0);
    CHECK(utilTrans.selectCount(pNdb, 64, &count) == 0);
    CHECK(count == (records/2));
    CHECK(utilTrans.clearTable(pNdb, records) == 0);

    i++;
  }

  ndbout << "runSystemRestartTestUndoLog finished" << endl;  

  return result;
}

int runSystemRestartTestFullDb(NDBT_Context* ctx, NDBT_Step* step){
  Ndb* pNdb = GETNDB(step);
  int result = NDBT_OK;
  int timeout = 300;
  Uint32 loops = ctx->getNumLoops();
  int count1, count2;
  NdbRestarter restarter;
  Uint32 i = 1;

  UtilTransactions utilTrans(*ctx->getTab());
  HugoTransactions hugoTrans(*ctx->getTab());
  while(i<=loops && result != NDBT_FAILED){

    ndbout << "Loop " << i << "/"<< loops <<" started" << endl;
    /*
      1. Load data until db reports it's full
      2. Restart cluster and verify records
    */
    ndbout << "Filling up table..." << endl;
    CHECK(hugoTrans.fillTable(pNdb) == 0);
    CHECK(utilTrans.selectCount(pNdb, 64, &count1) == 0);
    ndbout << "Db is full. Table has "<<count1 <<" records."<< endl;
    
    ndbout << "Restarting cluster" << endl;
    CHECK(restarter.restartAll() == 0);
    CHECK(restarter.waitClusterStarted(timeout) == 0);
    CHECK(pNdb->waitUntilReady(timeout) == 0);

    ndbout << "Verifying records..." << endl;
    CHECK(hugoTrans.scanReadRecords(pNdb, count1) == 0);
    CHECK(utilTrans.selectCount(pNdb, 64, &count2) == 0);
    CHECK(count1 == count2);

    ndbout << "Deleting all records..." << endl;
    CHECK(utilTrans.clearTable2(pNdb, count1) == 0);

    ndbout << "Restarting cluster..." << endl;
    CHECK(restarter.restartAll() == 0);
    CHECK(restarter.waitClusterStarted(timeout) == 0);
    CHECK(pNdb->waitUntilReady(timeout) == 0);

    ndbout << "Verifying records..." << endl;
    CHECK(utilTrans.selectCount(pNdb, 64, &count1) == 0);
    CHECK(count1 == 0);

    i++;
  }

  ndbout << "runSystemRestartTestFullDb finished" << endl;  

  return result;
}

int runSystemRestart3(NDBT_Context* ctx, NDBT_Step* step){
  Ndb* pNdb = GETNDB(step);
  int result = NDBT_OK;
  int timeout = 300;
  Uint32 loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  int count;
  NdbRestarter restarter;
  Uint32 i = 1;

  const Uint32 nodeCount = restarter.getNumDbNodes();
  if(nodeCount < 2){
    g_info << "SR3 - Needs atleast 2 nodes to test" << endl;
    return NDBT_OK;
  }

  Vector<int> nodeIds;
  for(i = 0; i<nodeCount; i++)
    nodeIds.push_back(restarter.getDbNodeId(i));
  
  Uint32 currentRestartNodeIndex = 0;
  UtilTransactions utilTrans(*ctx->getTab());
  HugoTransactions hugoTrans(*ctx->getTab());
  
  while(i<=loops && result != NDBT_FAILED){
    
    g_info << "Loop " << i << "/"<< loops <<" started" << endl;
    /**
     * 1. Load data
     * 2. Restart 1 node -nostart
     * 3. Update records
     * 4. Restart cluster and verify records
     * 5. Restart 1 node -nostart
     * 6. Delete half of the records
     * 7. Restart cluster and verify records
     * 8. Restart 1 node -nostart
     * 9. Delete all records
     * 10. Restart cluster and verify records
     */
    g_info << "Loading records..." << endl;
    CHECK(hugoTrans.loadTable(pNdb, records) == 0);

    /*** 1 ***/
    g_info << "1 - Stopping one node" << endl;
    CHECK(restarter.restartOneDbNode(nodeIds[currentRestartNodeIndex],
				     false, 
				     true,
				     false) == 0);
    currentRestartNodeIndex = (currentRestartNodeIndex + 1 ) % nodeCount;

    g_info << "Updating records..." << endl;
    CHECK(hugoTrans.pkUpdateRecords(pNdb, records) == 0);
    
    g_info << "Restarting cluster..." << endl;
    CHECK(restarter.restartAll() == 0);
    CHECK(restarter.waitClusterStarted(timeout) == 0);
    CHECK(pNdb->waitUntilReady(timeout) == 0);

    g_info << "Verifying records..." << endl;
    CHECK(hugoTrans.pkReadRecords(pNdb, records) == 0);
    CHECK(utilTrans.selectCount(pNdb, 64, &count) == 0);
    CHECK(count == records);

    g_info << "2 - Stopping one node" << endl;
    CHECK(restarter.restartOneDbNode(nodeIds[currentRestartNodeIndex],
				     false, 
				     true,
				     false) == 0);
    currentRestartNodeIndex = (currentRestartNodeIndex + 1 ) % nodeCount;

    g_info << "Deleting 50% of records..." << endl;
    CHECK(hugoTrans.pkDelRecords(pNdb, records/2) == 0);
    
    g_info << "Restarting cluster..." << endl;
    CHECK(restarter.restartAll() == 0);
    CHECK(restarter.waitClusterStarted(timeout) == 0);
    CHECK(pNdb->waitUntilReady(timeout) == 0);

    g_info << "Verifying records..." << endl;
    CHECK(hugoTrans.scanReadRecords(pNdb, records/2, 0, 64) == 0);
    CHECK(utilTrans.selectCount(pNdb, 64, &count) == 0);
    CHECK(count == (records/2));

    g_info << "3 - Stopping one node" << endl;
    CHECK(restarter.restartOneDbNode(nodeIds[currentRestartNodeIndex],
				     false, 
				     true,
				     false) == 0);
    currentRestartNodeIndex = (currentRestartNodeIndex + 1 ) % nodeCount;
    g_info << "Deleting all records..." << endl;
    CHECK(utilTrans.clearTable(pNdb, records/2) == 0);

    g_info << "Restarting cluster..." << endl;
    CHECK(restarter.restartAll() == 0);
    CHECK(restarter.waitClusterStarted(timeout) == 0);
    CHECK(pNdb->waitUntilReady(timeout) == 0);
    
    ndbout << "Verifying records..." << endl;
    CHECK(utilTrans.selectCount(pNdb, 64, &count) == 0);
    CHECK(count == 0);
    
    i++;
  }

  g_info << "runSystemRestart3 finished" << endl;  

  return result;
}

int runSystemRestart4(NDBT_Context* ctx, NDBT_Step* step){
  Ndb* pNdb = GETNDB(step);
  int result = NDBT_OK;
  int timeout = 300;
  Uint32 loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  int count;
  NdbRestarter restarter;
  Uint32 i = 1;

  const Uint32 nodeCount = restarter.getNumDbNodes();
  if(nodeCount < 2){
    g_info << "SR4 - Needs atleast 2 nodes to test" << endl;
    return NDBT_OK;
  }

  Vector<int> nodeIds;
  for(i = 0; i<nodeCount; i++)
    nodeIds.push_back(restarter.getDbNodeId(i));
  
  Uint32 currentRestartNodeIndex = 0;
  UtilTransactions utilTrans(*ctx->getTab());
  HugoTransactions hugoTrans(*ctx->getTab());

  {
    int val = DumpStateOrd::DihMinTimeBetweenLCP;
    if(restarter.dumpStateAllNodes(&val, 1) != 0){
      g_err << "ERR: "<< step->getName() 
	    << " failed on line " << __LINE__ << endl; 
      return NDBT_FAILED;
    }
  }
  
  while(i<=loops && result != NDBT_FAILED){
    
    g_info << "Loop " << i << "/"<< loops <<" started" << endl;
    /**
     * 1. Load data
     * 2. Restart 1 node -nostart
     * 3. Update records
     * 4. Restart cluster and verify records
     * 5. Restart 1 node -nostart
     * 6. Delete half of the records
     * 7. Restart cluster and verify records
     * 8. Restart 1 node -nostart
     * 9. Delete all records
     * 10. Restart cluster and verify records
     */
    g_info << "Loading records..." << endl;
    CHECK(hugoTrans.loadTable(pNdb, records) == 0);

    /*** 1 ***/
    g_info << "1 - Stopping one node" << endl;
    CHECK(restarter.restartOneDbNode(nodeIds[currentRestartNodeIndex],
				     false, 
				     true,
				     false) == 0);
    currentRestartNodeIndex = (currentRestartNodeIndex + 1 ) % nodeCount;

    g_info << "Updating records..." << endl;
    CHECK(hugoTrans.pkUpdateRecords(pNdb, records) == 0);
    
    g_info << "Restarting cluster..." << endl;
    CHECK(restarter.restartAll() == 0);
    CHECK(restarter.waitClusterStarted(timeout) == 0);
    {
      int val = DumpStateOrd::DihMinTimeBetweenLCP;
      CHECK(restarter.dumpStateAllNodes(&val, 1) == 0);
    }
    CHECK(pNdb->waitUntilReady(timeout) == 0);

    g_info << "Verifying records..." << endl;
    CHECK(hugoTrans.pkReadRecords(pNdb, records) == 0);
    CHECK(utilTrans.selectCount(pNdb, 64, &count) == 0);
    CHECK(count == records);

    g_info << "2 - Stopping one node" << endl;
    CHECK(restarter.restartOneDbNode(nodeIds[currentRestartNodeIndex],
				     false, 
				     true,
				     false) == 0);
    currentRestartNodeIndex = (currentRestartNodeIndex + 1 ) % nodeCount;

    g_info << "Deleting 50% of records..." << endl;
    CHECK(hugoTrans.pkDelRecords(pNdb, records/2) == 0);
    
    g_info << "Restarting cluster..." << endl;
    CHECK(restarter.restartAll() == 0);
    CHECK(restarter.waitClusterStarted(timeout) == 0);
    {
      int val = DumpStateOrd::DihMinTimeBetweenLCP;
      CHECK(restarter.dumpStateAllNodes(&val, 1) == 0);
    }
    CHECK(pNdb->waitUntilReady(timeout) == 0);

    g_info << "Verifying records..." << endl;
    CHECK(hugoTrans.scanReadRecords(pNdb, records/2, 0, 64) == 0);
    CHECK(utilTrans.selectCount(pNdb, 64, &count) == 0);
    CHECK(count == (records/2));

    g_info << "3 - Stopping one node" << endl;
    CHECK(restarter.restartOneDbNode(nodeIds[currentRestartNodeIndex],
				     false, 
				     true,
				     false) == 0);
    currentRestartNodeIndex = (currentRestartNodeIndex + 1 ) % nodeCount;
    g_info << "Deleting all records..." << endl;
    CHECK(utilTrans.clearTable(pNdb, records/2) == 0);

    g_info << "Restarting cluster..." << endl;
    CHECK(restarter.restartAll() == 0);
    CHECK(restarter.waitClusterStarted(timeout) == 0);
    {
      int val = DumpStateOrd::DihMinTimeBetweenLCP;
      CHECK(restarter.dumpStateAllNodes(&val, 1) == 0);
    }
    CHECK(pNdb->waitUntilReady(timeout) == 0);
    
    ndbout << "Verifying records..." << endl;
    CHECK(utilTrans.selectCount(pNdb, 64, &count) == 0);
    CHECK(count == 0);
    
    i++;
  }

  g_info << "runSystemRestart4 finished" << endl;  

  return result;
}

int runSystemRestart5(NDBT_Context* ctx, NDBT_Step* step){
  Ndb* pNdb = GETNDB(step);
  int result = NDBT_OK;
  int timeout = 300;
  Uint32 loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  int count;
  NdbRestarter restarter;
  Uint32 i = 1;

  const Uint32 nodeCount = restarter.getNumDbNodes();
  if(nodeCount < 2){
    g_info << "SR5 - Needs atleast 2 nodes to test" << endl;
    return NDBT_OK;
  }

  Vector<int> nodeIds;
  for(i = 0; i<nodeCount; i++)
    nodeIds.push_back(restarter.getDbNodeId(i));
  
  Uint32 currentRestartNodeIndex = 0;
  UtilTransactions utilTrans(*ctx->getTab());
  HugoTransactions hugoTrans(*ctx->getTab());

  {
    int val = DumpStateOrd::DihMinTimeBetweenLCP;
    if(restarter.dumpStateAllNodes(&val, 1) != 0){
      g_err << "ERR: "<< step->getName() 
	    << " failed on line " << __LINE__ << endl; 
      return NDBT_FAILED;
    }
  }
  
  while(i<=loops && result != NDBT_FAILED){
    
    g_info << "Loop " << i << "/"<< loops <<" started" << endl;
    /**
     * 1. Load data
     * 2. Restart 1 node -nostart
     * 3. Update records
     * 4. Restart cluster and verify records
     * 5. Restart 1 node -nostart
     * 6. Delete half of the records
     * 7. Restart cluster and verify records
     * 8. Restart 1 node -nostart
     * 9. Delete all records
     * 10. Restart cluster and verify records
     */
    g_info << "Loading records..." << endl;
    hugoTrans.loadTable(pNdb, records);

    /*** 1 ***/
    g_info << "1 - Stopping one node" << endl;
    CHECK(restarter.restartOneDbNode(nodeIds[currentRestartNodeIndex],
				     false, 
				     true,
				     false) == 0);
    currentRestartNodeIndex = (currentRestartNodeIndex + 1 ) % nodeCount;

    g_info << "Updating records..." << endl;
    hugoTrans.pkUpdateRecords(pNdb, records);
    
    g_info << "Restarting cluster..." << endl;
    CHECK(restarter.restartAll(false, false, true) == 0);
    CHECK(restarter.waitClusterStarted(timeout) == 0);
    {
      int val = DumpStateOrd::DihMinTimeBetweenLCP;
      CHECK(restarter.dumpStateAllNodes(&val, 1) == 0);
    }
    CHECK(pNdb->waitUntilReady(timeout) == 0);

    g_info << "Verifying records..." << endl;
    hugoTrans.pkReadRecords(pNdb, records);
    CHECK(utilTrans.selectCount(pNdb, 64, &count) == 0);
    //CHECK(count == records);

    g_info << "2 - Stopping one node" << endl;
    CHECK(restarter.restartOneDbNode(nodeIds[currentRestartNodeIndex],
				     false, 
				     true,
				     false) == 0);
    currentRestartNodeIndex = (currentRestartNodeIndex + 1 ) % nodeCount;

    g_info << "Deleting 50% of records..." << endl;
    hugoTrans.pkDelRecords(pNdb, records/2);
    
    g_info << "Restarting cluster..." << endl;
    CHECK(restarter.restartAll(false, false, true) == 0);
    CHECK(restarter.waitClusterStarted(timeout) == 0);
    {
      int val = DumpStateOrd::DihMinTimeBetweenLCP;
      CHECK(restarter.dumpStateAllNodes(&val, 1) == 0);
    }
    CHECK(pNdb->waitUntilReady(timeout) == 0);

    g_info << "Verifying records..." << endl;
    hugoTrans.scanReadRecords(pNdb, records/2, 0, 64);
    CHECK(utilTrans.selectCount(pNdb, 64, &count) == 0);
    //CHECK(count == (records/2));

    g_info << "3 - Stopping one node" << endl;
    CHECK(restarter.restartOneDbNode(nodeIds[currentRestartNodeIndex],
				     false, 
				     true,
				     false) == 0);
    currentRestartNodeIndex = (currentRestartNodeIndex + 1 ) % nodeCount;
    g_info << "Deleting all records..." << endl;
    utilTrans.clearTable(pNdb, records/2);

    g_info << "Restarting cluster..." << endl;
    CHECK(restarter.restartAll(false, false, true) == 0);
    CHECK(restarter.waitClusterStarted(timeout) == 0);
    {
      int val = DumpStateOrd::DihMinTimeBetweenLCP;
      CHECK(restarter.dumpStateAllNodes(&val, 1) == 0);
    }
    CHECK(pNdb->waitUntilReady(timeout) == 0);
    
    ndbout << "Verifying records..." << endl;
    CHECK(utilTrans.selectCount(pNdb, 64, &count) == 0);
    //CHECK(count == 0);
    
    CHECK(utilTrans.clearTable(pNdb) == 0);    
    i++;
  }

  g_info << "runSystemRestart5 finished" << endl;  

  return result;
}

int runSystemRestart6(NDBT_Context* ctx, NDBT_Step* step){
  Ndb* pNdb = GETNDB(step);
  int result = NDBT_OK;
  int timeout = 300;
  Uint32 loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  NdbRestarter restarter;
  Uint32 i = 1;

  const Uint32 nodeCount = restarter.getNumDbNodes();
  if(nodeCount < 2){
    g_info << "SR6 - Needs atleast 2 nodes to test" << endl;
    return NDBT_OK;
  }

  Vector<int> nodeIds;
  for(i = 0; i<nodeCount; i++)
    nodeIds.push_back(restarter.getDbNodeId(i));
  
  Uint32 currentRestartNodeIndex = 0;
  UtilTransactions utilTrans(*ctx->getTab());
  HugoTransactions hugoTrans(*ctx->getTab());

  while(i<=loops && result != NDBT_FAILED){
    
    g_info << "Loop " << i << "/"<< loops <<" started" << endl;
    /**
     * 1. Load data
     * 2. Restart all node -nostart
     * 3. Restart some nodes -i -nostart
     * 4. Start all nodes verify records
     */
    g_info << "Loading records..." << endl;
    hugoTrans.loadTable(pNdb, records);

    CHECK(restarter.restartAll(false, true, false) == 0);

    Uint32 nodeId = nodeIds[currentRestartNodeIndex];
    currentRestartNodeIndex = (currentRestartNodeIndex + 1 ) % nodeCount;
    
    CHECK(restarter.restartOneDbNode(nodeId, true, true,false) == 0);
    CHECK(restarter.waitClusterNoStart(timeout) == 0);
    CHECK(restarter.startAll() == 0);
    CHECK(restarter.waitClusterStarted(timeout) == 0);
    CHECK(pNdb->waitUntilReady(timeout) == 0);
    int count = records - 1;
    CHECK(utilTrans.selectCount(pNdb, 64, &count) == 0);
    CHECK(count == records);
    CHECK(utilTrans.clearTable(pNdb) == 0);    
    i++;
  }

  g_info << "runSystemRestart6 finished" << endl;  

  return result;
}

int runSystemRestart7(NDBT_Context* ctx, NDBT_Step* step){
  Ndb* pNdb = GETNDB(step);
  int result = NDBT_OK;
  Uint32 loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  NdbRestarter restarter;
  Uint32 i = 1;

  const Uint32 nodeCount = restarter.getNumDbNodes();
  if(nodeCount < 2){
    g_info << "SR7 - Needs atleast 2 nodes to test" << endl;
    return NDBT_OK;
  }

  Vector<int> nodeIds;
  for(i = 0; i<nodeCount; i++)
    nodeIds.push_back(restarter.getDbNodeId(i));

  int a_nodeIds[64];
  if(nodeCount > 64)
    abort();

  Uint32 currentRestartNodeIndex = 1;
  UtilTransactions utilTrans(*ctx->getTab());
  HugoTransactions hugoTrans(*ctx->getTab());

  while(i<=loops && result != NDBT_FAILED){
    
    g_info << "Loop " << i << "/"<< loops <<" started" << endl;
    /**
     * 1. Load data
     * 2. Restart all node -nostart
     * 3. Start all but one node
     * 4. Wait for startphase >= 2
     * 5. Start last node
     * 6. Verify records
     */
    g_info << "Loading records..." << endl;
    hugoTrans.loadTable(pNdb, records);
    
    CHECK(restarter.restartAll(false, true, false) == 0);
    
    int nodeId = nodeIds[currentRestartNodeIndex];
    currentRestartNodeIndex = (currentRestartNodeIndex + 1 ) % nodeCount;

    Uint32 j = 0;
    for(Uint32 k = 0; k<nodeCount; k++){
      if(nodeIds[k] != nodeId){
	a_nodeIds[j++] = nodeIds[k];
      }
    }

    CHECK(restarter.startNodes(a_nodeIds, nodeCount - 1) == 0);
    CHECK(restarter.waitNodesStarted(a_nodeIds, nodeCount - 1, 120) == 0);
    CHECK(pNdb->waitUntilReady(5) == 0);
    int count = records - 1;
    CHECK(utilTrans.selectCount(pNdb, 64, &count) == 0);
    CHECK(count == records);
    
    CHECK(restarter.startNodes(&nodeId, 1) == 0);
    CHECK(restarter.waitNodesStarted(&nodeId, 1, 120) == 0);
    
    CHECK(utilTrans.selectCount(pNdb, 64, &count) == 0);
    CHECK(count == records);
    CHECK(utilTrans.clearTable(pNdb) == 0);    

    i++;
  }
  
  g_info << "runSystemRestart7 finished" << endl;  

  return result;
}

int runSystemRestart8(NDBT_Context* ctx, NDBT_Step* step){
  Ndb* pNdb = GETNDB(step);
  int result = NDBT_OK;
  int timeout = 300;
  Uint32 loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  NdbRestarter restarter;
  Uint32 i = 1;

  const Uint32 nodeCount = restarter.getNumDbNodes();
  if(nodeCount < 2){
    g_info << "SR8 - Needs atleast 2 nodes to test" << endl;
    return NDBT_OK;
  }

  Vector<int> nodeIds;
  for(i = 0; i<nodeCount; i++)
    nodeIds.push_back(restarter.getDbNodeId(i));

  int a_nodeIds[64];
  if(nodeCount > 64)
    abort();

  Uint32 currentRestartNodeIndex = 1;
  UtilTransactions utilTrans(*ctx->getTab());
  HugoTransactions hugoTrans(*ctx->getTab());

  while(i<=loops && result != NDBT_FAILED){
    
    g_info << "Loop " << i << "/"<< loops <<" started" << endl;
    /**
     * 1. Load data
     * 2. Restart all node -nostart
     * 3. Start all but one node
     * 4. Verify records
     * 5. Start last node
     * 6. Verify records
     */
    g_info << "Loading records..." << endl;
    hugoTrans.loadTable(pNdb, records);
    
    CHECK(restarter.restartAll(false, true, false) == 0);
    
    int nodeId = nodeIds[currentRestartNodeIndex];
    currentRestartNodeIndex = (currentRestartNodeIndex + 1 ) % nodeCount;

    Uint32 j = 0;
    for(Uint32 k = 0; k<nodeCount; k++){
      if(nodeIds[k] != nodeId){
	a_nodeIds[j++] = nodeIds[k];
      }
    }
    
    CHECK(restarter.startNodes(a_nodeIds, nodeCount-1) == 0);
    CHECK(restarter.waitNodesStartPhase(a_nodeIds, nodeCount-1, 3, 120) == 0);
    CHECK(restarter.startNodes(&nodeId, 1) == 0);
    CHECK(restarter.waitClusterStarted(timeout) == 0);
    CHECK(pNdb->waitUntilReady() == 0);
    
    int count = records - 1;
    CHECK(utilTrans.selectCount(pNdb, 64, &count) == 0);
    CHECK(count == records);
    CHECK(utilTrans.clearTable(pNdb) == 0);    
    i++;
  }
  
  g_info << "runSystemRestart8 finished" << endl;  

  return result;
}

int runSystemRestart9(NDBT_Context* ctx, NDBT_Step* step){
  Ndb* pNdb = GETNDB(step);
  int result = NDBT_OK;
  int timeout = 300;
  NdbRestarter restarter;
  Uint32 i = 1;
  
  UtilTransactions utilTrans(*ctx->getTab());
  HugoTransactions hugoTrans(*ctx->getTab());

  int args[] = { DumpStateOrd::DihMaxTimeBetweenLCP };
  int dump[] = { DumpStateOrd::DihStartLcpImmediately };
  
  do {
    CHECK(restarter.dumpStateAllNodes(args, 1) == 0);
    
    HugoOperations ops(* ctx->getTab());
    CHECK(ops.startTransaction(pNdb) == 0);
    for(i = 0; i<10; i++){
      CHECK(ops.pkInsertRecord(pNdb, i, 1, 1) == 0);
      CHECK(ops.execute_NoCommit(pNdb) == 0);
    }
    for(i = 0; i<10; i++){
      CHECK(ops.pkUpdateRecord(pNdb, i, 1) == 0);
      CHECK(ops.execute_NoCommit(pNdb) == 0);
    }
    NdbSleep_SecSleep(10);
    CHECK(restarter.dumpStateAllNodes(dump, 1) == 0);
    NdbSleep_SecSleep(10);
    CHECK(ops.execute_Commit(pNdb) == 0);  
    
    CHECK(restarter.restartAll() == 0);
    CHECK(restarter.waitClusterStarted(timeout) == 0);
    CHECK(pNdb->waitUntilReady(timeout) == 0);
    ops.closeTransaction(pNdb);
  } while(0);
  
  g_info << "runSystemRestart9 finished" << endl;  

  return result;
}

int runSystemRestart10(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  int result = NDBT_OK;
  //Uint32 loops = ctx->getNumLoops();
  Uint32 loops = 3;
  int records = ctx->getNumRecords();
  NdbRestarter restarter;
  Uint32 i = 1;

  const Uint32 nodeCount = restarter.getNumDbNodes();
  if(nodeCount < 4){
    g_info << "SR10 - Needs atleast 4 nodes to test" << endl;
    return NDBT_OK;
  }

  Vector<int> nodeIds;
  for(i = 0; i<nodeCount; i++)
    nodeIds.push_back(restarter.getDbNodeId(i));

  int a_nodeIds[64];
  if(nodeCount > 64)
    abort();

  UtilTransactions utilTrans(*ctx->getTab());
  HugoTransactions hugoTrans(*ctx->getTab());

  i = 1;
  while(i < loops && result != NDBT_FAILED){
    
    g_info << "Loop " << i << "/"<< loops <<" started" << endl;
    /**
     * 1. Load data
     * 2. Stop one node X (restart -nostart)
     * 3. Wait 10 seconds to ensure some GCPs are executed.
     * 4. Stop the rest of the nodes
     * 5. Start all nodes, but insert an error into the 2nd
     *    node to prevent it from passing phase 3 for 10
     *    seconds. The cluster should wait for these 10
     *    seconds, it cannot proceed at this point without
     *    it. If it tries to start without it, there will
     *    be a crash of the system restart.
     * 6. Verify records
     */

    g_info << "Loading records..." << endl;
    hugoTrans.loadTable(pNdb, records);
   
    Uint32 j = 0;
    for(Uint32 k = 0; k<nodeCount; k++)
    {
      a_nodeIds[j++] = nodeIds[k];
    }

    g_info << "Stop 2nd last node" << endl;
    CHECK(restarter.restartOneDbNode(a_nodeIds[nodeCount - 2],
				     false, 
				     true,
				     false) == 0);

    NdbSleep_SecSleep(10);
    g_info << "Stop rest of the nodes" << endl;
    CHECK(restarter.restartAll(false, true, false) == 0);
    
    int nodeId = a_nodeIds[nodeCount - 1];

    if (i == 0)
    {
      g_info << "Inject Error 1021 into last node to stop it in phase 1" << endl;
      CHECK(restarter.insertErrorInNode(nodeId, 1021) == 0);
    }
    else if (i == 1)
    {
      g_info << "Inject Error 1010 into last node to stop it in phase 4" << endl;
      CHECK(restarter.insertErrorInNode(nodeId, 1010) == 0);
    }
    if (i == 2)
    {
      g_info << "Start all nodes except the last node" << endl;
      CHECK(restarter.startNodes(a_nodeIds, nodeCount - 1) == 0);
      g_info << "Wait for those nodes to start, expect failure" << endl;
      CHECK(restarter.waitNodesStarted(a_nodeIds, nodeCount - 1, 30) != 0);
      g_info << "Start the last node" << endl;
      CHECK(restarter.startNodes(&nodeId, 1) == 0);
      g_info << "Wait for cluster to be started" << endl;
      CHECK(restarter.waitNodesStarted(a_nodeIds, nodeCount, 120) == 0);
    }
    else
    {
      CHECK(restarter.startNodes(a_nodeIds, nodeCount) == 0);
      g_info << "Wait for cluster to be started" << endl;
      CHECK(restarter.waitNodesStarted(a_nodeIds, nodeCount, 120) == 0);
    }
    g_info << "Perform consistency checks" << endl;
    CHECK(pNdb->waitUntilReady(5) == 0);
    int count = records - 1;
    CHECK(utilTrans.selectCount(pNdb, 64, &count) == 0);
    CHECK(count == records);
    
    CHECK(utilTrans.selectCount(pNdb, 64, &count) == 0);
    CHECK(count == records);
    CHECK(utilTrans.clearTable(pNdb) == 0);    

    i++;
  }
  
  g_info << "runSystemRestart10 finished" << endl;  

  return result;
}

int runBug18385(NDBT_Context* ctx, NDBT_Step* step){
  NdbRestarter restarter;
  const Uint32 nodeCount = restarter.getNumDbNodes();
  if(nodeCount < 2){
    g_info << "Bug18385 - Needs atleast 2 nodes to test" << endl;
    return NDBT_OK;
  }

  int node1 = restarter.getDbNodeId(rand() % nodeCount);
  int node2 = restarter.getRandomNodeSameNodeGroup(node1, rand());

  if (node1 == -1 || node2 == -1)
    return NDBT_OK;
  
  int dump[] = { DumpStateOrd::DihSetTimeBetweenGcp, 300 };
  
  int result = NDBT_OK;
  do {
    CHECK(restarter.dumpStateAllNodes(dump, 2) == 0);
    CHECK(restarter.restartOneDbNode(node1, false, true, false) == 0);
    NdbSleep_SecSleep(3);
    CHECK(restarter.restartAll(false, true, false) == 0);
    
    Uint32 cnt = 0;
    int nodes[128];
    for(Uint32 i = 0; i<nodeCount; i++)
      if ((nodes[cnt] = restarter.getDbNodeId(i)) != node2)
	cnt++;
    
    require(cnt == nodeCount - 1);
    
    CHECK(restarter.startNodes(nodes, cnt) == 0);
    CHECK(restarter.waitNodesStarted(nodes, cnt, 300) == 0);
    
    CHECK(restarter.insertErrorInNode(node2, 7170) == 0);
    CHECK(restarter.waitNodesNoStart(&node2, 1) == 0);
    CHECK(restarter.restartOneDbNode(node2, true, false, true) == 0);
    CHECK(restarter.waitNodesStarted(&node2, 1) == 0);

  } while(0);
  
  g_info << "Bug18385 finished" << endl;  
  
  return result;
}

int runWaitStarted(NDBT_Context* ctx, NDBT_Step* step){

  NdbRestarter restarter;
  restarter.waitClusterStarted(300);

  NdbSleep_SecSleep(3);
  return NDBT_OK;
}

int runClearTable(NDBT_Context* ctx, NDBT_Step* step){
  int records = ctx->getNumRecords();
  
  Ndb* pNdb = GETNDB(step);
  if(pNdb->waitUntilReady(5) != 0){
    return NDBT_FAILED;
  }

  UtilTransactions utilTrans(*ctx->getTab());  
  if (utilTrans.clearTable2(pNdb,  records) != 0){
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int 
runBug21536(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbRestarter restarter;
  const Uint32 nodeCount = restarter.getNumDbNodes();
  if(nodeCount != 2){
    g_info << "Bug21536 - 2 nodes to test" << endl;
    return NDBT_OK;
  }

  int node1 = restarter.getDbNodeId(rand() % nodeCount);
  int node2 = restarter.getRandomNodeSameNodeGroup(node1, rand());

  if (node1 == -1 || node2 == -1)
    return NDBT_OK;
  
  int result = NDBT_OK;
  do {
    CHECK(restarter.restartOneDbNode(node1, false, true, true) == 0);
    CHECK(restarter.waitNodesNoStart(&node1, 1) == 0);    
    CHECK(restarter.insertErrorInNode(node1, 1000) == 0);    
    int val2[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };
    CHECK(restarter.dumpStateOneNode(node1, val2, 2) == 0);
    CHECK(restarter.startNodes(&node1, 1) == 0);    
    restarter.waitNodesStartPhase(&node1, 1, 3, 120);
    CHECK(restarter.waitNodesNoStart(&node1, 1) == 0);    
    
    CHECK(restarter.restartOneDbNode(node2, true, true, true) == 0);
    CHECK(restarter.waitNodesNoStart(&node2, 1) == 0);    
    CHECK(restarter.startNodes(&node1, 1) == 0);   
    CHECK(restarter.waitNodesStarted(&node1, 1) == 0);
    CHECK(restarter.startNodes(&node2, 1) == 0);   
    CHECK(restarter.waitClusterStarted() == 0);

  } while(0);
  
  g_info << "Bug21536 finished" << endl;  
  
  return result;
}

int 
runBug24664(NDBT_Context* ctx, NDBT_Step* step)
{
  int result = NDBT_OK;
  NdbRestarter restarter;
  Ndb* pNdb = GETNDB(step);

  int records = ctx->getNumRecords();
  UtilTransactions utilTrans(*ctx->getTab());
  HugoTransactions hugoTrans(*ctx->getTab());

  int args[] = { DumpStateOrd::DihMaxTimeBetweenLCP };
  int dump[] = { DumpStateOrd::DihStartLcpImmediately };
  
  restarter.getNumDbNodes();
  int filter[] = { 15, NDB_MGM_EVENT_CATEGORY_CHECKPOINT, 0 };
  NdbLogEventHandle handle = 
    ndb_mgm_create_logevent_handle(restarter.handle, filter);

  struct ndb_logevent event;

  do {
    CHECK(restarter.dumpStateAllNodes(args, 1) == 0);
    CHECK(restarter.dumpStateAllNodes(dump, 1) == 0);
    while(ndb_logevent_get_next(handle, &event, 0) >= 0 &&
	  event.type != NDB_LE_LocalCheckpointStarted);
    while(ndb_logevent_get_next(handle, &event, 0) >= 0 &&
	  event.type != NDB_LE_LocalCheckpointCompleted);
    
    if (hugoTrans.loadTable(GETNDB(step), records) != 0){
      return NDBT_FAILED;
    }
  
    restarter.insertErrorInAllNodes(10039); // Hang LCP
    CHECK(restarter.dumpStateAllNodes(dump, 1) == 0);
    while(ndb_logevent_get_next(handle, &event, 0) >= 0 &&
	  event.type != NDB_LE_LocalCheckpointStarted);
    NdbSleep_SecSleep(3);
    CHECK(utilTrans.clearTable(pNdb,  records) == 0);
    if (hugoTrans.loadTable(GETNDB(step), records) != 0){
      return NDBT_FAILED;
    }

    restarter.insertErrorInAllNodes(10040); // Resume LCP
    while(ndb_logevent_get_next(handle, &event, 0) >= 0 &&
	  event.type != NDB_LE_LocalCheckpointCompleted);

    while(ndb_logevent_get_next(handle, &event, 0) >= 0 &&
	  event.type != NDB_LE_GlobalCheckpointCompleted);
    while(ndb_logevent_get_next(handle, &event, 0) >= 0 &&
	  event.type != NDB_LE_GlobalCheckpointCompleted);
    restarter.restartAll(false, false, true);
    CHECK(restarter.waitClusterStarted() == 0);
  } while(false);
  
  return result;
}

int 
runBug27434(NDBT_Context* ctx, NDBT_Step* step)
{
  int result = NDBT_OK;
  NdbRestarter restarter;
  const Uint32 nodeCount = restarter.getNumDbNodes();

  if (nodeCount < 2)
    return NDBT_OK;

  int args[] = { DumpStateOrd::DihMaxTimeBetweenLCP };
  int dump[] = { DumpStateOrd::DihStartLcpImmediately };

  int filter[] = { 15, NDB_MGM_EVENT_CATEGORY_CHECKPOINT, 0 };
  NdbLogEventHandle handle = 
    ndb_mgm_create_logevent_handle(restarter.handle, filter);

  struct ndb_logevent event;

  do {
    int node1 = restarter.getDbNodeId(rand() % nodeCount);
    CHECK(restarter.restartOneDbNode(node1, false, true, true) == 0);
    NdbSleep_SecSleep(3);
    CHECK(restarter.waitNodesNoStart(&node1, 1) == 0);

    CHECK(restarter.dumpStateAllNodes(args, 1) == 0);

    for (Uint32 i = 0; i<3; i++)
    {
      CHECK(restarter.dumpStateAllNodes(dump, 1) == 0);
      while(ndb_logevent_get_next(handle, &event, 0) >= 0 &&
	    event.type != NDB_LE_LocalCheckpointStarted);
      while(ndb_logevent_get_next(handle, &event, 0) >= 0 &&
	    event.type != NDB_LE_LocalCheckpointCompleted);
    }      
    
    restarter.restartAll(false, true, true);
    NdbSleep_SecSleep(3);
    CHECK(restarter.waitClusterNoStart() == 0);
    restarter.insertErrorInNode(node1, 5046);
    restarter.startAll();
    CHECK(restarter.waitClusterStarted() == 0);
  } while(false);
  
  return result;
}

int
runBug29167(NDBT_Context* ctx, NDBT_Step* step)
{
  int result = NDBT_OK;
  NdbRestarter restarter;
  const Uint32 nodeCount = restarter.getNumDbNodes();

  if (nodeCount < 4)
    return NDBT_OK;

  struct ndb_logevent event;
  int master = restarter.getMasterNodeId();
  do {
    int node1 = restarter.getRandomNodeOtherNodeGroup(master, rand());
    int node2 = restarter.getRandomNodeSameNodeGroup(node1, rand());
    
    ndbout_c("node1: %u node2: %u", node1, node2);

    int val2[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };    
    restarter.dumpStateAllNodes(val2, 2);
    int dump[] = { DumpStateOrd::DihSetTimeBetweenGcp, 30000 };
    restarter.dumpStateAllNodes(dump, 2);
    
    int filter[] = { 15, NDB_MGM_EVENT_CATEGORY_CHECKPOINT, 0 };
    NdbLogEventHandle handle =
      ndb_mgm_create_logevent_handle(restarter.handle, filter);

    while(ndb_logevent_get_next(handle, &event, 0) >= 0 &&
          event.type != NDB_LE_GlobalCheckpointCompleted);
    
    ndb_mgm_destroy_logevent_handle(&handle);

    CHECK(restarter.insertErrorInAllNodes(932) == 0);
    
    CHECK(restarter.insertErrorInNode(node1, 7183) == 0);
    CHECK(restarter.insertErrorInNode(node2, 7183) == 0);

    CHECK(restarter.waitClusterNoStart() == 0);
    restarter.startAll();
    CHECK(restarter.waitClusterStarted() == 0);  
  } while(false);
  
  return result;
}
int
runBug28770(NDBT_Context* ctx, NDBT_Step* step) {
  Ndb* pNdb = GETNDB(step);
  NdbRestarter restarter;
  int result = NDBT_OK;
  int count = 0;
  Uint32 i = 0;
  Uint32 loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  UtilTransactions utilTrans(*ctx->getTab());
  HugoTransactions hugoTrans(*ctx->getTab());

  g_info << "Loading records..." << endl;  hugoTrans.loadTable(pNdb, 
 records);


  while(i<=loops && result != NDBT_FAILED)
  {
    g_info << "Loop " << i << "/"<< loops <<" started" << endl;
    if (i == 0)
    {
      CHECK(restarter.restartAll(false, true, false) == 0); // graceful
    }
    else
    {
      CHECK(restarter.restartAll(false, true, true) == 0); // abort
    }
    CHECK(restarter.waitClusterNoStart() == 0);
    restarter.insertErrorInAllNodes(6024);
    CHECK(restarter.startAll()== 0);
    CHECK(restarter.waitClusterStarted() == 0);
    CHECK(pNdb->waitUntilReady() == 0);
    CHECK(utilTrans.selectCount(pNdb, 64, &count) == 0);
    CHECK(count == records);
    i++;
  }
  ndbout << " runBug28770 finished" << endl;
  return result;
}

int
runStopper(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbRestarter restarter;
  Uint32 stop = 0;
loop:
  while (!ctx->isTestStopped() && 
	 ((stop = ctx->getProperty("StopAbort", Uint32(0))) == 0))
  {
    NdbSleep_MilliSleep(30);
  }

  if (ctx->isTestStopped())
  {
    return NDBT_OK;
  }

  ctx->setProperty("StopAbort", Uint32(0));
  
  ndbout << "Killing in " << stop << "ms..." << flush;
  NdbSleep_MilliSleep(stop);
  restarter.restartAll(false, true, true);
  goto loop;
}

int runSR_DD_1(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  int result = NDBT_OK;
  Uint32 loops = ctx->getNumLoops();
  NdbRestarter restarter;
  NdbBackup backup;
  bool lcploop = ctx->getProperty("LCP", (unsigned)0);
  bool all = ctx->getProperty("ALL", (unsigned)0);

  Uint32 i = 1;

  int val[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };
  int lcp = DumpStateOrd::DihMinTimeBetweenLCP;

  int startFrom = 0;

  HugoTransactions hugoTrans(*ctx->getTab());
  while(i<=loops && result != NDBT_FAILED)
  {
    if (i > 0 && ctx->closeToTimeout(30))
      break;

    if (lcploop)
    {
      CHECK(restarter.dumpStateAllNodes(&lcp, 1) == 0);
    }

    int nodeId = restarter.getDbNodeId(rand() % restarter.getNumDbNodes());
    //CHECK(restarter.dumpStateAllNodes(&val, 1) == 0);
    
    ndbout << "Loop " << i << "/"<< loops <<" started" << endl;
    ndbout << "Loading records..." << startFrom << endl;
    CHECK(hugoTrans.loadTable(pNdb, startFrom) == 0);

    if (!all)
    {
      ndbout << "Making " << nodeId << " crash" << endl;
      int kill[] = { 9999, 1000, 3000 };
      CHECK(restarter.dumpStateOneNode(nodeId, val, 2) == 0);
      CHECK(restarter.dumpStateOneNode(nodeId, kill, 3) == 0);
    }
    else
    {
      ndbout << "Crashing cluster" << endl;
      ctx->setProperty("StopAbort", 1000 + rand() % (3000 - 1000));
    }
    Uint64 end = NdbTick_CurrentMillisecond() + 4000;
    Uint32 row = startFrom;
    do {
      ndbout << "Loading from " << row << " to " << row + 1000 << endl;
      if (hugoTrans.loadTableStartFrom(pNdb, row, 1000) != 0)
	break;
      row += 1000;
    } while (NdbTick_CurrentMillisecond() < end);

    if (!all)
    {
      ndbout << "Waiting for " << nodeId << " to restart" << endl;
      CHECK(restarter.waitNodesNoStart(&nodeId, 1) == 0);
      ndbout << "Restarting cluster" << endl;
      CHECK(restarter.restartAll(false, true, true) == 0);
    }
    else
    {
      ndbout << "Waiting for cluster to restart" << endl;
    }
    CHECK(restarter.waitClusterNoStart() == 0);
    CHECK(restarter.startAll() == 0);
    CHECK(restarter.waitClusterStarted() == 0);
    CHECK(pNdb->waitUntilReady() == 0);

    ndbout << "Starting backup..." << flush;
    CHECK(backup.start() == 0);
    ndbout << "done" << endl;

    int cnt = 0;
    CHECK(hugoTrans.selectCount(pNdb, 0, &cnt) == 0);
    ndbout << "Found " << cnt << " records..." << endl;
    ndbout << "Updating..." << endl;
    CHECK(hugoTrans.scanUpdateRecords(pNdb,
                                      NdbScanOperation::SF_TupScan, cnt) == 0
          || hugoTrans.getRetryMaxReached());
    ndbout << "Clearing..." << endl;    
    CHECK(hugoTrans.clearTable(pNdb,
                               NdbScanOperation::SF_TupScan, cnt) == 0);
    
    if (cnt > startFrom)
    {
      startFrom = cnt;
    }
    startFrom += 1000;
    i++;
  }
  
  ndbout << "runSR_DD_1 finished" << endl;  
  ctx->stopTest();
  return result;
}

int runSR_DD_2(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  int result = NDBT_OK;
  Uint32 loops = ctx->getNumLoops();
  Uint32 rows = ctx->getNumRecords();
  NdbRestarter restarter;
  NdbBackup backup;
  bool lcploop = ctx->getProperty("LCP", (unsigned)0);
  bool all = ctx->getProperty("ALL", (unsigned)0);
  int error = (int)ctx->getProperty("ERROR", (unsigned)0);
  rows = ctx->getProperty("ROWS", rows);

  Uint32 i = 1;

  int val[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };
  int lcp = DumpStateOrd::DihMinTimeBetweenLCP;

  if (error)
  {
    restarter.insertErrorInAllNodes(error);
  }

  HugoTransactions hugoTrans(*ctx->getTab());
  while(i<=loops && result != NDBT_FAILED)
  {
    if (i > 0 && ctx->closeToTimeout(30))
      break;

    if (lcploop)
    {
      CHECK(restarter.dumpStateAllNodes(&lcp, 1) == 0);
    }

    int nodeId = restarter.getDbNodeId(rand() % restarter.getNumDbNodes());
    
    if (!all)
    {
      ndbout << "Making " << nodeId << " crash" << endl;
      int kill[] = { 9999, 3000, 10000 };
      CHECK(restarter.dumpStateOneNode(nodeId, val, 2) == 0);
      CHECK(restarter.dumpStateOneNode(nodeId, kill, 3) == 0);
    }
    else
    {
      ndbout << "Crashing cluster" << endl;
      ctx->setProperty("StopAbort", 3000 + rand() % (10000 - 3000));
    }

    Uint64 end = NdbTick_CurrentMillisecond() + 11000;
    do {
      if (hugoTrans.loadTable(pNdb, rows) != 0)
	break;
      
      if (hugoTrans.clearTable(pNdb, NdbScanOperation::SF_TupScan, rows) != 0)
	break;
    } while (NdbTick_CurrentMillisecond() < end);
    
    if (!all)
    {
      ndbout << "Waiting for " << nodeId << " to restart" << endl;
      CHECK(restarter.waitNodesNoStart(&nodeId, 1) == 0);
      ndbout << "Restarting cluster" << endl;
      CHECK(restarter.restartAll(false, true, true) == 0);
    }
    else
    {
      ndbout << "Waiting for cluster to restart" << endl;
    }

    CHECK(restarter.waitClusterNoStart() == 0);
    CHECK(restarter.startAll() == 0);
    CHECK(restarter.waitClusterStarted() == 0);
    CHECK(pNdb->waitUntilReady() == 0);

    if (error)
    {
      restarter.insertErrorInAllNodes(error);
    }

    ndbout << "Starting backup..." << flush;
    CHECK(backup.start() == 0);
    ndbout << "done" << endl;

    int cnt = 0;
    CHECK(hugoTrans.selectCount(pNdb, 0, &cnt) == 0);
    ndbout << "Found " << cnt << " records..." << endl;
    ndbout << "Updating..." << endl;
    CHECK(hugoTrans.scanUpdateRecords(pNdb,
                                      NdbScanOperation::SF_TupScan, cnt) == 0
          || hugoTrans.getRetryMaxReached());
    ndbout << "Clearing..." << endl;    
    CHECK(hugoTrans.clearTable(pNdb,
                               NdbScanOperation::SF_TupScan, cnt) == 0);
    i++;
  }

  if (error)
  {
    restarter.insertErrorInAllNodes(0);
  }
  
  ndbout << "runSR_DD_2 finished" << endl;  
  ctx->stopTest();  
  return result;
}

int runSR_DD_3(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  int result = NDBT_OK;
  Uint32 loops = ctx->getNumLoops();
  Uint32 rows = ctx->getNumRecords();
  NdbRestarter restarter;
  NdbBackup backup;
  bool lcploop = ctx->getProperty("LCP", (unsigned)0);
  bool all = ctx->getProperty("ALL", (unsigned)0);
  int error = (int)ctx->getProperty("ERROR", (unsigned)0);
  rows = ctx->getProperty("ROWS", rows);

  Uint32 i = 1;

  int val[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };
  int lcp = DumpStateOrd::DihMinTimeBetweenLCP;

  if (error)
  {
    restarter.insertErrorInAllNodes(error);
  }

  HugoTransactions hugoTrans(*ctx->getTab());
  while(i<=loops && result != NDBT_FAILED)
  {
    if (i > 0 && ctx->closeToTimeout(30))
      break;

    if (lcploop)
    {
      CHECK(restarter.dumpStateAllNodes(&lcp, 1) == 0);
    }

    int nodeId = restarter.getDbNodeId(rand() % restarter.getNumDbNodes());

    if (hugoTrans.loadTable(pNdb, rows) != 0)
    {
      return NDBT_FAILED;
    }

    if (!all)
    {
      ndbout << "Making " << nodeId << " crash" << endl;
      int kill[] = { 9999, 3000, 10000 };
      CHECK(restarter.dumpStateOneNode(nodeId, val, 2) == 0);
      CHECK(restarter.dumpStateOneNode(nodeId, kill, 3) == 0);
    }
    else
    {
      ndbout << "Crashing cluster" << endl;
      ctx->setProperty("StopAbort", 3000 + rand() % (10000 - 3000));
    }

    int deletedrows[100];
    Uint64 end = NdbTick_CurrentMillisecond() + 13000;
    do {
      Uint32 cnt = 0;
      for (; cnt<NDB_ARRAY_SIZE(deletedrows); cnt++)
      {
        deletedrows[cnt] = rand() % rows;
        if (hugoTrans.startTransaction(pNdb))
          break;
        if (hugoTrans.pkDeleteRecord(pNdb, deletedrows[cnt]))
          break;
        if (hugoTrans.execute_Commit(pNdb))
          break;
        hugoTrans.closeTransaction(pNdb);
      }
      if (hugoTrans.getTransaction() != 0)
        hugoTrans.closeTransaction(pNdb);

      if (hugoTrans.scanUpdateRecords(pNdb, NdbScanOperation::SF_TupScan,0)!=0)
	break;

      for (Uint32 n = 0; n<cnt; n++)
      {
        if (hugoTrans.startTransaction(pNdb))
          break;
        if (hugoTrans.pkInsertRecord(pNdb, deletedrows[n], 1, rand()))
          break;
        if (hugoTrans.execute_Commit(pNdb))
          break;
        hugoTrans.closeTransaction(pNdb);
      }
      if (hugoTrans.getTransaction() != 0)
        hugoTrans.closeTransaction(pNdb);

      if (hugoTrans.scanUpdateRecords(pNdb, NdbScanOperation::SF_TupScan,0)!=0
          && !hugoTrans.getRetryMaxReached())
	break;
    } while (NdbTick_CurrentMillisecond() < end);

    if (!all)
    {
      ndbout << "Waiting for " << nodeId << " to restart" << endl;
      CHECK(restarter.waitNodesNoStart(&nodeId, 1) == 0);
      ndbout << "Restarting cluster" << endl;
      CHECK(restarter.restartAll(false, true, true) == 0);
    }
    else
    {
      ndbout << "Waiting for cluster to restart" << endl;
    }

    CHECK(restarter.waitClusterNoStart() == 0);
    CHECK(restarter.startAll() == 0);
    CHECK(restarter.waitClusterStarted() == 0);
    if (error)
    {
      restarter.insertErrorInAllNodes(error);
    }

    ndbout << "Starting backup..." << flush;
    CHECK(backup.start() == 0);
    ndbout << "done" << endl;

    int cnt = 0;
    CHECK(hugoTrans.selectCount(pNdb, 0, &cnt) == 0);
    ndbout << "Found " << cnt << " records..." << endl;
    ndbout << "Updating..." << endl;
    CHECK(hugoTrans.scanUpdateRecords(pNdb,
                                      NdbScanOperation::SF_TupScan, cnt) == 0);
    ndbout << "Clearing..." << endl;
    CHECK(hugoTrans.clearTable(pNdb,
                               NdbScanOperation::SF_TupScan, cnt) == 0);
    i++;
  }

  if (error)
  {
    restarter.insertErrorInAllNodes(0);
  }

  ndbout << "runSR_DD_3 finished" << endl;
  ctx->stopTest();
  return result;
}

int runBug22696(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  int result = NDBT_OK;
  Uint32 loops = ctx->getNumLoops();
  Uint32 rows = ctx->getNumRecords();
  NdbRestarter restarter;
  HugoTransactions hugoTrans(*ctx->getTab());

  Uint32 i = 0;
  while(i<=loops && result != NDBT_FAILED)
  {
    ndbout_c("loop %u", i);
    for (Uint32 j = 0; j<10 && result != NDBT_FAILED; j++)
      CHECK(hugoTrans.scanUpdateRecords(pNdb, rows) == 0);
    
    CHECK(restarter.restartAll(false, true, i > 0 ? true : false) == 0);
    CHECK(restarter.waitClusterNoStart() == 0);
    CHECK(restarter.insertErrorInAllNodes(7072) == 0);
    CHECK(restarter.startAll() == 0);
    CHECK(restarter.waitClusterStarted() == 0);
    CHECK(pNdb->waitUntilReady() == 0);

    i++;
    if (i < loops)
    {
      NdbSleep_SecSleep(5); // Wait for a few gcp
    }
  }
  
  ctx->stopTest();  
  return result;
}

int 
runCreateAllTables(NDBT_Context* ctx, NDBT_Step* step)
{
  if (NDBT_Tables::createAllTables(GETNDB(step), false, true))
    return NDBT_FAILED;
  return NDBT_OK;
}

int
runBasic(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary * pDict = pNdb->getDictionary();
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  NdbRestarter restarter;
  int result = NDBT_OK;

  for (int l = 0; l<loops; l++)
  {
    for (int i = 0; i<NDBT_Tables::getNumTables(); i++)
    {
      const NdbDictionary::Table* tab = 
        pDict->getTable(NDBT_Tables::getTable(i)->getName());
      HugoTransactions trans(* tab);
      switch(l % 3){
      case 0:
        trans.loadTable(pNdb, records);
        trans.scanUpdateRecords(pNdb, records);
        break;
      case 1:
        trans.scanUpdateRecords(pNdb, records);
        trans.clearTable(pNdb, records/2);
        trans.loadTable(pNdb, records/2);
        break;
      case 2:
        trans.clearTable(pNdb, records/2);
        trans.loadTable(pNdb, records/2);
        trans.clearTable(pNdb, records/2);
        break;
      }
    }

    ndbout << "Restarting cluster..." << endl;
    CHECK(restarter.restartAll(false, true, false) == 0);
    CHECK(restarter.waitClusterNoStart() == 0);
    CHECK(restarter.startAll() == 0);
    CHECK(restarter.waitClusterStarted() == 0);
    CHECK(pNdb->waitUntilReady() == 0);

    for (int i = 0; i<NDBT_Tables::getNumTables(); i++)
    {
      const NdbDictionary::Table* tab = 
        pDict->getTable(NDBT_Tables::getTable(i)->getName());
      HugoTransactions trans(* tab);
      trans.scanUpdateRecords(pNdb, records);
    }
  }

  return result;
}

int 
runDropAllTables(NDBT_Context* ctx, NDBT_Step* step)
{
  NDBT_Tables::dropAllTables(GETNDB(step));
  return NDBT_OK;
}

int 
runTO(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  int result = NDBT_OK;
  Uint32 loops = ctx->getNumLoops();
  Uint32 rows = ctx->getNumRecords();
  NdbRestarter res;
  HugoTransactions hugoTrans(*ctx->getTab());

  if (res.getNumDbNodes() < 2)
    return NDBT_OK;

  Uint32 nodeGroups[256];
  Bitmask<256/32> nodeGroupMap;
  for (int j = 0; j<res.getNumDbNodes(); j++)
  {
    int node = res.getDbNodeId(j);
    nodeGroups[node] = res.getNodeGroup(node);
    nodeGroupMap.set(nodeGroups[node]);
  }

  struct ndb_logevent event;
  int val[] = { DumpStateOrd::DihMinTimeBetweenLCP, 0 };

  Uint32 i = 0;
  while(i<=loops && result != NDBT_FAILED)
  {
    if (i > 0 && ctx->closeToTimeout(35))
      break;

    CHECK(res.dumpStateAllNodes(val, 1) == 0);

    int filter[] = { 15, NDB_MGM_EVENT_CATEGORY_CHECKPOINT, 0 };
    NdbLogEventHandle handle = 
      ndb_mgm_create_logevent_handle(res.handle, filter);
    
    Bitmask<256/32> notstopped = nodeGroupMap;
    while(!notstopped.isclear())
    {
      int node;
      do {
        node = res.getDbNodeId(rand() % res.getNumDbNodes());
      } while (!notstopped.get(nodeGroups[node]));
      
      notstopped.clear(nodeGroups[node]);
      ndbout_c("stopping %u", node);
      CHECK(res.restartOneDbNode(node, false, true, true) == 0);
      CHECK(res.waitNodesNoStart(&node, 1) == 0);
      for (Uint32 j = 0; j<25; j++)
      {
        if (! (hugoTrans.scanUpdateRecords(pNdb, 0) == 0))
          break;
      }
      while(ndb_logevent_get_next(handle, &event, 0) >= 0 &&
            event.type != NDB_LE_LocalCheckpointCompleted);
    }

    while(ndb_logevent_get_next(handle, &event, 0) >= 0 &&
	  event.type != NDB_LE_LocalCheckpointCompleted);
    
    Uint32 LCP = event.LocalCheckpointCompleted.lci;
    ndbout_c("LCP: %u", LCP);
    
    do 
    {
      bzero(&event, sizeof(event));
      while(ndb_logevent_get_next(handle, &event, 0) >= 0 &&
            event.type != NDB_LE_LocalCheckpointCompleted)
        bzero(&event, sizeof(event));
      
      if (event.type == NDB_LE_LocalCheckpointCompleted &&
          event.LocalCheckpointCompleted.lci < LCP + 3)
      {
        hugoTrans.scanUpdateRecords(pNdb, 0);
      }
      else
      {
        break;
      }
    } while (true);
    
    ndbout_c("LCP: %u", event.LocalCheckpointCompleted.lci);
    
    CHECK(res.restartAll(false, true, true) == 0);
    CHECK(res.waitClusterNoStart() == 0);
    CHECK(res.startAll() == 0);
    Uint64 now = NdbTick_CurrentMillisecond();
    /**
     * running transaction while cluster is down...
     * causes *lots* of printouts...redirect to /dev/null
     * so that log files doe't get megabytes
     */
    NullOutputStream null;
    OutputStream * save[1];
    save[0] = g_err.m_out;
    g_err.m_out = &null;
    do
    {
      hugoTrans.scanUpdateRecords(pNdb, 0);
    } while (NdbTick_CurrentMillisecond() < (now + 30000));
    g_err.m_out = save[0];
    CHECK(res.waitClusterStarted() == 0);
    CHECK(pNdb->waitUntilReady() == 0);

    hugoTrans.clearTable(pNdb);
    hugoTrans.loadTable(pNdb, rows);
    
    CHECK(res.dumpStateAllNodes(val, 1) == 0);
    
    while(ndb_logevent_get_next(handle, &event, 0) >= 0 &&
          event.type != NDB_LE_LocalCheckpointCompleted);

    ndb_mgm_destroy_logevent_handle(&handle);
    
    i++;
  }

  res.dumpStateAllNodes(val, 2); // Reset LCP time

  ctx->stopTest();  
  return result;
}

int runBug45154(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary * pDict = pNdb->getDictionary();
  int result = NDBT_OK;
  Uint32 loops = ctx->getNumLoops();
  Uint32 rows = ctx->getNumRecords();
  NdbRestarter restarter;

  restarter.getNumDbNodes();
  int filter[] = { 15, NDB_MGM_EVENT_CATEGORY_CHECKPOINT, 0 };
  NdbLogEventHandle handle =
    ndb_mgm_create_logevent_handle(restarter.handle, filter);

  struct ndb_logevent event;

  Uint32 frag_data[128];
  bzero(frag_data, sizeof(frag_data));

  NdbDictionary::HashMap map;
  pDict->getDefaultHashMap(map, 2*restarter.getNumDbNodes());
  pDict->createHashMap(map);

  pDict->getDefaultHashMap(map, restarter.getNumDbNodes());
  pDict->createHashMap(map);  

  for(Uint32 i = 0; i < loops && result != NDBT_FAILED; i++)
  {
    ndbout_c("loop %u", i);

    NdbDictionary::Table copy = *ctx->getTab();
    copy.setName("BUG_45154");
    copy.setFragmentType(NdbDictionary::Object::DistrKeyLin);
    copy.setFragmentCount(2 * restarter.getNumDbNodes());
    copy.setFragmentData(frag_data, 2*restarter.getNumDbNodes());
    pDict->dropTable("BUG_45154");
    int res = pDict->createTable(copy);
    if (res != 0)
    {
      ndbout << pDict->getNdbError() << endl;
      return NDBT_FAILED;
    }
    const NdbDictionary::Table* copyptr= pDict->getTable("BUG_45154");

    {
      HugoTransactions hugoTrans(*copyptr);
      hugoTrans.loadTable(pNdb, rows);
    }

    int dump[] = { DumpStateOrd::DihStartLcpImmediately };
    for (int l = 0; l<2; l++)
    {
      CHECK(restarter.dumpStateAllNodes(dump, 1) == 0);
      while(ndb_logevent_get_next(handle, &event, 0) >= 0 &&
            event.type != NDB_LE_LocalCheckpointStarted);
      while(ndb_logevent_get_next(handle, &event, 0) >= 0 &&
            event.type != NDB_LE_LocalCheckpointCompleted);
    }

    pDict->dropTable("BUG_45154");
    copy.setFragmentCount(restarter.getNumDbNodes());
    copy.setFragmentData(frag_data, restarter.getNumDbNodes());
    res = pDict->createTable(copy);
    if (res != 0)
    {
      ndbout << pDict->getNdbError() << endl;
      return NDBT_FAILED;
    }
    copyptr = pDict->getTable("BUG_45154");

    {
      HugoTransactions hugoTrans(*copyptr);
      hugoTrans.loadTable(pNdb, rows);
      for (Uint32 pp = 0; pp<3; pp++)
        hugoTrans.scanUpdateRecords(pNdb, rows);
    }
    restarter.restartAll(false, true, true);
    restarter.waitClusterNoStart();
    restarter.startAll();
    restarter.waitClusterStarted();

    pDict->dropTable("BUG_45154");
  }

  ctx->stopTest();
  return result;
}

int runBug46651(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary * pDict = pNdb->getDictionary();
  Uint32 rows = ctx->getNumRecords();
  NdbRestarter res;

  NdbDictionary::Table tab;
  tab.setName("BUG_46651");

  NdbDictionary::Column col;
  col.setName("ATTR1");
  col.setType(NdbDictionary::Column::Unsigned);
  col.setLength(1);
  col.setPrimaryKey(true);
  col.setNullable(false);
  col.setAutoIncrement(false);
  tab.addColumn(col);
  col.setName("ATTR2");
  col.setType(NdbDictionary::Column::Unsigned);
  col.setLength(1);
  col.setPrimaryKey(false);
  col.setNullable(false);
  tab.addColumn(col);
  col.setName("ATTR3");
  col.setType(NdbDictionary::Column::Unsigned);
  col.setLength(1);
  col.setPrimaryKey(false);
  col.setNullable(false);
  tab.addColumn(col);
  tab.setForceVarPart(true);
  pDict->dropTable(tab.getName());
  if (pDict->createTable(tab))
  {
    ndbout << pDict->getNdbError() << endl;
    return NDBT_FAILED;
  }

  const NdbDictionary::Table* pTab = pDict->getTable(tab.getName());
  if (pTab == 0)
  {
    ndbout << pDict->getNdbError() << endl;
    return NDBT_FAILED;
  }

  {
    HugoTransactions trans(* pTab);
    if (trans.loadTable(pNdb, rows) != 0)
    {
      return NDBT_FAILED;
    }
  }

  res.restartAll2(NdbRestarter::NRRF_NOSTART);
  if (res.waitClusterNoStart())
    return NDBT_FAILED;
  res.startAll();
  if (res.waitClusterStarted())
    return NDBT_FAILED;

  pNdb->waitUntilReady();

  NdbDictionary::Table newTab = *pTab;
  col.setName("ATTR4");
  col.setType(NdbDictionary::Column::Varbinary);
  col.setLength(25);
  col.setPrimaryKey(false);
  col.setNullable(true);
  col.setDynamic(true);
  newTab.addColumn(col);

  if (pDict->alterTable(*pTab, newTab))
  {
    ndbout << pDict->getNdbError() << endl;
    return NDBT_FAILED;
  }

  res.restartAll2(NdbRestarter::NRRF_NOSTART | NdbRestarter::NRRF_ABORT);
  if (res.waitClusterNoStart())
    return NDBT_FAILED;
  res.startAll();
  if (res.waitClusterStarted())
    return NDBT_FAILED;

  pNdb->waitUntilReady();
  pDict->dropTable(tab.getName());

  return NDBT_OK;
}

int
runBug46412(NDBT_Context* ctx, NDBT_Step* step)
{
  Uint32 loops = ctx->getNumLoops();
  NdbRestarter res;
  const Uint32 nodeCount = res.getNumDbNodes();
  if(nodeCount < 2)
  {
    return NDBT_OK;
  }

  for (Uint32 l = 0; l<loops; l++)
  {
loop:
    printf("checking nodegroups of getNextMasterNodeId(): ");
    int nodes[256];
    bzero(nodes, sizeof(nodes));
    nodes[0] = res.getMasterNodeId();
    printf("%d ", nodes[0]);
    for (Uint32 i = 1; i<nodeCount; i++)
    {
      nodes[i] = res.getNextMasterNodeId(nodes[i-1]);
      printf("%d ", nodes[i]);
    }
    printf("\n");

    Bitmask<256/32> ng;
    int cnt = 0;
    int restartnodes[256];

    Uint32 limit = (nodeCount / 2);
    for (Uint32 i = 0; i<limit; i++)
    {
      int tmp = res.getNodeGroup(nodes[i]);
      printf("node %d ng: %d", nodes[i], tmp);
      if (ng.get(tmp))
      {
        restartnodes[cnt++] = nodes[i];
        ndbout_c(" COLLISION");
        limit++;
        if (limit > nodeCount)
          limit = nodeCount;
      }
      else
      {
        ng.set(tmp);
        ndbout_c(" OK");
      }
    }

    if (cnt)
    {
      printf("restarting nodes: ");
      for (int i = 0; i<cnt; i++)
        printf("%d ", restartnodes[i]);
      printf("\n");
      for (int i = 0; i<cnt; i++)
      {
        res.restartOneDbNode(restartnodes[i], false, true, true);
      }
      res.waitNodesNoStart(restartnodes, cnt);
      res.startNodes(restartnodes, cnt);
      if (res.waitClusterStarted())
        return NDBT_FAILED;

      goto loop;
    }

    int val2[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };
    res.dumpStateAllNodes(val2, 2);

    Bitmask<256/32> mask;
    for (Uint32 i = 0; i<(nodeCount / 2); i++)
    {
      int node = nodes[(nodeCount / 2) - (i + 1)];
      mask.set(node);
      res.insertErrorInNode(node, 7218);
    }
    
    for (Uint32 i = 0; i<nodeCount; i++)
    {
      int node = nodes[i];
      if (mask.get(node))
        continue;
      res.insertErrorInNode(node, 7220);
    }

    int lcp = 7099;
    res.dumpStateAllNodes(&lcp, 1);

    res.waitClusterNoStart();
    res.startAll();
    if (res.waitClusterStarted())
      return NDBT_FAILED;
  }

  return NDBT_OK;
}

int
runScanUpdateUntilStopped(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  HugoTransactions hugoTrans(*ctx->getTab());

  NullOutputStream null;
  OutputStream * save[1];
  save[0] = g_err.m_out;
  g_err.m_out = &null;
  while (!ctx->isTestStopped())
  {
    hugoTrans.scanUpdateRecords(pNdb, 0);
  }
  g_err.m_out = save[0];
  return NDBT_OK;
}

int
runBug48436(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbRestarter res;
  Uint32 loops = ctx->getNumLoops();
  const Uint32 nodeCount = res.getNumDbNodes();
  if(nodeCount < 2)
  {
    return NDBT_OK;
  }

  for (Uint32 l = 0; l<loops; l++)
  {
    int nodes[2];
    nodes[0] = res.getNode(NdbRestarter::NS_RANDOM);
    nodes[1] = res.getRandomNodeSameNodeGroup(nodes[0], rand());
    int val = 7099;
    int val2[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };

    ndbout_c("nodes %u %u", nodes[0], nodes[1]);

    for (Uint32 j = 0; j<5; j++)
    {
      int c = (rand()) % 11;
      ndbout_c("case: %u", c);
      switch(c){
      case 0:
      case 1:
        res.dumpStateAllNodes(&val, 1);
      case 2:
      case 3:
      case 4:
      case 5:
        res.restartOneDbNode(nodes[0], false, true, true);
        res.waitNodesNoStart(nodes+0,1);
        res.dumpStateOneNode(nodes[0], val2, 2);
        res.insertErrorInNode(nodes[0], 5054); // crash during restart
        res.startAll();
        sleep(3);
        res.waitNodesNoStart(nodes+0,1);
        res.startAll();
        break;
      case 6:
        res.restartOneDbNode(nodes[0], false, true, true);
        res.waitNodesNoStart(nodes+0, 1);
        res.startAll();
        break;
      case 7:
        res.dumpStateAllNodes(&val, 1);
      case 8:
        res.restartOneDbNode(nodes[1], false, true, true);
        res.waitNodesNoStart(nodes+1,1);
        res.dumpStateOneNode(nodes[1], val2, 2);
        res.insertErrorInNode(nodes[1], 5054); // crash during restart
        res.startAll();
        sleep(3);
        res.waitNodesNoStart(nodes+1,1);
        res.startAll();
        break;
      case 9:
        res.restartAll(false, true, true);
        res.waitClusterNoStart();
        res.startAll();
        break;
      case 10:
      {
        res.dumpStateAllNodes(val2, 2);
        int node = res.getMasterNodeId();
        res.insertErrorInNode(node, 7222);
        res.waitClusterNoStart();
        res.startAll();
        break;
      }
      }
      res.waitClusterStarted();
    }
    res.restartAll(false, true, true);
    res.waitClusterNoStart();
    res.startAll();
    res.waitClusterStarted();
  }
  ctx->stopTest();

  return NDBT_OK;
}

int
runBug54611(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbRestarter res;
  Uint32 loops = ctx->getNumLoops();
  Ndb* pNdb = GETNDB(step);
  int rows = ctx->getNumRecords();

  HugoTransactions hugoTrans(*ctx->getTab());

  for (Uint32 l = 0; l<loops; l++)
  {
    int val = DumpStateOrd::DihMinTimeBetweenLCP;
    res.dumpStateAllNodes(&val, 1);

    for (Uint32 i = 0; i < 5; i++)
    {
      hugoTrans.scanUpdateRecords(pNdb, rows);
    }

    int val2[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };
    res.dumpStateAllNodes(val2, 2);

    int node = res.getMasterNodeId();
    res.insertErrorInNode(node, 7222);

    while (hugoTrans.scanUpdateRecords(pNdb, rows) == 0);
    res.waitClusterNoStart();

    res.insertErrorInAllNodes(5055);
    res.startAll();
    res.waitClusterStarted();
    pNdb->waitUntilReady();
  }

  return NDBT_OK;
}

int
runBug56961(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbRestarter res;
  Uint32 loops = ctx->getNumLoops();
  Ndb* pNdb = GETNDB(step);
  int rows = ctx->getNumRecords();

  int node = res.getNode(NdbRestarter::NS_RANDOM);
  int val2[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };
  HugoTransactions hugoTrans(*ctx->getTab());

  for (Uint32 l = 0; l<loops; l++)
  {
    ndbout_c("Waiting for %d to restart (5058)", node);
    res.dumpStateOneNode(node, val2, 2);
    res.insertErrorInNode(node, 5058);

    hugoTrans.clearTable(pNdb);
    hugoTrans.loadTable(pNdb, rows);
    while (hugoTrans.scanUpdateRecords(pNdb, rows) == NDBT_OK &&
           res.getNodeStatus(node) != NDB_MGM_NODE_STATUS_NOT_STARTED &&
           res.getNodeStatus(node) != NDB_MGM_NODE_STATUS_NO_CONTACT);
    res.waitNodesNoStart(&node, 1);
    res.startNodes(&node, 1);
    ndbout_c("Waiting for %d to start", node);
    res.waitClusterStarted();

    ndbout_c("Waiting for %d to restart (5059)", node);
    res.dumpStateOneNode(node, val2, 2);
    res.insertErrorInNode(node, 5059);

    hugoTrans.clearTable(pNdb);
    hugoTrans.loadTable(pNdb, rows);
    while (hugoTrans.scanUpdateRecords(pNdb, rows) == NDBT_OK &&
           res.getNodeStatus(node) != NDB_MGM_NODE_STATUS_NOT_STARTED &&
           res.getNodeStatus(node) != NDB_MGM_NODE_STATUS_NO_CONTACT);
    res.waitNodesNoStart(&node, 1);
    res.startNodes(&node, 1);
    ndbout_c("Waiting for %d to start", node);
    res.waitClusterStarted();
    pNdb->waitUntilReady();
  }

  return NDBT_OK;
}

int runAddNodes(NDBT_Context* ctx, NDBT_Step* step)
{
  /*
   To add new nodes online, the two nodes should be already up in the cluster,
   with nodegroup 65536. Then they can be added to the cluster online using the
   ndb_mgm command create nodegroup. Here,
   1. we retrieve the list of such nodes with ng 65536(internally -256) and
   2. add them to the cluster by passing them to the mgmapi function
      ndb_mgm_create_nodegroup().
   */
  NdbRestarter restarter;

  Vector<int> newNodes;
  int ng;

  /* Retrieve the list of nodes with nodegroup 65536(-256) */
  for(int i= 0; i < restarter.getNumDbNodes(); i++ )
  {
    int _node_id= restarter.getDbNodeId(i);
    if(restarter.getNodeGroup(_node_id) == -256)
    {
      /* nodes that don't have a nodegroup yet */
      newNodes.push_back(_node_id);
    }
  }

  /* if there are no new nodes, can't test add node restart */
  if(newNodes.size() == 0)
  {
    g_err << "ERR: "<< step->getName()
        << " failed on line " << __LINE__ << endl;
    g_err << "Incorrect cluster configuration."
        << "Requires additional nodes with nodegroup 65536." << endl;
    return NDBT_FAILED;
  }

  /* end of array value for newNodes */
  newNodes.push_back(0);

  /* include the new nodes into cluster using ndb_mgm_create_nodegroup() */
  if(ndb_mgm_create_nodegroup(restarter.handle, newNodes.getBase(),
                              &ng, NULL) != 0)
  {
    g_err << "ERR: "<< step->getName()
        << " failed on line " << __LINE__ << endl;
    g_err << ndb_mgm_get_latest_error_desc(restarter.handle) << endl;
    return NDBT_FAILED;
  }
  g_info << "New nodes added to nodegroup " << ng << endl;

  return NDBT_OK;
}

int runAlterTableAndOptimize(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbRestarter restarter;
  /* check if there is a possibility of node killing during redistribution */
  bool nodesKilledDuringStep= ctx->getProperty("NodesKilledDuringStep");

  /* Redistribute existing cluster data */
  DbUtil sql("TEST_DB");
  {
    BaseString query;
    int numOfTables = ctx->getNumTables();

    /* ALTER ONLINE TABLE <tbl_name> REORGANIZE PARTITION */
    for(int i= 0; i < numOfTables; i++ )
    {
      SqlResultSet resultSet;
      query.assfmt("ALTER ONLINE TABLE %s REORGANIZE PARTITION",
                   ctx->getTableName(i));
      g_info << "Executing query : "<< query.c_str() << endl;

      if(!sql.doQuery(query.c_str(), resultSet)){
        if(nodesKilledDuringStep &&
           sql.getErrorNumber() == 0)
        {
          /* query failed probably because of a node kill in another step.
             wait for the nodes to get into start phase before retrying */
          if(restarter.waitClusterStarted() != 0){
            g_err << "Cluster went down during reorganize partition" << endl;
            return NDBT_FAILED;
          }
          /* retry the query for same table */
          i--;
          nodesKilledDuringStep= false;
          continue;
        } else {
          /* either the query failed due to returning error code from server
           or cluster crash */
          g_err << "QUERY : "<< query.c_str() << "; failed" << endl;
          return NDBT_FAILED;
        }
      }
    }

    if(nodesKilledDuringStep){
      /* Nodes were supposed to be killed during alter table,
         but they never were. Test lost its purpose. Mark it as failed
         Mostly won't happen. Just insuring. */
      g_err << "Nodes were never killed during alter table." << endl;
      return NDBT_FAILED;
    }

    /* Reclaim freed space by running optimize table */
    for(int i= 0; i < numOfTables; i++ )
    {
      SqlResultSet result;
      BaseString query;
      query.assfmt("OPTIMIZE TABLE %s", ctx->getTableName(i));
      g_info << "Executing query : "<< query.c_str() << endl;
      if (!sql.doQuery(query.c_str(), result)){
        g_err << "Failed executing optimize table" << endl;
        return NDBT_FAILED;
      }
    }
  }
  return NDBT_OK;
}

int runKillTwoNodes(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbRestarter restarter;
  int val[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };
  int kill[] = { 9999, 3000, 10000 };
  int result = NDBT_OK;

  Vector<int> nodes;

  /* choose first victim */
  nodes.push_back(restarter.getDbNodeId(rand() % restarter.getNumDbNodes()));
  /* select a node from different group as next victim */
  nodes.push_back(restarter.getRandomNodeOtherNodeGroup(nodes[0], rand()));
  for(int i = 0; i < 2; i++){
    g_info << "Killing node " << nodes[i] << "..." << endl;
    CHECK(restarter.dumpStateOneNode(nodes[i], val, 2) == 0);
    CHECK(restarter.dumpStateOneNode(nodes[i], kill, 3) == 0);
  }

  /* wait for both of them to come into no start */
  if(restarter.waitNodesNoStart(nodes.getBase(), 2) != 0)
  {
    g_err << "Nodes never restarted" << endl;
    return NDBT_FAILED;
  }

  /* start the killed nodes */
  if(restarter.startNodes(nodes.getBase(), 2) != 0)
  {
    g_err << "Unable to start killed node." << endl;
    return NDBT_FAILED;
  }

  /* wait for nodes to get started */
  if(restarter.waitNodesStarted(nodes.getBase(), nodes.size()) != 0)
  {
    g_err << "Killed nodes stuck in start phase." << endl;
    return NDBT_FAILED;
  }

  return result;
}

int runRestartOneNode(NDBT_Context* ctx, NDBT_Step* step){
  Ndb* pNdb = GETNDB(step);
  int result = NDBT_OK;
  int timeout = 300;
  int records = ctx->getNumRecords();
  int count;
  NdbRestarter restarter;
  const int nodeCount = restarter.getNumDbNodes();
  if(nodeCount < 2){
    g_info << "RestartOneNode - Needs atleast 2 nodes to test" << endl;
    return NDBT_OK;
  }
  Vector<int> nodeIds;
  for(int i = 0; i<nodeCount; i++)
    nodeIds.push_back(restarter.getDbNodeId(i));
  Uint32 currentRestartNodeIndex = 0;
  HugoTransactions hugoTrans(*ctx->getTab());
  int cnt = nodeCount;
  /**
  1. Load data
  2. One by one restart all nodes with -nostart
  3. Verify records
  **/

  /*** 1 ***/
  g_info << "1- Loading Data " << endl;
  hugoTrans.loadTable(pNdb, records);

  while(cnt-- && result != NDBT_FAILED)
  {
    /*** 2 ***/
    g_info << "2- Restarting node : " << nodeIds[currentRestartNodeIndex]<< endl;

    CHECK(restarter.restartOneDbNode(nodeIds[currentRestartNodeIndex],
                                          false,//Initial
                                          true,//nostart
                                          false//abort
                                          ) == 0);
    CHECK(restarter.waitNodesNoStart(&nodeIds[currentRestartNodeIndex], 1, timeout) == 0);
    CHECK(restarter.startNodes(&nodeIds[currentRestartNodeIndex], 1) == 0);
    CHECK(restarter.waitNodesStarted(&nodeIds[currentRestartNodeIndex], 1, timeout) == 0);
    currentRestartNodeIndex = (currentRestartNodeIndex + 1 ) % nodeCount;
  }

  /*** 3 ***/
  ndbout << "3- Verifying records..." << endl;
  if(hugoTrans.selectCount(pNdb, 64, &count) )
    return NDBT_FAILED;
  if(hugoTrans.clearTable(pNdb))
    return NDBT_FAILED;

  /*** done ***/
  g_info << "runRestartOneNode finished" << endl;
  return result;
}

int runMixedModeRestart(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  int timeout = 300;
  NdbRestarter restarter;
  const int nodeCount = restarter.getNumDbNodes();
  if(nodeCount < 4){
    g_info << "MixedModeRestart - Needs atleast 4 nodes to test" << endl;
    return NDBT_OK;
  }
  Vector<int> nodeIds;
  for(int i = 0; i<nodeCount; i++)
    nodeIds.push_back(restarter.getDbNodeId(i));
  int nodeToKill = nodeIds[0];
  int val[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };
  /**
  1. Killing two nodes of diffrent groups.
  2. Starting nodes with and without --initial option.
  **/

  /*** 1 ***/
  g_info << "1- Killing two nodes..." << endl;
  int otherNodeToKill = restarter.getRandomNodeOtherNodeGroup(nodeToKill,rand());
  if(otherNodeToKill == -1)
    return NDBT_FAILED;

  int kill[] = { 9999, 3000, 10000 };

  g_info <<"    Killing node : "<< nodeToKill << endl;
  if(restarter.dumpStateOneNode(nodeToKill, val, 2))
    return NDBT_FAILED;
  if(restarter.dumpStateOneNode(nodeToKill, kill, 3))
    return NDBT_FAILED;

  g_info <<"    Killing node : "<< otherNodeToKill << endl;
  if(restarter.dumpStateOneNode(otherNodeToKill, val, 2))
    return NDBT_FAILED;
  if(restarter.dumpStateOneNode(otherNodeToKill, kill, 3))
    return NDBT_FAILED;

  /*** 2 ***/
  g_info << "2 - Starting nodes with and without --initial option..." << endl;

  if(restarter.restartOneDbNode(nodeToKill,
                                false,//Initial
                                true,//nostart
                                false//abort
                                ))
    return NDBT_FAILED;
  if(restarter.waitNodesNoStart(&nodeToKill, 1, timeout))
    return NDBT_FAILED;
  if(restarter.startNodes(&nodeToKill, 1))
    return NDBT_FAILED;
  if(restarter.waitNodesStarted(&nodeToKill, 1, timeout))
    return NDBT_FAILED;

  if(restarter.restartOneDbNode(otherNodeToKill,
                                true,//Initial
                                true,//nostart
                                false//abort
                                ))
    return NDBT_FAILED;
  if(restarter.waitNodesNoStart(&otherNodeToKill, 1, timeout))
    return NDBT_FAILED;
  if(restarter.startNodes(&otherNodeToKill, 1))
    return NDBT_FAILED;
  if(restarter.waitNodesStarted(&otherNodeToKill, 1, timeout))
    return NDBT_FAILED;

  /*** done ***/
  g_info << "runMixedModeRestart finished" << endl;
  return result;
}

int runStartWithNodeGroupZero(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  int timeout = 300;
  NdbRestarter restarter;
  const int nodeCount = restarter.getNumDbNodes();
  if(nodeCount < 4){
    g_info << "StartWithNodeGroupZero - Needs atleast 4 nodes to test" << endl;
    return NDBT_OK;
  }
  Vector<int> nodeIds;
  for(int i = 0; i<nodeCount; i++)
    nodeIds.push_back(restarter.getDbNodeId(i));
  int nodeId = nodeIds[0];
  int cnt = nodeCount;
  int nodeGroup = 0;
  while(cnt-- && nodeGroup == 0 && result != NDBT_FAILED)
  {
    /**
    1. Finding a node of group id other then 0.
    2. Restart that node
    3. Check the group id of the above node
    **/
    /*** 1 ***/
    g_info << "1- Findind a node of group id other then 0" << endl;
    nodeGroup = restarter.getNodeGroup(nodeId);
    g_info << "    Current node group : " << nodeGroup << endl;
    if(nodeGroup == 0)
    {
      g_info << "    Skiping this node" << endl;
      nodeId = restarter.getRandomNodeOtherNodeGroup(nodeId, 4);
      continue;
    }

    /*** 2 ***/
    g_info << "2- Restarting node : " << nodeId << " whose Group id is "
           << nodeGroup << endl;

    CHECK(restarter.restartOneDbNode(nodeId,
                                     true,//Initial
                                     true,//nostart
                                     false//abort
                                     ) == 0);
    CHECK(restarter.waitNodesNoStart(&nodeId, 1, timeout) == 0);
    CHECK(restarter.startNodes(&nodeId, 1) == 0);
    CHECK(restarter.waitNodesStarted(&nodeId, 1, timeout) == 0);
    nodeGroup = restarter.getNodeGroup(nodeId);
    /*** 3 ***/
    g_info << "3- Checking its group id" << endl;
    CHECK(nodeGroup !=0)
    g_info << "    current node group : " << nodeGroup << endl;
  }

  /*** done ***/
  g_info << "runStartWithNodeGroupZero finished" << endl;

  return result;
}

int runMixedModeRestart4Node(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  NdbRestarter restarter;
  const int nodeCount = restarter.getNumDbNodes();
  if(nodeCount < 8){
    g_info << "MixedModeRestart4Node - Needs atleast 8 nodes to test" << endl;
    return NDBT_OK;
  }
  Vector<int> nodeIds;
  for(int i = 0; i<nodeCount; i++)
    nodeIds.push_back(restarter.getDbNodeId(i));
  int val[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };
  /**
  1. Killing four nodes of diffrent groups.
  2. Starting nodes with and without --initial option.
  **/

  /*** 1 ***/
  g_info << "1- Killing four nodes of diffrent groups." << endl;
  int nodesarray[256];
  int cnt = 0;
  int timeout = 300;
  Bitmask<4> seen_groups;
  for(int i = 0; i< nodeCount; i++)
  {
    int nodeGroup=restarter.getNodeGroup(nodeIds[i]);
    if (seen_groups.get(nodeGroup))
    {
      // One node in this node group already down
      g_info << "    Continuing as one node from this group is already killed."
             << " NodeGroup = " << nodeGroup << endl;
      continue;
    }
    seen_groups.set(nodeGroup);
    int kill[] = { 9999, 3000, 10000 };
    g_info <<"    Killing node : "<< nodeIds[i] << endl;
    CHECK(restarter.dumpStateOneNode(nodeIds[i], val, 2) == 0);
    CHECK(restarter.dumpStateOneNode(nodeIds[i], kill, 3) == 0);
    nodesarray[cnt++] = nodeIds[i];
  }

  /*** 2 ***/
  g_info << "2- Starting nodes with and without --initial option." << endl;
  bool flag = true;
  for(int i = 0; i < cnt; i++)
  {
    CHECK(restarter.restartOneDbNode(nodesarray[i],
                                     flag,//Initial
                                     true,//nostart
                                     false//abort
                                     ) == 0);
    CHECK(restarter.waitNodesNoStart(&nodesarray[i], 1, timeout) == 0);
    CHECK(restarter.startNodes(&nodesarray[i], 1) == 0);
    CHECK(restarter.waitNodesStarted(&nodesarray[i], 1, timeout) == 0);
    flag = false;
  }

   /*** done ***/
  g_info << "runMixedModeRestart4Node finished" << endl;
  return result;
}

int runKillMasterNodes(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  NdbRestarter restarter;
  const int nodeCount = restarter.getNumDbNodes();
  if(nodeCount < 4){
    g_info << "KillMasterNodes - Needs atleast 4 nodes to test" << endl;
    return NDBT_OK;
  }

  Vector<int> nodeIds;
  for(int i = 0; i<nodeCount; i++)
    nodeIds.push_back(restarter.getDbNodeId(i));
  int val[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };
  int kill[] = { 9999, 3000, 10000 };
  /**
  1. Killing only master node one by one.
  2. Start nodes without --initial option.
  **/

  /*** 1 ***/
  g_info << "1- Killing only master node one by one." << endl;
  int nodesarray[256];
  int timeout = 120;
  int cnt= 0;
  Bitmask<8> seen_groups;
  int master = restarter.getMasterNodeId();
  int newMaster;
  for(int i = 0; i< nodeCount; i++)
  {
    g_info << "Master Node Id : " << master << endl;
    int nodeGroup = restarter.getNodeGroup(master);
    CHECK(nodeGroup != -1);
    if (seen_groups.get(nodeGroup))
    {
      // One node in this node group already down
      g_info << "Breaking because master node belongs to the group whoes one"
      << "node is already down. Master = " << master << ", node Group = "
      << nodeGroup << endl;
      break;
    }
    seen_groups.set(nodeGroup);
    nodesarray[cnt++] = master;
    newMaster = restarter.getNextMasterNodeId(master);
    g_info <<"   killing node : "<< master << " group : " << nodeGroup << endl;
    CHECK(restarter.dumpStateOneNode(master, val, 2) == 0);
    CHECK(restarter.dumpStateOneNode(master, kill, 3) == 0);
    CHECK(restarter.waitNodesNoStart(&master, 1) == 0);
    master = newMaster;
  }

  /*** 2 ***/
  g_info << "2- Starting nodes without --initial option..." << endl;
  for(int i = 0; i<cnt; i++)
  {
    CHECK(restarter.startNodes(&nodesarray[i], 1) == 0);
    CHECK(restarter.waitNodesStarted(&nodesarray[i], 1, timeout) == 0);
  }

  /*** done ***/
  g_info << "runKillMasterNodes finished" << endl;
  return result;
}

NDBT_TESTSUITE(testSystemRestart);
TESTCASE("SR1", 
	 "Basic system restart test. Focus on testing restart from REDO log.\n"
	 "NOTE! Time between lcp's and gcp's should be left at default, \n"
	 "so that Ndb  uses the Redo log when restarting\n" 
	 "1. Load records\n"
	 "2. Restart cluster and verify records \n"
	 "3. Update records\n"
	 "4. Restart cluster and verify records \n"
	 "5. Delete half of the records \n"
	 "6. Restart cluster and verify records \n"
	 "7. Delete all records \n"
	 "8. Restart cluster and verify records \n"
	 "9. Insert, update, delete records \n"
	 "10. Restart cluster and verify records\n"
	 "11. Insert, update, delete records \n"
	 "12. Restart cluster with error insert 5020 and verify records\n"){ 
  INITIALIZER(runWaitStarted);
  STEP(runSystemRestart1);
}
TESTCASE("SR2", 
	 "Basic system restart test. Focus on testing restart from LCP\n"
	 "NOTE! Time between lcp's is automatically set to it's  min value\n"
	 "so that Ndb  uses LCP's when restarting.\n" 
	 "1. Load records\n"
	 "2. Restart cluster and verify records \n"
	 "3. Update records\n"
	 "4. Restart cluster and verify records \n"
	 "5. Delete half of the records \n"
	 "6. Restart cluster and verify records \n"
	 "7. Delete all records \n"
	 "8. Restart cluster and verify records \n"
	 "9. Insert, update, delete records \n"
	 "10. Restart cluster and verify records\n"){
  INITIALIZER(runWaitStarted);
  STEP(runSystemRestart2);
}
TESTCASE("SR_UNDO", 
	 "System restart test. Focus on testing of undologging\n"
	 "in DBACC and DBTUP.\n"
	 "This is done by starting a LCP, turn on undologging \n"
	 "but don't start writing the datapages. This will force all\n"
	 "operations to be written into the undolog.\n"
	 "Then write datapages and complete LCP.\n"
	 "Restart the system\n"){
  INITIALIZER(runWaitStarted);
  STEP(runSystemRestartTestUndoLog);
}
TESTCASE("SR_FULLDB", 
	 "System restart test. Test to restart when DB is full.\n"){
  INITIALIZER(runWaitStarted);
  STEP(runSystemRestartTestFullDb);
}
TESTCASE("SR3", 
	 "System restart test. Focus on testing restart from with\n"
	 "not all nodes alive when system went down\n"
	 "* 1. Load data\n"
	 "* 2. Restart 1 node -nostart\n"
	 "* 3. Update records\n"
	 "* 4. Restart cluster and verify records\n"
	 "* 5. Restart 1 node -nostart\n"
	 "* 6. Delete half of the records\n"
	 "* 7. Restart cluster and verify records\n"
	 "* 8. Restart 1 node -nostart\n"
	 "* 9. Delete all records\n"
	 "* 10. Restart cluster and verify records\n"){
  INITIALIZER(runWaitStarted);
  STEP(runSystemRestart3);
}
TESTCASE("SR4", 
	 "System restart test. Focus on testing restart from with\n"
	 "not all nodes alive when system went down but running LCP at\n"
	 "high speed so that sometimes a TO is required to start cluster\n"
	 "* 1. Load data\n"
	 "* 2. Restart 1 node -nostart\n"
	 "* 3. Update records\n"
	 "* 4. Restart cluster and verify records\n"
	 "* 5. Restart 1 node -nostart\n"
	 "* 6. Delete half of the records\n"
	 "* 7. Restart cluster and verify records\n"
	 "* 8. Restart 1 node -nostart\n"
	 "* 9. Delete all records\n"
	 "* 10. Restart cluster and verify records\n"){
  INITIALIZER(runWaitStarted);
  STEP(runSystemRestart4);
}
TESTCASE("SR5", 
	 "As SR4 but making restart aborts\n"
	 "* 1. Load data\n"
	 "* 2. Restart 1 node -nostart\n"
	 "* 3. Update records\n"
	 "* 4. Restart cluster and verify records\n"
	 "* 5. Restart 1 node -nostart\n"
	 "* 6. Delete half of the records\n"
	 "* 7. Restart cluster and verify records\n"
	 "* 8. Restart 1 node -nostart\n"
	 "* 9. Delete all records\n"
	 "* 10. Restart cluster and verify records\n"){
  INITIALIZER(runWaitStarted);
  STEP(runSystemRestart5);
}
TESTCASE("SR6", 
	 "Perform system restart with some nodes having FS others wo/\n"
	 "* 1. Load data\n"
	 "* 2. Restart all node -nostart\n"
	 "* 3. Restart some nodes -i -nostart\n"
	 "* 4. Start all nodes verify records\n"){
  INITIALIZER(runWaitStarted);
  INITIALIZER(runClearTable);
  STEP(runSystemRestart6);
}
TESTCASE("SR7", 
	 "Perform partition win system restart\n"
	 "* 1. Load data\n"
	 "* 2. Restart all node -nostart\n"
	 "* 3. Start all but one node\n"
	 "* 4. Verify records\n"
	 "* 5. Start last node\n"
	 "* 6. Verify records\n"){
  INITIALIZER(runWaitStarted);
  INITIALIZER(runClearTable);
  STEP(runSystemRestart7);
}
TESTCASE("SR8", 
	 "Perform partition win system restart with other nodes delayed\n"
	 "* 1. Load data\n"
	 "* 2. Restart all node -nostart\n"
	 "* 3. Start all but one node\n"
	 "* 4. Wait for startphase >= 2\n"
	 "* 5. Start last node\n"
	 "* 6. Verify records\n"){
  INITIALIZER(runWaitStarted);
  INITIALIZER(runClearTable);
  STEP(runSystemRestart8);
}
TESTCASE("SR9", 
	 "Perform partition win system restart with other nodes delayed\n"
	 "* 1. Start transaction\n"
	 "* 2. insert (1,1)\n"
	 "* 3. update (1,2)\n"
	 "* 4. start lcp\n"
	 "* 5. commit\n"
	 "* 6. restart\n"){
  INITIALIZER(runWaitStarted);
  INITIALIZER(runClearTable);
  STEP(runSystemRestart9);
}
TESTCASE("SR10", 
     "More tests of partitioned system restarts\n")
{
  INITIALIZER(runWaitStarted);
  INITIALIZER(runClearTable);
  STEP(runSystemRestart10);
}
TESTCASE("Bug18385", 
	 "Perform partition system restart with other nodes with higher GCI"){
  INITIALIZER(runWaitStarted);
  INITIALIZER(runClearTable);
  STEP(runBug18385);
}
TESTCASE("Bug21536", 
	 "Perform partition system restart with other nodes with higher GCI"){
  INITIALIZER(runWaitStarted);
  INITIALIZER(runClearTable);
  STEP(runBug21536);
}
TESTCASE("Bug24664",
	 "Check handling of LCP skip/keep")
{
  INITIALIZER(runWaitStarted);
  INITIALIZER(runClearTable);
  STEP(runBug24664);
}
TESTCASE("Bug27434",
	 "")
{
  INITIALIZER(runWaitStarted);
  STEP(runBug27434);
}
TESTCASE("SR_DD_1", "")
{
  TC_PROPERTY("ALL", 1);
  INITIALIZER(runWaitStarted);
  INITIALIZER(clearOldBackups);
  STEP(runStopper);
  STEP(runSR_DD_1);
}
TESTCASE("SR_DD_1b", "")
{
  INITIALIZER(runWaitStarted);
  INITIALIZER(clearOldBackups);
  STEP(runSR_DD_1);
}
TESTCASE("SR_DD_1_LCP", "")
{
  TC_PROPERTY("ALL", 1);
  TC_PROPERTY("LCP", 1);
  INITIALIZER(runWaitStarted);
  INITIALIZER(clearOldBackups);
  STEP(runStopper);
  STEP(runSR_DD_1);
}
TESTCASE("SR_DD_1b_LCP", "")
{
  TC_PROPERTY("LCP", 1);
  INITIALIZER(runWaitStarted);
  INITIALIZER(clearOldBackups);
  STEP(runSR_DD_1);
}
TESTCASE("SR_DD_2", "")
{
  TC_PROPERTY("ALL", 1);
  INITIALIZER(runWaitStarted);
  INITIALIZER(clearOldBackups);
  STEP(runStopper);
  STEP(runSR_DD_2);
}
TESTCASE("SR_DD_2b", "")
{
  INITIALIZER(runWaitStarted);
  INITIALIZER(clearOldBackups);
  STEP(runSR_DD_2);
}
TESTCASE("SR_DD_2_LCP", "")
{
  TC_PROPERTY("ALL", 1);
  TC_PROPERTY("LCP", 1);
  INITIALIZER(runWaitStarted);
  INITIALIZER(clearOldBackups);
  STEP(runStopper);
  STEP(runSR_DD_2);
}
TESTCASE("SR_DD_2b_LCP", "")
{
  TC_PROPERTY("LCP", 1);
  INITIALIZER(runWaitStarted);
  INITIALIZER(clearOldBackups);
  STEP(runSR_DD_2);
}
TESTCASE("SR_DD_3", "")
{
  TC_PROPERTY("ALL", 1);
  INITIALIZER(runWaitStarted);
  INITIALIZER(clearOldBackups);
  STEP(runStopper);
  STEP(runSR_DD_3);
}
TESTCASE("SR_DD_3b", "")
{
  INITIALIZER(runWaitStarted);
  INITIALIZER(clearOldBackups);
  STEP(runSR_DD_3);
}
TESTCASE("SR_DD_3_LCP", "")
{
  TC_PROPERTY("ALL", 1);
  TC_PROPERTY("LCP", 1);
  INITIALIZER(runWaitStarted);
  INITIALIZER(clearOldBackups);
  STEP(runStopper);
  STEP(runSR_DD_3);
}
TESTCASE("SR_DD_3b_LCP", "")
{
  TC_PROPERTY("LCP", 1);
  INITIALIZER(runWaitStarted);
  INITIALIZER(clearOldBackups);
  STEP(runSR_DD_3);
}
TESTCASE("Bug29167", "")
{
  INITIALIZER(runWaitStarted);
  STEP(runBug29167);
}
TESTCASE("Bug28770",
         "Check readTableFile1 fails, readTableFile2 succeeds\n"
         "1. Restart all node -nostart\n"
         "2. Insert error 6100 into all nodes\n"
         "3. Start all nodes\n"
         "4. Ensure cluster start\n"
         "5. Read and verify reocrds\n"
         "6. Repeat until looping is completed\n"){
  INITIALIZER(runWaitStarted);
  INITIALIZER(runClearTable);
  STEP(runBug28770);
}
TESTCASE("Bug22696", "")
{
  INITIALIZER(runWaitStarted);
  INITIALIZER(runLoadTable);
  INITIALIZER(runBug22696);
}
TESTCASE("to", "Take-over during SR")
{
  INITIALIZER(runWaitStarted);
  INITIALIZER(runLoadTable);
  INITIALIZER(runTO);
}
TESTCASE("basic", "")
{
  INITIALIZER(runWaitStarted);
  INITIALIZER(runCreateAllTables);
  STEP(runBasic);
  FINALIZER(runDropAllTables);
}
TESTCASE("Bug41915", "")
{
  TC_PROPERTY("ALL", 1);
  TC_PROPERTY("ERROR", 5053);
  TC_PROPERTY("ROWS", 30);
  INITIALIZER(runWaitStarted);
  STEP(runStopper);
  STEP(runSR_DD_2);
}
TESTCASE("Bug45154", "")
{
  INITIALIZER(runBug45154);
}
TESTCASE("Bug46651", "")
{
  INITIALIZER(runBug46651);
}
TESTCASE("Bug46412", "")
{
  INITIALIZER(runBug46412);
}
TESTCASE("Bug48436", "")
{
  INITIALIZER(runLoadTable);
  STEP(runBug48436);
  STEP(runScanUpdateUntilStopped);
}
TESTCASE("Bug54611", "")
{
  INITIALIZER(runLoadTable);
  INITIALIZER(runBug54611);
}
TESTCASE("Bug56961", "")
{
  INITIALIZER(runLoadTable);
  INITIALIZER(runBug56961);
}
TESTCASE("MTR_AddNodesAndRestart1",
         "1. Insert few rows to table"
         "2. Add nodes to the cluster"
         "3. Reorganize partition and optimize table"
         "Should be run only once")
{
  ALL_TABLES();
  INITIALIZER(runWaitStarted);
  INITIALIZER(runFillTable);
  INITIALIZER(runAddNodes);
  STEP(runAlterTableAndOptimize);
  VERIFIER(runVerifyFilledTables);
}
TESTCASE("MTR_AddNodesAndRestart2",
         "1. Fill the table fully"
         "2. Add nodes to the cluster"
         "3. Reorganize partition and optimize table"
         "4. Kill 2 nodes during reorganization"
         "Should be run only once")
{
  ALL_TABLES();
  TC_PROPERTY("NodesKilledDuringStep", true);
  INITIALIZER(runWaitStarted);
  INITIALIZER(runFillTable);
  INITIALIZER(runAddNodes);
  STEP(runAlterTableAndOptimize);
  STEP(runKillTwoNodes);
  VERIFIER(runVerifyFilledTables);
}
TESTCASE("RestartOneNode",
	 "Perform one nodes restart\n"
	 "* 1. Load data\n"
	 "* 2. Restart 1 node\n"
	 "* 3. Verify records\n"){
  INITIALIZER(runWaitStarted);
  STEP(runRestartOneNode);
}
TESTCASE("MixedModeRestart",
         "Perform kiiling of two node and starting them\n"
         "* 1. Killing two nodes of diffrent groups\n"
         "* 2. Starting nodes with and without --initial option\n"){
  INITIALIZER(runWaitStarted);
  STEP(runMixedModeRestart);
}
TESTCASE("StartWithNodeGroupZero",
         "check that a node doesn't always attached to group 0 while restart\n"
         "* 1. Finding a node of group id other then 0\n"
         "* 2. Restart that node\n"
         "* 3. Check the group id of the above node\n"){
  INITIALIZER(runWaitStarted);
  STEP(runStartWithNodeGroupZero);
}
TESTCASE("MixedModeRestart4Node",
         "Perform killing of four nodes and starting them\n"
         "* 1. Killing four nodes of diffrent groups\n"
         "* 2. Starting nodes with and without --initial option\n"){
  INITIALIZER(runWaitStarted);
  STEP(runMixedModeRestart4Node);
}
TESTCASE("KillMasterNodes",
	 "perform Killing of master node and then starting them\n"
	 "* 1. Killing only the master nodes one by one\n"
         "* 2. Start without --initial option\n"){
  INITIALIZER(runWaitStarted);
  STEP(runKillMasterNodes);
}
NDBT_TESTSUITE_END(testSystemRestart);

int main(int argc, const char** argv){
  ndb_init();
  NDBT_TESTSUITE_INSTANCE(testSystemRestart);
  return testSystemRestart.execute(argc, argv);
}
