/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <NDBT.hpp>
#include <NDBT_Test.hpp>
#include <HugoTransactions.hpp>
#include <UtilTransactions.hpp>
#include <NdbRestarter.hpp>
#include <Vector.hpp>
#include <signaldata/DumpStateOrd.hpp>

int runLoadTable(NDBT_Context* ctx, NDBT_Step* step){

  int records = ctx->getNumRecords();
  HugoTransactions hugoTrans(*ctx->getTab());
  if (hugoTrans.loadTable(GETNDB(step), records) != 0){
    return NDBT_FAILED;
  }
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
    CHECK(hugoTrans.scanUpdateRecords(pNdb, records) == 0);
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
    CHECK(hugoTrans.scanUpdateRecords(pNdb, records) == 0);
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
    CHECK(hugoTrans.scanUpdateRecords(pNdb, records) == 0);
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
    CHECK(hugoTrans.scanUpdateRecords(pNdb, records) == 0);
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
  Uint32 loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  NdbRestarter restarter;
  Uint32 i = 1;
  
  Uint32 currentRestartNodeIndex = 1;
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
  FINALIZER(runClearTable);
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
  FINALIZER(runClearTable);
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
  FINALIZER(runClearTable);
}
TESTCASE("SR_FULLDB", 
	 "System restart test. Test to restart when DB is full.\n"){
  INITIALIZER(runWaitStarted);
  STEP(runSystemRestartTestFullDb);
  FINALIZER(runClearTable);
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
  FINALIZER(runClearTable);
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
  FINALIZER(runClearTable);
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
  FINALIZER(runClearTable);
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
  FINALIZER(runClearTable);
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
  FINALIZER(runClearTable);
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
  FINALIZER(runClearTable);
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
  FINALIZER(runClearTable);
}
NDBT_TESTSUITE_END(testSystemRestart);

int main(int argc, const char** argv){
  ndb_init();
  return testSystemRestart.execute(argc, argv);
}


