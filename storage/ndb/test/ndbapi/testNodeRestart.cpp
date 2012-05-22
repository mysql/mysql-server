/*
   Copyright (c) 2003, 2010, Oracle and/or its affiliates. All rights reserved.

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
#include <NdbRestarts.hpp>
#include <Vector.hpp>
#include <signaldata/DumpStateOrd.hpp>
#include <Bitmask.hpp>
#include <RefConvert.hpp>
#include <NdbEnv.h>
#include <NdbMgmd.hpp>

int runLoadTable(NDBT_Context* ctx, NDBT_Step* step){

  int records = ctx->getNumRecords();
  HugoTransactions hugoTrans(*ctx->getTab());
  if (hugoTrans.loadTable(GETNDB(step), records) != 0){
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int runFillTable(NDBT_Context* ctx, NDBT_Step* step){

  HugoTransactions hugoTrans(*ctx->getTab());
  if (hugoTrans.fillTable(GETNDB(step)) != 0){
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int runInsertUntilStopped(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  int records = ctx->getNumRecords();
  int i = 0;
  HugoTransactions hugoTrans(*ctx->getTab());
  while (ctx->isTestStopped() == false) {
    g_info << i << ": ";    
    if (hugoTrans.loadTable(GETNDB(step), records) != 0){
      return NDBT_FAILED;
    }
    i++;
  }
  return result;
}

int runClearTable(NDBT_Context* ctx, NDBT_Step* step){
  int records = ctx->getNumRecords();
  
  UtilTransactions utilTrans(*ctx->getTab());
  if (utilTrans.clearTable(GETNDB(step),  records) != 0){
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int runClearTableUntilStopped(NDBT_Context* ctx, NDBT_Step* step){
  int records = ctx->getNumRecords();
  int i = 0;
  
  UtilTransactions utilTrans(*ctx->getTab());
  while (ctx->isTestStopped() == false) {
    g_info << i << ": ";    
    if (utilTrans.clearTable(GETNDB(step),  records) != 0){
      return NDBT_FAILED;
    }
    i++;
  }
  return NDBT_OK;
}

int runScanReadUntilStopped(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  int records = ctx->getNumRecords();
  int i = 0;
  HugoTransactions hugoTrans(*ctx->getTab());
  while (ctx->isTestStopped() == false) {
    g_info << i << ": ";
    if (hugoTrans.scanReadRecords(GETNDB(step), records) != 0){
      return NDBT_FAILED;
    }
    i++;
  }
  return result;
}

int runPkReadUntilStopped(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  int records = ctx->getNumRecords();
  NdbOperation::LockMode lm = 
    (NdbOperation::LockMode)ctx->getProperty("ReadLockMode", 
					     (Uint32)NdbOperation::LM_Read);
  int i = 0;
  HugoTransactions hugoTrans(*ctx->getTab());
  while (ctx->isTestStopped() == false) {
    g_info << i << ": ";
    int rows = (rand()%records)+1;
    int batch = (rand()%rows)+1;
    if (hugoTrans.pkReadRecords(GETNDB(step), rows, batch, lm) != 0){
      return NDBT_FAILED;
    }
    i++;
  }
  return result;
}

int runPkUpdateUntilStopped(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  int records = ctx->getNumRecords();
  int multiop = ctx->getProperty("MULTI_OP", 1);
  Ndb* pNdb = GETNDB(step);
  int i = 0;

  HugoOperations hugoOps(*ctx->getTab());
  while (ctx->isTestStopped() == false)
  {
    g_info << i << ": ";
    int batch = (rand()%records)+1;
    int row = rand() % records;

    if (batch > 25)
      batch = 25;

    if(row + batch > records)
      batch = records - row;

    if(hugoOps.startTransaction(pNdb) != 0)
      goto err;

    if(hugoOps.pkUpdateRecord(pNdb, row, batch, rand()) != 0)
      goto err;

    for (int j = 1; j<multiop; j++)
    {
      if(hugoOps.execute_NoCommit(pNdb) != 0)
	goto err;

      if(hugoOps.pkUpdateRecord(pNdb, row, batch, rand()) != 0)
        goto err;
    }

    if(hugoOps.execute_Commit(pNdb) != 0)
      goto err;

    hugoOps.closeTransaction(pNdb);

    continue;

err:
    NdbConnection* pCon = hugoOps.getTransaction();
    if(pCon == 0)
      continue;
    NdbError error = pCon->getNdbError();
    hugoOps.closeTransaction(pNdb);
    if (error.status == NdbError::TemporaryError){
      NdbSleep_MilliSleep(50);
      continue;
    }
    return NDBT_FAILED;

    i++;
  }
  return result;
}

int runPkReadPkUpdateUntilStopped(NDBT_Context* ctx, NDBT_Step* step)
{
  int records = ctx->getNumRecords();
  Ndb* pNdb = GETNDB(step);
  int i = 0;
  HugoOperations hugoOps(*ctx->getTab());
  while (ctx->isTestStopped() == false) {
    g_info << i++ << ": ";
    int rows = (rand()%records)+1;
    int batch = (rand()%rows)+1;
    int row = (records - rows) ? rand() % (records - rows) : 0;
    
    int j,k;
    for(j = 0; j<rows; j += batch)
    {
      k = batch;
      if(j+k > rows)
	k = rows - j;
      
      if(hugoOps.startTransaction(pNdb) != 0)
	goto err;
      
      if(hugoOps.pkReadRecord(pNdb, row+j, k, NdbOperation::LM_Exclusive) != 0)
	goto err;

      if(hugoOps.execute_NoCommit(pNdb) != 0)
	goto err;

      if(hugoOps.pkUpdateRecord(pNdb, row+j, k, rand()) != 0)
	goto err;

      if(hugoOps.execute_Commit(pNdb) != 0)
	goto err;

      if(hugoOps.closeTransaction(pNdb) != 0)
	return NDBT_FAILED;
    }
    
    continue;
err:
    NdbConnection* pCon = hugoOps.getTransaction();
    if(pCon == 0)
      continue;
    NdbError error = pCon->getNdbError();
    hugoOps.closeTransaction(pNdb);
    if (error.status == NdbError::TemporaryError){
      NdbSleep_MilliSleep(50);
      continue;
    }
    return NDBT_FAILED;    
  }
  return NDBT_OK;
}

int runPkReadPkUpdatePkUnlockUntilStopped(NDBT_Context* ctx, NDBT_Step* step)
{
  int records = ctx->getNumRecords();
  Ndb* pNdb = GETNDB(step);
  int i = 0;
  HugoOperations hugoOps(*ctx->getTab());
  while (ctx->isTestStopped() == false) {
    g_info << i++ << ": ";
    int rows = (rand()%records)+1;
    int batch = (rand()%rows)+1;
    int row = (records - rows) ? rand() % (records - rows) : 0;
    
    int j,k;
    for(j = 0; j<rows; j += batch)
    {
      k = batch;
      if(j+k > rows)
	k = rows - j;
      
      Vector<const NdbLockHandle*> lockHandles;

      if(hugoOps.startTransaction(pNdb) != 0)
	goto err;
      
      if(hugoOps.pkReadRecordLockHandle(pNdb, lockHandles, row+j, k, NdbOperation::LM_Exclusive) != 0)
	goto err;

      if(hugoOps.execute_NoCommit(pNdb) != 0)
	goto err;

      if(hugoOps.pkUpdateRecord(pNdb, row+j, k, rand()) != 0)
	goto err;

      if(hugoOps.execute_NoCommit(pNdb) != 0)
	goto err;

      if(hugoOps.pkUnlockRecord(pNdb, lockHandles) != 0)
        goto err;

      if(hugoOps.execute_Commit(pNdb) != 0)
	goto err;

      if(hugoOps.closeTransaction(pNdb) != 0)
	return NDBT_FAILED;
    }
    
    continue;
err:
    NdbConnection* pCon = hugoOps.getTransaction();
    if(pCon == 0)
      continue;
    NdbError error = pCon->getNdbError();
    hugoOps.closeTransaction(pNdb);
    if (error.status == NdbError::TemporaryError){
      NdbSleep_MilliSleep(50);
      continue;
    }
    return NDBT_FAILED;    
  }
  return NDBT_OK;
}

int runDeleteInsertUntilStopped(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  int records = ctx->getNumRecords();
  int i = 0;
  HugoTransactions hugoTrans(*ctx->getTab());
  UtilTransactions utilTrans(*ctx->getTab());
  while (ctx->isTestStopped() == false) 
  {
    g_info << i << ": ";    
    if (utilTrans.clearTable(GETNDB(step),  records) != 0){
      result = NDBT_FAILED;
      break;
    }
    if (hugoTrans.loadTable(GETNDB(step), records, 1) != 0){
      result = NDBT_FAILED;
      break;
    }
    i++;
  }

  return result;
}

int runScanUpdateUntilStopped(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  int records = ctx->getNumRecords();
  int parallelism = ctx->getProperty("Parallelism", 1);
  int abort = ctx->getProperty("AbortProb", (Uint32)0);
  int check = ctx->getProperty("ScanUpdateNoRowCountCheck", (Uint32)0);
  
  if (check)
    records = 0;
  
  int i = 0;
  HugoTransactions hugoTrans(*ctx->getTab());
  while (ctx->isTestStopped() == false) {
    g_info << i << ": ";
    if (hugoTrans.scanUpdateRecords(GETNDB(step), records, abort, 
				    parallelism) == NDBT_FAILED){
      return NDBT_FAILED;
    }
    i++;
  }
  return result;
}

int runScanReadVerify(NDBT_Context* ctx, NDBT_Step* step){
  int records = ctx->getNumRecords();
  HugoTransactions hugoTrans(*ctx->getTab());

  if (hugoTrans.scanReadRecords(GETNDB(step), records, 0, 64) != 0){
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int runRestarter(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  int sync_threads = ctx->getProperty("SyncThreads", (unsigned)0);
  int sleep0 = ctx->getProperty("Sleep0", (unsigned)0);
  int sleep1 = ctx->getProperty("Sleep1", (unsigned)0);
  int randnode = ctx->getProperty("RandNode", (unsigned)0);
  NdbRestarter restarter;
  int i = 0;
  int lastId = 0;

  if (restarter.getNumDbNodes() < 2){
    ctx->stopTest();
    return NDBT_OK;
  }

  if(restarter.waitClusterStarted() != 0){
    g_err << "Cluster failed to start" << endl;
    return NDBT_FAILED;
  }
  
  loops *= (restarter.getNumDbNodes() > 2 ? 2 : restarter.getNumDbNodes());
  if (loops < restarter.getNumDbNodes())
    loops = restarter.getNumDbNodes();

  while(i<loops && result != NDBT_FAILED && !ctx->isTestStopped()){

    int id = lastId % restarter.getNumDbNodes();
    if (randnode == 1)
    {
      id = rand() % restarter.getNumDbNodes();
    }
    int nodeId = restarter.getDbNodeId(id);
    ndbout << "Restart node " << nodeId << endl; 
    if(restarter.restartOneDbNode(nodeId, false, true, true) != 0){
      g_err << "Failed to restartNextDbNode" << endl;
      result = NDBT_FAILED;
      break;
    }    

    if (restarter.waitNodesNoStart(&nodeId, 1))
    {
      g_err << "Failed to waitNodesNoStart" << endl;
      result = NDBT_FAILED;
      break;
    }

    if (sleep1)
      NdbSleep_MilliSleep(sleep1);

    if (restarter.startNodes(&nodeId, 1))
    {
      g_err << "Failed to start node" << endl;
      result = NDBT_FAILED;
      break;
    }

    if(restarter.waitClusterStarted() != 0){
      g_err << "Cluster failed to start" << endl;
      result = NDBT_FAILED;
      break;
    }

    if (sleep0)
      NdbSleep_MilliSleep(sleep0);

    ctx->sync_up_and_wait("PauseThreads", sync_threads);

    lastId++;
    i++;
  }

  ctx->stopTest();
  
  return result;
}

int runCheckAllNodesStarted(NDBT_Context* ctx, NDBT_Step* step){
  NdbRestarter restarter;

  if(restarter.waitClusterStarted(1) != 0){
    g_err << "All nodes was not started " << endl;
    return NDBT_FAILED;
  }
  
  return NDBT_OK;
}



int runRestarts(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  NDBT_TestCase* pCase = ctx->getCase();
  NdbRestarts restarts;
  int i = 0;
  int timeout = 240;

  while (i<loops && result != NDBT_FAILED && !ctx->isTestStopped())
  {
    int safety = 0;
    if (i > 0)
      safety = 15;

    if (ctx->closeToTimeout(safety))
      break;

    if(restarts.executeRestart(ctx, pCase->getName(), timeout, safety) != 0){
      g_err << "Failed to executeRestart(" <<pCase->getName() <<")" << endl;
      result = NDBT_FAILED;
      break;
    }
    i++;
  }
  ctx->stopTest();
  return result;
}

int runDirtyRead(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  NdbRestarter restarter;
  HugoOperations hugoOps(*ctx->getTab());
  Ndb* pNdb = GETNDB(step);
    
  int i = 0;
  while(i<loops && result != NDBT_FAILED && !ctx->isTestStopped()){
    g_info << i << ": ";

    int id = i % restarter.getNumDbNodes();
    int nodeId = restarter.getDbNodeId(id);
    ndbout << "Restart node " << nodeId << endl; 
    restarter.insertErrorInNode(nodeId, 5041);
    restarter.insertErrorInAllNodes(8048 + (i & 1));
    
    for(int j = 0; j<records; j++){
      if(hugoOps.startTransaction(pNdb) != 0)
	return NDBT_FAILED;
      
      if(hugoOps.pkReadRecord(pNdb, j, 1, NdbOperation::LM_CommittedRead) != 0)
	goto err;
      
      int res;
      if((res = hugoOps.execute_Commit(pNdb)) == 4119)
	goto done;
      
      if(res != 0)
	goto err;
      
      if(hugoOps.closeTransaction(pNdb) != 0)
	return NDBT_FAILED;
    }
done:
    if(hugoOps.closeTransaction(pNdb) != 0)
      return NDBT_FAILED;
    
    i++;
    restarter.waitClusterStarted(60) ;
  }
  return result;
err:
  hugoOps.closeTransaction(pNdb);
  return NDBT_FAILED;
}

int runLateCommit(NDBT_Context* ctx, NDBT_Step* step)
{
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  NdbRestarter restarter;
  HugoOperations hugoOps(*ctx->getTab());
  Ndb* pNdb = GETNDB(step);
  
  int i = 0;
  while(i<loops && result != NDBT_FAILED && !ctx->isTestStopped()){
    g_info << i << ": ";

    if(hugoOps.startTransaction(pNdb) != 0)
      return NDBT_FAILED;
      
    if(hugoOps.pkUpdateRecord(pNdb, 1, 128) != 0)
      return NDBT_FAILED;

    if(hugoOps.execute_NoCommit(pNdb) != 0)
      return NDBT_FAILED;

    Uint32 transNode= hugoOps.getTransaction()->getConnectedNodeId();
    int id = i % restarter.getNumDbNodes();
    int nodeId;
    while((nodeId = restarter.getDbNodeId(id)) == (int)transNode)
      id = (id + 1) % restarter.getNumDbNodes();

    ndbout << "Restart node " << nodeId << endl; 
    
    restarter.restartOneDbNode(nodeId,
			     /** initial */ false, 
			     /** nostart */ true,
			     /** abort   */ true);
    
    restarter.waitNodesNoStart(&nodeId, 1);
    
    int res;
    if(i & 1)
      res= hugoOps.execute_Commit(pNdb);
    else
      res= hugoOps.execute_Rollback(pNdb);
    
    ndbout_c("res= %d", res);
    
    hugoOps.closeTransaction(pNdb);
    
    restarter.startNodes(&nodeId, 1);
    restarter.waitNodesStarted(&nodeId, 1);
    
    if(i & 1)
    {
      if(res != 286)
	return NDBT_FAILED;
    }
    else
    {
      if(res != 0)
	return NDBT_FAILED;
    }
    i++;
  }
  
  return NDBT_OK;
}

int runBug15587(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbRestarter restarter;
  
  Uint32 tableId = ctx->getTab()->getTableId();
  int dump[2] = { DumpStateOrd::LqhErrorInsert5042, 0 };
  dump[1] = tableId;

  int nodeId = restarter.getDbNodeId(1);

  ndbout << "Restart node " << nodeId << endl; 
  
  if (restarter.restartOneDbNode(nodeId,
				 /** initial */ false, 
				 /** nostart */ true,
				 /** abort   */ true))
    return NDBT_FAILED;
  
  if (restarter.waitNodesNoStart(&nodeId, 1))
    return NDBT_FAILED; 
   
  int val2[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };
  
  if (restarter.dumpStateOneNode(nodeId, val2, 2))
    return NDBT_FAILED;

  if (restarter.dumpStateOneNode(nodeId, dump, 2))
    return NDBT_FAILED;

  if (restarter.startNodes(&nodeId, 1))
    return NDBT_FAILED;

  restarter.waitNodesStartPhase(&nodeId, 1, 3);
  
  if (restarter.waitNodesNoStart(&nodeId, 1))
    return NDBT_FAILED; 
   
  if (restarter.dumpStateOneNode(nodeId, val2, 1))
    return NDBT_FAILED;
  
  if (restarter.startNodes(&nodeId, 1))
    return NDBT_FAILED;
  
  if (restarter.waitNodesStarted(&nodeId, 1))
    return NDBT_FAILED;
  
  ctx->stopTest();
  return NDBT_OK;
}

int runBug15632(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbRestarter restarter;
  
  int nodeId = restarter.getDbNodeId(1);

  ndbout << "Restart node " << nodeId << endl; 
  
  if (restarter.restartOneDbNode(nodeId,
				 /** initial */ false, 
				 /** nostart */ true,
				 /** abort   */ true))
    return NDBT_FAILED;
  
  if (restarter.waitNodesNoStart(&nodeId, 1))
    return NDBT_FAILED; 
   
  if (restarter.insertErrorInNode(nodeId, 7165))
    return NDBT_FAILED;
  
  if (restarter.startNodes(&nodeId, 1))
    return NDBT_FAILED;

  if (restarter.waitNodesStarted(&nodeId, 1))
    return NDBT_FAILED;

  if (restarter.restartOneDbNode(nodeId,
				 /** initial */ false, 
				 /** nostart */ true,
				 /** abort   */ true))
    return NDBT_FAILED;
  
  if (restarter.waitNodesNoStart(&nodeId, 1))
    return NDBT_FAILED; 
   
  if (restarter.insertErrorInNode(nodeId, 7171))
    return NDBT_FAILED;
  
  if (restarter.startNodes(&nodeId, 1))
    return NDBT_FAILED;
  
  if (restarter.waitNodesStarted(&nodeId, 1))
    return NDBT_FAILED;
  
  ctx->stopTest();
  return NDBT_OK;
}

int runBug15685(NDBT_Context* ctx, NDBT_Step* step){

  Ndb* pNdb = GETNDB(step);
  HugoOperations hugoOps(*ctx->getTab());
  NdbRestarter restarter;

  HugoTransactions hugoTrans(*ctx->getTab());
  if (hugoTrans.loadTable(GETNDB(step), 10) != 0){
    return NDBT_FAILED;
  }

  if(hugoOps.startTransaction(pNdb) != 0)
    goto err;
  
  if(hugoOps.pkUpdateRecord(pNdb, 0, 1, rand()) != 0)
    goto err;

  if(hugoOps.execute_NoCommit(pNdb) != 0)
    goto err;

  if (restarter.insertErrorInAllNodes(5100))
    return NDBT_FAILED;
  
  hugoOps.execute_Rollback(pNdb);

  if (restarter.waitClusterStarted() != 0)
    goto err;

  if (restarter.insertErrorInAllNodes(0))
    return NDBT_FAILED;
  
  ctx->stopTest();
  return NDBT_OK;
  
err:
  ctx->stopTest();
  return NDBT_FAILED;
}

int 
runBug16772(NDBT_Context* ctx, NDBT_Step* step){

  NdbRestarter restarter;
  if (restarter.getNumDbNodes() < 2)
  {
    ctx->stopTest();
    return NDBT_OK;
  }

  int aliveNodeId = restarter.getRandomNotMasterNodeId(rand());
  int deadNodeId = aliveNodeId;
  while (deadNodeId == aliveNodeId)
    deadNodeId = restarter.getDbNodeId(rand() % restarter.getNumDbNodes());
  
  if (restarter.insertErrorInNode(aliveNodeId, 930))
    return NDBT_FAILED;

  if (restarter.restartOneDbNode(deadNodeId,
				 /** initial */ false, 
				 /** nostart */ true,
				 /** abort   */ true))
    return NDBT_FAILED;
  
  if (restarter.waitNodesNoStart(&deadNodeId, 1))
    return NDBT_FAILED;

  if (restarter.startNodes(&deadNodeId, 1))
    return NDBT_FAILED;

  // It should now be hanging since we throw away NDB_FAILCONF
  int ret = restarter.waitNodesStartPhase(&deadNodeId, 1, 3, 10);
  // So this should fail...i.e it should not reach startphase 3

  // Now send a NDB_FAILCONF for deadNo
  int dump[] = { 7020, 323, 252, 0 };
  dump[3] = deadNodeId;
  if (restarter.dumpStateOneNode(aliveNodeId, dump, 4))
    return NDBT_FAILED;
  
  if (restarter.waitNodesStarted(&deadNodeId, 1))
    return NDBT_FAILED;

  return ret ? NDBT_OK : NDBT_FAILED;
}

int 
runBug18414(NDBT_Context* ctx, NDBT_Step* step){

  NdbRestarter restarter;
  if (restarter.getNumDbNodes() < 2)
  {
    ctx->stopTest();
    return NDBT_OK;
  }

  Ndb* pNdb = GETNDB(step);
  HugoOperations hugoOps(*ctx->getTab());
  HugoTransactions hugoTrans(*ctx->getTab());
  int loop = 0;
  do 
  {
    if(hugoOps.startTransaction(pNdb) != 0)
      goto err;
    
    if(hugoOps.pkUpdateRecord(pNdb, 0, 128, rand()) != 0)
      goto err;
    
    if(hugoOps.execute_NoCommit(pNdb) != 0)
      goto err;

    int node1 = hugoOps.getTransaction()->getConnectedNodeId();
    int node2 = restarter.getRandomNodeSameNodeGroup(node1, rand());
    
    if (node1 == -1 || node2 == -1)
      break;
    
    if (loop & 1)
    {
      if (restarter.insertErrorInNode(node1, 8080))
	goto err;
    }
    
    int val2[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };
    if (restarter.dumpStateOneNode(node2, val2, 2))
      goto err;
    
    if (restarter.insertErrorInNode(node2, 5003))
      goto err;
    
    /** int res= */ hugoOps.execute_Rollback(pNdb);
  
    if (restarter.waitNodesNoStart(&node2, 1) != 0)
      goto err;
    
    if (restarter.insertErrorInAllNodes(0))
      goto err;
    
    if (restarter.startNodes(&node2, 1) != 0)
      goto err;
    
    if (restarter.waitClusterStarted() != 0)
      goto err;
    
    if (hugoTrans.scanUpdateRecords(pNdb, 128) != 0)
      goto err;

    hugoOps.closeTransaction(pNdb);
    
  } while(++loop < 5);
  
  return NDBT_OK;
  
err:
  hugoOps.closeTransaction(pNdb);
  return NDBT_FAILED;    
}

int 
runBug18612(NDBT_Context* ctx, NDBT_Step* step){

  // Assume two replicas
  NdbRestarter restarter;
  if (restarter.getNumDbNodes() < 2)
  {
    ctx->stopTest();
    return NDBT_OK;
  }

  Uint32 cnt = restarter.getNumDbNodes();

  for(int loop = 0; loop < ctx->getNumLoops(); loop++)
  {
    int partition0[256];
    int partition1[256];
    memset(partition0, 0, sizeof(partition0));
    memset(partition1, 0, sizeof(partition1));
    Bitmask<4> nodesmask;
    
    Uint32 node1 = restarter.getDbNodeId(rand()%cnt);
    for (Uint32 i = 0; i<cnt/2; i++)
    {
      do { 
	int tmp = restarter.getRandomNodeOtherNodeGroup(node1, rand());
	if (tmp == -1)
	{
	  ctx->stopTest();
	  return NDBT_OK;
	}
	node1 = tmp;
      } while(nodesmask.get(node1));
      
      partition0[i] = node1;
      partition1[i] = restarter.getRandomNodeSameNodeGroup(node1, rand());
      
      ndbout_c("nodes %d %d", node1, partition1[i]);
      
      assert(!nodesmask.get(node1));
      assert(!nodesmask.get(partition1[i]));
      nodesmask.set(node1);
      nodesmask.set(partition1[i]);
    } 
    
    ndbout_c("done");

    int dump[255];
    dump[0] = DumpStateOrd::NdbcntrStopNodes;
    memcpy(dump + 1, partition0, sizeof(int)*cnt/2);
    
    Uint32 master = restarter.getMasterNodeId();
    
    if (restarter.dumpStateOneNode(master, dump, 1+cnt/2))
      return NDBT_FAILED;
    
    if (restarter.waitNodesNoStart(partition0, cnt/2))
      return NDBT_FAILED;

    int val2[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };
    
    if (restarter.dumpStateAllNodes(val2, 2))
      return NDBT_FAILED;
    
    if (restarter.insertErrorInAllNodes(932))
      return NDBT_FAILED;

    dump[0] = 9000;
    memcpy(dump + 1, partition0, sizeof(int)*cnt/2);    
    for (Uint32 i = 0; i<cnt/2; i++)
      if (restarter.dumpStateOneNode(partition1[i], dump, 1+cnt/2))
	return NDBT_FAILED;

    dump[0] = 9000;
    memcpy(dump + 1, partition1, sizeof(int)*cnt/2);    
    for (Uint32 i = 0; i<cnt/2; i++)
      if (restarter.dumpStateOneNode(partition0[i], dump, 1+cnt/2))
	return NDBT_FAILED;
    
    if (restarter.startNodes(partition0, cnt/2))
      return NDBT_FAILED;
    
    if (restarter.waitNodesStartPhase(partition0, cnt/2, 2))
      return NDBT_FAILED;
    
    dump[0] = 9001;
    for (Uint32 i = 0; i<cnt/2; i++)
      if (restarter.dumpStateAllNodes(dump, 2))
	return NDBT_FAILED;

    if (restarter.waitNodesNoStart(partition0, cnt/2))
      return NDBT_FAILED;
    
    for (Uint32 i = 0; i<cnt/2; i++)
      if (restarter.restartOneDbNode(partition0[i], true, true, true))
	return NDBT_FAILED;
    
    if (restarter.waitNodesNoStart(partition0, cnt/2))
      return NDBT_FAILED;
    
    if (restarter.startAll())
      return NDBT_FAILED;

    if (restarter.waitClusterStarted())
      return NDBT_FAILED;
  }
  return NDBT_OK;
}

int 
runBug18612SR(NDBT_Context* ctx, NDBT_Step* step){

  // Assume two replicas
  NdbRestarter restarter;
  if (restarter.getNumDbNodes() < 2)
  {
    ctx->stopTest();
    return NDBT_OK;
  }

  Uint32 cnt = restarter.getNumDbNodes();

  for(int loop = 0; loop < ctx->getNumLoops(); loop++)
  {
    int partition0[256];
    int partition1[256];
    memset(partition0, 0, sizeof(partition0));
    memset(partition1, 0, sizeof(partition1));
    Bitmask<4> nodesmask;
    
    Uint32 node1 = restarter.getDbNodeId(rand()%cnt);
    for (Uint32 i = 0; i<cnt/2; i++)
    {
      do { 
	int tmp = restarter.getRandomNodeOtherNodeGroup(node1, rand());
	if (tmp == -1)
	  break;
	node1 = tmp;
      } while(nodesmask.get(node1));
      
      partition0[i] = node1;
      partition1[i] = restarter.getRandomNodeSameNodeGroup(node1, rand());
      
      ndbout_c("nodes %d %d", node1, partition1[i]);
      
      assert(!nodesmask.get(node1));
      assert(!nodesmask.get(partition1[i]));
      nodesmask.set(node1);
      nodesmask.set(partition1[i]);
    } 
    
    ndbout_c("done");

    if (restarter.restartAll(false, true, false))
      return NDBT_FAILED;

    int dump[255];
    dump[0] = 9000;
    memcpy(dump + 1, partition0, sizeof(int)*cnt/2);    
    for (Uint32 i = 0; i<cnt/2; i++)
      if (restarter.dumpStateOneNode(partition1[i], dump, 1+cnt/2))
	return NDBT_FAILED;

    dump[0] = 9000;
    memcpy(dump + 1, partition1, sizeof(int)*cnt/2);    
    for (Uint32 i = 0; i<cnt/2; i++)
      if (restarter.dumpStateOneNode(partition0[i], dump, 1+cnt/2))
	return NDBT_FAILED;

    int val2[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };
    
    if (restarter.dumpStateAllNodes(val2, 2))
      return NDBT_FAILED;
    
    if (restarter.insertErrorInAllNodes(932))
      return NDBT_FAILED;
    
    if (restarter.startAll())
      return NDBT_FAILED;
    
    if (restarter.waitClusterStartPhase(2))
      return NDBT_FAILED;
    
    dump[0] = 9001;
    for (Uint32 i = 0; i<cnt/2; i++)
      if (restarter.dumpStateAllNodes(dump, 2))
	return NDBT_FAILED;

    if (restarter.waitClusterNoStart(30))
      if (restarter.waitNodesNoStart(partition0, cnt/2, 10))
	if (restarter.waitNodesNoStart(partition1, cnt/2, 10))
	  return NDBT_FAILED;
    
    if (restarter.startAll())
      return NDBT_FAILED;
    
    if (restarter.waitClusterStarted())
      return NDBT_FAILED;
  }
  return NDBT_OK;
}

int runBug20185(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbRestarter restarter;
  HugoOperations hugoOps(*ctx->getTab());
  Ndb* pNdb = GETNDB(step);
  
  const int masterNode = restarter.getMasterNodeId();

  int dump[] = { 7090, 20 } ;
  if (restarter.dumpStateAllNodes(dump, 2))
    return NDBT_FAILED;
  
  NdbSleep_MilliSleep(3000);
  Vector<int> nodes;
  for (int i = 0; i<restarter.getNumDbNodes(); i++)
    nodes.push_back(restarter.getDbNodeId(i));
  
retry:
  if(hugoOps.startTransaction(pNdb) != 0)
    return NDBT_FAILED;
  
  if(hugoOps.pkUpdateRecord(pNdb, 1, 1) != 0)
    return NDBT_FAILED;
  
  if (hugoOps.execute_NoCommit(pNdb) != 0)
    return NDBT_FAILED;
  
  const int node = hugoOps.getTransaction()->getConnectedNodeId();
  if (node != masterNode)
  {
    hugoOps.closeTransaction(pNdb);
    goto retry;
  } 
  
  int nodeId;
  do {
    nodeId = restarter.getDbNodeId(rand() % restarter.getNumDbNodes());
  } while (nodeId == node);
  
  ndbout_c("7031 to %d", nodeId);
  if (restarter.insertErrorInNode(nodeId, 7031))
    return NDBT_FAILED;

  for (Uint32 i = 0; i<nodes.size(); i++)
  {
    if (nodes[i] != nodeId)
      if (restarter.insertErrorInNode(nodes[i], 7030))
	return NDBT_FAILED;
  }
  
  NdbSleep_MilliSleep(500);
  
  if (hugoOps.execute_Commit(pNdb) == 0)
    return NDBT_FAILED;

  NdbSleep_MilliSleep(3000);

  restarter.waitClusterStarted();
  
  if (restarter.dumpStateAllNodes(dump, 1))
    return NDBT_FAILED;
  
  return NDBT_OK;
}

int runBug24717(NDBT_Context* ctx, NDBT_Step* step)
{
  int loops = ctx->getNumLoops();
  NdbRestarter restarter;
  Ndb* pNdb = GETNDB(step);
  
  HugoTransactions hugoTrans(*ctx->getTab());

  int dump[] = { 9002, 0 } ;
  Uint32 ownNode = refToNode(pNdb->getReference());
  dump[1] = ownNode;

  for (; loops; loops --)
  {
    int nodeId = restarter.getRandomNotMasterNodeId(rand());
    restarter.restartOneDbNode(nodeId, false, true, true);
    restarter.waitNodesNoStart(&nodeId, 1);
    
    if (restarter.dumpStateOneNode(nodeId, dump, 2))
      return NDBT_FAILED;
    
    restarter.startNodes(&nodeId, 1);
    
    do {
      for (Uint32 i = 0; i < 100; i++)
      {
        hugoTrans.pkReadRecords(pNdb, 100, 1, NdbOperation::LM_CommittedRead);
      }
    } while (restarter.waitClusterStarted(5) != 0);
  }
  
  return NDBT_OK;
}

int 
runBug29364(NDBT_Context* ctx, NDBT_Step* step)
{
  int loops = ctx->getNumLoops();
  NdbRestarter restarter;
  Ndb* pNdb = GETNDB(step);
  
  HugoTransactions hugoTrans(*ctx->getTab());

  if (restarter.getNumDbNodes() < 4)
    return NDBT_OK;

  int dump0[] = { 9000, 0 } ;
  int dump1[] = { 9001, 0 } ;
  Uint32 ownNode = refToNode(pNdb->getReference());
  dump0[1] = ownNode;

  for (; loops; loops --)
  {
    int node0 = restarter.getDbNodeId(rand() % restarter.getNumDbNodes());
    int node1 = restarter.getRandomNodeOtherNodeGroup(node0, rand());

    restarter.restartOneDbNode(node0, false, true, true);
    restarter.waitNodesNoStart(&node0, 1);
    restarter.startNodes(&node0, 1);
    restarter.waitClusterStarted();

    restarter.restartOneDbNode(node1, false, true, true);    
    restarter.waitNodesNoStart(&node1, 1);
    if (restarter.dumpStateOneNode(node1, dump0, 2))
      return NDBT_FAILED;

    restarter.startNodes(&node1, 1);    
    
    do {
      
      for (Uint32 i = 0; i < 100; i++)
      {
        hugoTrans.pkReadRecords(pNdb, 100, 1, NdbOperation::LM_CommittedRead);
      }
    } while (restarter.waitClusterStarted(5) != 0);
    
    if (restarter.dumpStateOneNode(node1, dump1, 1))
      return NDBT_FAILED;
  }
  
  return NDBT_OK;
}

int runBug25364(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbRestarter restarter;
  int loops = ctx->getNumLoops();
  
  if (restarter.getNumDbNodes() < 4)
    return NDBT_OK;

  int val2[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };

  for (; loops; loops --)
  {
    int master = restarter.getMasterNodeId();
    int victim = restarter.getRandomNodeOtherNodeGroup(master, rand());
    int second = restarter.getRandomNodeSameNodeGroup(victim, rand());
    
    int dump[] = { 935, victim } ;
    if (restarter.dumpStateOneNode(master, dump, 2))
      return NDBT_FAILED;
  
    if (restarter.dumpStateOneNode(master, val2, 2))
      return NDBT_FAILED;
  
    if (restarter.restartOneDbNode(second, false, true, true))
      return NDBT_FAILED;

    int nodes[2] = { master, second };
    if (restarter.waitNodesNoStart(nodes, 2))
      return NDBT_FAILED;

    restarter.startNodes(nodes, 2);

    if (restarter.waitNodesStarted(nodes, 2))
      return NDBT_FAILED;
  }

  return NDBT_OK;
}
  
int 
runBug21271(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbRestarter restarter;
  HugoOperations hugoOps(*ctx->getTab());
  
  const int masterNode = restarter.getMasterNodeId();
  const int nodeId = restarter.getRandomNodeSameNodeGroup(masterNode, rand());

  int val2[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };
  if (restarter.dumpStateOneNode(nodeId, val2, 2))
    return NDBT_FAILED;
  
  Uint32 tableId = ctx->getTab()->getTableId();
  int dump[] = { DumpStateOrd::LqhErrorInsert5042, 0, 5044 };
  dump[1] = tableId;

  if (restarter.dumpStateOneNode(nodeId, dump, 3))
    return NDBT_FAILED;
  
  restarter.waitNodesNoStart(&nodeId, 1);
  ctx->stopTest();

  restarter.startNodes(&nodeId, 1);

  if (restarter.waitClusterStarted() != 0)
    return NDBT_FAILED;

  return NDBT_OK;
  return NDBT_OK;
}

int 
runBug24543(NDBT_Context* ctx, NDBT_Step* step){
  NdbRestarter restarter;
  
  int val2[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };
  if (restarter.dumpStateAllNodes(val2, 2))
    return NDBT_FAILED;

  int nodes[2];
  nodes[0] = restarter.getMasterNodeId();
  restarter.insertErrorInNode(nodes[0], 934);

  nodes[1] = restarter.getRandomNodeOtherNodeGroup(nodes[0], rand());
  if (nodes[1] == -1)
  {
    nodes[1] = restarter.getRandomNodeSameNodeGroup(nodes[0], rand());
  }
  
  restarter.restartOneDbNode(nodes[1], false, true, true);
  if (restarter.waitNodesNoStart(nodes, 2))
    return NDBT_FAILED;
  
  restarter.startNodes(nodes, 2);
  if (restarter.waitNodesStarted(nodes, 2))
  {
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int runBug25468(NDBT_Context* ctx, NDBT_Step* step)
{
  int loops = ctx->getNumLoops();
  NdbRestarter restarter;
  
  for (int i = 0; i<loops; i++)
  {
    int master = restarter.getMasterNodeId();
    int node1, node2;
    switch(i % 5){
    case 0:
      node1 = master;
      node2 = restarter.getRandomNodeSameNodeGroup(master, rand());
      break;
    case 1:
      node1 = restarter.getRandomNodeSameNodeGroup(master, rand());
      node2 = master;
      break;
    case 2:
    case 3:
    case 4:
      node1 = restarter.getRandomNodeOtherNodeGroup(master, rand());
      if (node1 == -1)
	node1 = master;
      node2 = restarter.getRandomNodeSameNodeGroup(node1, rand());
      break;
    }

    ndbout_c("node1: %d node2: %d master: %d", node1, node2, master);

    int val2[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };
  
    if (restarter.dumpStateOneNode(node2, val2, 2))
      return NDBT_FAILED;

    if (restarter.insertErrorInNode(node1, 7178))
      return NDBT_FAILED;

    int val1 = 7099;
    if (restarter.dumpStateOneNode(master, &val1, 1))
      return NDBT_FAILED;

    if (restarter.waitNodesNoStart(&node2, 1))
      return NDBT_FAILED;

    if (restarter.startAll())
      return NDBT_FAILED;

    if (restarter.waitClusterStarted())
      return NDBT_FAILED;
  }

  return NDBT_OK;
}

int runBug25554(NDBT_Context* ctx, NDBT_Step* step)
{
  int loops = ctx->getNumLoops();
  NdbRestarter restarter;
  
  if (restarter.getNumDbNodes() < 4)
    return NDBT_OK;

  for (int i = 0; i<loops; i++)
  {
    int master = restarter.getMasterNodeId();
    int node1 = restarter.getRandomNodeOtherNodeGroup(master, rand());
    restarter.restartOneDbNode(node1, false, true, true);

    int val2[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };
  
    if (restarter.dumpStateOneNode(master, val2, 2))
      return NDBT_FAILED;

    if (restarter.insertErrorInNode(master, 7141))
      return NDBT_FAILED;

    if (restarter.waitNodesNoStart(&node1, 1))
      return NDBT_FAILED;

    if (restarter.dumpStateOneNode(node1, val2, 2))
      return NDBT_FAILED;

    if (restarter.insertErrorInNode(node1, 932))
      return NDBT_FAILED;

    if (restarter.startNodes(&node1, 1))
      return NDBT_FAILED;

    int nodes[] = { master, node1 };
    if (restarter.waitNodesNoStart(nodes, 2))
      return NDBT_FAILED;

    if (restarter.startNodes(nodes, 2))
      return NDBT_FAILED;

    if (restarter.waitClusterStarted())
      return NDBT_FAILED;
  }    

  return NDBT_OK;
}

int runBug25984(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbRestarter restarter;
  Ndb* pNdb = GETNDB(step);

  NdbDictionary::Table tab = * ctx->getTab();
  NdbDictionary::Dictionary* pDict = GETNDB(step)->getDictionary();

  if (restarter.getNumDbNodes() < 2)
    return NDBT_OK;

  pDict->dropTable(tab.getName());

  if (restarter.restartAll(true, true, true))
    return NDBT_FAILED;

  if (restarter.waitClusterNoStart())
    return NDBT_FAILED;

  if (restarter.startAll())
    return NDBT_FAILED;

  if (restarter.waitClusterStarted())
    return NDBT_FAILED;

  int res = pDict->createTable(tab);
  if (res)
  {
    return NDBT_FAILED;
  }
  HugoTransactions trans(* pDict->getTable(tab.getName()));
  trans.loadTable(pNdb, ctx->getNumRecords());
                         
  int val2[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };    
  int master = restarter.getMasterNodeId();
  int victim = restarter.getRandomNodeOtherNodeGroup(master, rand());
  if (victim == -1)
    victim = restarter.getRandomNodeSameNodeGroup(master, rand());

  restarter.restartOneDbNode(victim, false, true, true);

  for (Uint32 i = 0; i<10; i++)
  {
    ndbout_c("Loop: %d", i);
    if (restarter.waitNodesNoStart(&victim, 1))
      return NDBT_FAILED;
    
    if (restarter.dumpStateOneNode(victim, val2, 2))
      return NDBT_FAILED;
    
    if (restarter.insertErrorInNode(victim, 7191))
      return NDBT_FAILED;

    trans.scanUpdateRecords(pNdb, ctx->getNumRecords());
    
    if (restarter.startNodes(&victim, 1))
      return NDBT_FAILED;

    NdbSleep_SecSleep(3);
  }

  if (restarter.waitNodesNoStart(&victim, 1))
    return NDBT_FAILED;
  
  if (restarter.restartAll(false, false, true))
    return NDBT_FAILED;

  if (restarter.waitClusterStarted())
    return NDBT_FAILED;

  trans.scanUpdateRecords(pNdb, ctx->getNumRecords());

  restarter.restartOneDbNode(victim, false, true, true);
  for (Uint32 i = 0; i<1; i++)
  {
    ndbout_c("Loop: %d", i);
    if (restarter.waitNodesNoStart(&victim, 1))
      return NDBT_FAILED;
    
    if (restarter.dumpStateOneNode(victim, val2, 2))
      return NDBT_FAILED;
    
    if (restarter.insertErrorInNode(victim, 7016))
      return NDBT_FAILED;
  
    trans.scanUpdateRecords(pNdb, ctx->getNumRecords());
  
    if (restarter.startNodes(&victim, 1))
      return NDBT_FAILED;

    NdbSleep_SecSleep(3);
  }

  if (restarter.waitNodesNoStart(&victim, 1))
    return NDBT_FAILED;
  
  if (restarter.startNodes(&victim, 1))
    return NDBT_FAILED;
  
  if (restarter.waitClusterStarted())
    return NDBT_FAILED;

  return NDBT_OK;
}

int
runBug26457(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbRestarter res;
  if (res.getNumDbNodes() < 4)
    return NDBT_OK;

  int loops = ctx->getNumLoops();
  while (loops --)
  {
retry:
    int master = res.getMasterNodeId();
    int next = res.getNextMasterNodeId(master);

    ndbout_c("master: %d next: %d", master, next);

    if (res.getNodeGroup(master) == res.getNodeGroup(next))
    {
      res.restartOneDbNode(next, false, false, true);
      if (res.waitClusterStarted())
	return NDBT_FAILED;
      goto retry;
    }

    int val2[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 2 };
    
    if (res.dumpStateOneNode(next, val2, 2))
      return NDBT_FAILED;
    
    if (res.insertErrorInNode(next, 7180))
      return NDBT_FAILED;
    
    res.restartOneDbNode(master, false, false, true);
    if (res.waitClusterStarted())
      return NDBT_FAILED;
  }

  return NDBT_OK;
}

int 
runBug26481(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbRestarter res;
  
  int node = res.getRandomNotMasterNodeId(rand());
  ndbout_c("node: %d", node);
  if (res.restartOneDbNode(node, true, true, true))
    return NDBT_FAILED;

  if (res.waitNodesNoStart(&node, 1))
    return NDBT_FAILED;

  int val2[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };
  if (res.dumpStateOneNode(node, val2, 2))
    return NDBT_FAILED;

  if (res.insertErrorInNode(node, 7018))
    return NDBT_FAILED;

  if (res.startNodes(&node, 1))
    return NDBT_FAILED;

  res.waitNodesStartPhase(&node, 1, 3);
  
  if (res.waitNodesNoStart(&node, 1))
    return NDBT_FAILED;

  res.startNodes(&node, 1);
  
  if (res.waitClusterStarted())
    return NDBT_FAILED;
  
  return NDBT_OK;
}

int 
runBug26450(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbRestarter res;
  Ndb* pNdb = GETNDB(step);
  
  int node = res.getRandomNotMasterNodeId(rand());
  Vector<int> nodes;
  for (int i = 0; i<res.getNumDbNodes(); i++)
  {
    if (res.getDbNodeId(i) != node)
      nodes.push_back(res.getDbNodeId(i));
  }
  
  if (res.restartAll())
    return NDBT_FAILED;

  if (res.waitClusterStarted())
    return NDBT_FAILED;

  ndbout_c("node: %d", node);
  if (res.restartOneDbNode(node, false, true, true))
    return NDBT_FAILED;
  
  if (res.waitNodesNoStart(&node, 1))
    return NDBT_FAILED;

  if (runClearTable(ctx, step))
    return NDBT_FAILED;

  for (int i = 0; i < 2; i++)
  {
    if (res.restartAll(false, true, i > 0))
      return NDBT_FAILED;
    
    if (res.waitClusterNoStart())
      return NDBT_FAILED;
    
    if (res.startNodes(nodes.getBase(), nodes.size()))
      return NDBT_FAILED;

    if (res.waitNodesStarted(nodes.getBase(), nodes.size()))
      return NDBT_FAILED;
  }

  if (res.startNodes(&node, 1))
    return NDBT_FAILED;

  if (res.waitNodesStarted(&node, 1))
    return NDBT_FAILED;

  HugoTransactions trans (* ctx->getTab());
  if (trans.selectCount(pNdb) != 0)
    return NDBT_FAILED;

  return NDBT_OK;
}

int
runBug27003(NDBT_Context* ctx, NDBT_Step* step)
{
  int loops = ctx->getNumLoops();
  NdbRestarter res;
  
  static const int errnos[] = { 4025, 4026, 4027, 4028, 0 };

  int node = res.getRandomNotMasterNodeId(rand());
  ndbout_c("node: %d", node);
  if (res.restartOneDbNode(node, true, true, true))
    return NDBT_FAILED;

  Uint32 pos = 0;
  for (int i = 0; i<loops; i++)
  {
    while (errnos[pos] != 0)
    {
      ndbout_c("Testing err: %d", errnos[pos]);
      
      if (res.waitNodesNoStart(&node, 1))
	return NDBT_FAILED;

      if (res.insertErrorInNode(node, 1000))
	return NDBT_FAILED;
      
      if (res.insertErrorInNode(node, errnos[pos]))
	return NDBT_FAILED;
      
      int val2[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 3 };
      if (res.dumpStateOneNode(node, val2, 2))
	return NDBT_FAILED;
      
      res.startNodes(&node, 1);
      NdbSleep_SecSleep(3);
      pos++;
    }
    pos = 0;
  }

  if (res.waitNodesNoStart(&node, 1))
    return NDBT_FAILED;
  
  res.startNodes(&node, 1);
  if (res.waitClusterStarted())
    return NDBT_FAILED;
  
  return NDBT_OK;
}


int
runBug27283(NDBT_Context* ctx, NDBT_Step* step)
{
  int loops = ctx->getNumLoops();
  NdbRestarter res;

  if (res.getNumDbNodes() < 2)
  {
    return NDBT_OK;
  }

  static const int errnos[] = { 7181, 7182, 0 };
  
  Uint32 pos = 0;
  for (Uint32 i = 0; i<(Uint32)loops; i++)
  {
    while (errnos[pos] != 0)
    {
      int master = res.getMasterNodeId();
      int next = res.getNextMasterNodeId(master);
      //int next2 = res.getNextMasterNodeId(next);
      
      //int node = (i & 1) ? next : next2;
      ndbout_c("Testing err: %d", errnos[pos]);
      if (res.insertErrorInNode(next, errnos[pos]))
	return NDBT_FAILED;

      NdbSleep_SecSleep(3);
      
      if (res.waitClusterStarted())
	return NDBT_FAILED;
      
      pos++;
    }
    pos = 0;
  }
  
  return NDBT_OK;
}

int
runBug27466(NDBT_Context* ctx, NDBT_Step* step)
{
  int loops = ctx->getNumLoops();
  NdbRestarter res;

  if (res.getNumDbNodes() < 2)
  {
    return NDBT_OK;
  }

  for (Uint32 i = 0; i<(Uint32)loops; i++)
  {
    int node1 = res.getDbNodeId(rand() % res.getNumDbNodes());
    int node2 = node1;
    while (node1 == node2)
    {
      node2 = res.getDbNodeId(rand() % res.getNumDbNodes());
    }

    ndbout_c("nodes %u %u", node1, node2);

    if (res.restartOneDbNode(node1, false, true, true))
      return NDBT_FAILED;
    
    if (res.waitNodesNoStart(&node1, 1))
      return NDBT_FAILED;
    
    int val2[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };
    if (res.dumpStateOneNode(node1, val2, 2))
      return NDBT_FAILED;
    
    if (res.insertErrorInNode(node2, 8039))
      return NDBT_FAILED;

    res.startNodes(&node1, 1);
    NdbSleep_SecSleep(3);
    if (res.waitNodesNoStart(&node1, 1))
      return NDBT_FAILED;
    NdbSleep_SecSleep(5); // Wait for delayed INCL_NODECONF to arrive
    
    res.startNodes(&node1, 1);
    if (res.waitClusterStarted())
      return NDBT_FAILED;
  }
  
  return NDBT_OK;
}

int
runBug28023(NDBT_Context* ctx, NDBT_Step* step)
{
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  Ndb* pNdb = GETNDB(step);
  NdbRestarter res;

  if (res.getNumDbNodes() < 2)
  {
    return NDBT_OK;
  }


  HugoTransactions hugoTrans(*ctx->getTab());
  if (hugoTrans.loadTable(pNdb, records) != 0){
    return NDBT_FAILED;
  }
  
  if (hugoTrans.clearTable(pNdb, records) != 0)
  {
    return NDBT_FAILED;
  }

  for (Uint32 i = 0; i<(Uint32)loops; i++)
  {
    int node1 = res.getDbNodeId(rand() % res.getNumDbNodes());
    
    if (res.restartOneDbNode2(node1, 
                              NdbRestarter::NRRF_ABORT |
                              NdbRestarter::NRRF_NOSTART))
      return NDBT_FAILED;
    
    if (res.waitNodesNoStart(&node1, 1))
      return NDBT_FAILED;

    if (hugoTrans.loadTable(pNdb, records) != 0){
      return NDBT_FAILED;
    }
    
    if (hugoTrans.clearTable(pNdb, records) != 0)
    {
      return NDBT_FAILED;
    }
    
    res.startNodes(&node1, 1);
    if (res.waitClusterStarted())
      return NDBT_FAILED;

    if (hugoTrans.loadTable(pNdb, records) != 0){
      return NDBT_FAILED;
    }
    
    if (hugoTrans.scanUpdateRecords(pNdb, records) != 0)
      return NDBT_FAILED;

    if (hugoTrans.clearTable(pNdb, records) != 0)
    {
      return NDBT_FAILED;
    }
  }

  return NDBT_OK;
}


int
runBug28717(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbRestarter res;

  if (res.getNumDbNodes() < 4)
  {
    return NDBT_OK;
  }

  int master = res.getMasterNodeId();
  int node0 = res.getRandomNodeOtherNodeGroup(master, rand());
  int node1 = res.getRandomNodeSameNodeGroup(node0, rand());
  
  ndbout_c("master: %d node0: %d node1: %d", master, node0, node1);
  
  if (res.restartOneDbNode(node0, false, true, true))
  {
    return NDBT_FAILED;
  }

  {
    int filter[] = { 15, NDB_MGM_EVENT_CATEGORY_CHECKPOINT, 0 };
    NdbLogEventHandle handle = 
      ndb_mgm_create_logevent_handle(res.handle, filter);
    

    int dump[] = { DumpStateOrd::DihStartLcpImmediately };
    struct ndb_logevent event;
    
    for (Uint32 i = 0; i<3; i++)
    {
      res.dumpStateOneNode(master, dump, 1);
      while(ndb_logevent_get_next(handle, &event, 0) >= 0 &&
            event.type != NDB_LE_LocalCheckpointStarted);
      while(ndb_logevent_get_next(handle, &event, 0) >= 0 &&
            event.type != NDB_LE_LocalCheckpointCompleted);
    } 
  }
  
  if (res.waitNodesNoStart(&node0, 1))
    return NDBT_FAILED;
  
  int val2[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };
  
  if (res.dumpStateOneNode(node0, val2, 2))
    return NDBT_FAILED;
  
  if (res.insertErrorInNode(node0, 5010))
    return NDBT_FAILED;
  
  if (res.insertErrorInNode(node1, 1001))
    return NDBT_FAILED;
  
  if (res.startNodes(&node0, 1))
    return NDBT_FAILED;
  
  NdbSleep_SecSleep(3);

  if (res.insertErrorInNode(node1, 0))
    return NDBT_FAILED;

  if (res.waitNodesNoStart(&node0, 1))
    return NDBT_FAILED;

  if (res.startNodes(&node0, 1))
    return NDBT_FAILED;

  if (res.waitClusterStarted())
    return NDBT_FAILED;
  
  return NDBT_OK;
}

static
int
f_master_failure [] = {
  7000, 7001, 7002, 7003, 7004, 7186, 7187, 7188, 7189, 7190, 0
};

static
int
f_participant_failure [] = {
  7005, 7006, 7007, 7008, 5000, 7228, 0
};

int
runerrors(NdbRestarter& res, NdbRestarter::NodeSelector sel, const int* errors)
{
  for (Uint32 i = 0; errors[i]; i++)
  {
    int node = res.getNode(sel);

    int val2[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };
    if (res.dumpStateOneNode(node, val2, 2))
      return NDBT_FAILED;

    ndbout << "node " << node << " err: " << errors[i]<< endl;
    if (res.insertErrorInNode(node, errors[i]))
      return NDBT_FAILED;

    if (res.waitNodesNoStart(&node, 1) != 0)
      return NDBT_FAILED;

    res.startNodes(&node, 1);

    if (res.waitClusterStarted() != 0)
      return NDBT_FAILED;
  }
  return NDBT_OK;
}

int
runGCP(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbRestarter res;
  int loops = ctx->getNumLoops();

  if (res.getNumDbNodes() < 2)
  {
    return NDBT_OK;
  }

  if (res.getNumDbNodes() < 4)
  {
    /**
     * 7186++ is only usable for 4 nodes and above
     */
    Uint32 i;
    for (i = 0; f_master_failure[i] && f_master_failure[i] != 7186; i++);
    f_master_failure[i] = 0;
  }

  while (loops >= 0 && !ctx->isTestStopped())
  {
    loops --;

#if 0
    if (runerrors(res, NdbRestarter::NS_NON_MASTER, f_participant_failure))
    {
      return NDBT_FAILED;
    }

    if (runerrors(res, NdbRestarter::NS_MASTER, f_participant_failure))
    {
      return NDBT_FAILED;
    }
#endif

    if (runerrors(res, NdbRestarter::NS_RANDOM, f_participant_failure))
    {
      return NDBT_FAILED;
    }

    if (runerrors(res, NdbRestarter::NS_MASTER, f_master_failure))
    {
      return NDBT_FAILED;
    }
  }
  ctx->stopTest();
  return NDBT_OK;
}

int 
runCommitAck(NDBT_Context* ctx, NDBT_Step* step)
{
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  NdbRestarter restarter;
  Ndb* pNdb = GETNDB(step);

  if (records < 2)
    return NDBT_OK;
  if (restarter.getNumDbNodes() < 2)
    return NDBT_OK;

  int trans_type= -1;
  NdbConnection *pCon;
  int node;
  while (loops--)
  {
    trans_type++;
    if (trans_type > 2)
      trans_type= 0;
    HugoTransactions hugoTrans(*ctx->getTab());
    switch (trans_type) {
    case 0:
      /*
        - load records less 1
      */
      g_info << "case 0\n";
      if (hugoTrans.loadTable(GETNDB(step), records - 1))
      {
        return NDBT_FAILED;
      }
      break;
    case 1:
      /*
        - load 1 record
      */
      g_info << "case 1\n";
      if (hugoTrans.loadTable(GETNDB(step), 1))
      {
        return NDBT_FAILED;
      }
      break;
    case 2:
      /*
        - load 1 record in the end
      */
      g_info << "case 2\n";
      {
        HugoOperations hugoOps(*ctx->getTab());
        if (hugoOps.startTransaction(pNdb))
          abort();
        if (hugoOps.pkInsertRecord(pNdb, records-1))
          abort();
        if (hugoOps.execute_Commit(pNdb))
          abort();
        if (hugoOps.closeTransaction(pNdb))
          abort();
      }
      break;
    default:
      abort();
    }

    /* run transaction that should be tested */
    HugoOperations hugoOps(*ctx->getTab());
    if (hugoOps.startTransaction(pNdb))
      return NDBT_FAILED;
    pCon= hugoOps.getTransaction();
    node= pCon->getConnectedNodeId();
    switch (trans_type) {
    case 0:
    case 1:
      /*
        insert records with ignore error
        - insert rows, some exist already
      */
      for (int i= 0; i < records; i++)
      {
        if (hugoOps.pkInsertRecord(pNdb, i))
          goto err;
      }
      break;
    case 2:
      /*
        insert records with ignore error
        - insert rows, some exist already
      */
      for (int i= 0; i < records; i++)
      {
        if (hugoOps.pkInsertRecord(pNdb, i))
          goto err;
      }
      break;
    default:
      abort();
    }

    /*
      insert error in ndb kernel (TC) that throws away acknowledge of commit
      and then die 5 seconds later
    */
    {
      if (restarter.insertErrorInNode(node, 8054))
        goto err;
    }
    {
      int val2[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };
      if (restarter.dumpStateOneNode(node, val2, 2))
        goto err;
    }

    /* execute transaction and verify return code */
    g_info << "  execute... hangs for 5 seconds\n";
    {
      const NdbOperation *first= pCon->getFirstDefinedOperation();
      int check= pCon->execute(Commit, AO_IgnoreError);
      const NdbError err = pCon->getNdbError();

      while (first)
      {
        const NdbError &err= first->getNdbError();
        g_info << "         error " << err.code << endl;
        first= pCon->getNextCompletedOperation(first);
      }

      int expected_commit_res[3]= { 630, 630, 630 };
      if (check == -1 ||
          err.code != expected_commit_res[trans_type])
      {
        g_err << "check == " << check << endl;
        g_err << "got error: "
              << err.code
              << " expected: "
              << expected_commit_res[trans_type]
              << endl;
        goto err;
      }
    }

    g_info << "  wait node nostart\n";
    if (restarter.waitNodesNoStart(&node, 1))
    {
      g_err << "  wait node nostart failed\n";
      goto err;
    }

    /* close transaction */
    if (hugoOps.closeTransaction(pNdb))
      return NDBT_FAILED;

    /* commit ack marker pools should be empty */
    g_info << "  dump pool status\n";
    {
      int dump[255];
      dump[0] = 2552;
      if (restarter.dumpStateAllNodes(dump, 1))
        return NDBT_FAILED;
    }

    /* wait for cluster to come up again */
    g_info << "  wait cluster started\n";
    if (restarter.startNodes(&node, 1) ||
        restarter.waitNodesStarted(&node, 1))
    {
      g_err << "Cluster failed to start\n";
      return NDBT_FAILED;
    }

    /* verify data */
    g_info << "  verifying\n";
    switch (trans_type) {
    case 0:
    case 1:
    case 2:
      /*
        insert records with ignore error
        - should have all records
      */
      if (hugoTrans.scanReadRecords(GETNDB(step), records, 0, 64) != 0){
        return NDBT_FAILED;
      }
      break;
    default:
      abort();
    }

    /* cleanup for next round in loop */
    g_info << "  cleaning\n";
    if (hugoTrans.clearTable(GETNDB(step), records))
    {
      return NDBT_FAILED;
    }
    continue;
err:
    hugoOps.closeTransaction(pNdb);
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

int
max_cnt(int arr[], int cnt)
{
  int res = 0;

  for (int i = 0; i<cnt ; i++)
  {
    if (arr[i] > res)
    {
      res = arr[i];
    }
  }
  return res;
}

int
runPnr(NDBT_Context* ctx, NDBT_Step* step)
{
  int loops = ctx->getNumLoops();
  NdbRestarter res;
  bool lcp = ctx->getProperty("LCP", (unsigned)0);
  
  int nodegroups[MAX_NDB_NODES];
  bzero(nodegroups, sizeof(nodegroups));
  
  for (int i = 0; i<res.getNumDbNodes(); i++)
  {
    int node = res.getDbNodeId(i);
    nodegroups[res.getNodeGroup(node)]++;
  }
  
  for (int i = 0; i<MAX_NDB_NODES; i++)
  {
    if (nodegroups[i] && nodegroups[i] == 1)
    {
      /**
       * nodegroup with only 1 member, can't run test
       */
      ctx->stopTest();
      return NDBT_OK;
    }
  }

  for (int i = 0; i<loops && ctx->isTestStopped() == false; i++)
  {
    if (lcp)
    {
      int lcpdump = DumpStateOrd::DihMinTimeBetweenLCP;
      res.dumpStateAllNodes(&lcpdump, 1);
    }

    int ng_copy[MAX_NDB_NODES];
    memcpy(ng_copy, nodegroups, sizeof(ng_copy));
    
    Vector<int> nodes;
    printf("restarting ");
    while (max_cnt(ng_copy, MAX_NDB_NODES) > 1)
    {
      int node = res.getNode(NdbRestarter::NS_RANDOM);
      int ng = res.getNodeGroup(node);
      if (ng_copy[ng] > 1)
      {
        printf("%u ", node);
        nodes.push_back(node);
        ng_copy[ng]--;
      }
    }
    printf("\n");
    
    int val2[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };
    for (Uint32 j = 0; j<nodes.size(); j++)
    {
      res.dumpStateOneNode(nodes[j], val2, 2);
    }
    
    int kill[] = { 9999, 1000, 3000 };
    for (Uint32 j = 0; j<nodes.size(); j++)
    {
      res.dumpStateOneNode(nodes[j], kill, 3);
    }
    
    if (res.waitNodesNoStart(nodes.getBase(), nodes.size()))
      return NDBT_FAILED;
    
    if (res.startNodes(nodes.getBase(), nodes.size()))
      return NDBT_FAILED;

    if (res.waitClusterStarted())
      return NDBT_FAILED;
  }
  
  ctx->stopTest();
  return NDBT_OK;
}

int
runCreateBigTable(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbDictionary::Table tab = *ctx->getTab();
  BaseString tmp;
  tmp.assfmt("_%s", tab.getName());
  tab.setName(tmp.c_str());
  
  NdbDictionary::Dictionary* pDict = GETNDB(step)->getDictionary();
  int res = pDict->createTable(tab);
  if (res)
  {
    return NDBT_FAILED;
  }

  const NdbDictionary::Table* pTab = pDict->getTable(tmp.c_str());
  if (pTab == 0)
  {
    return NDBT_FAILED;
  }

  int bytes = tab.getRowSizeInBytes();
  int size = 50*1024*1024; // 50Mb
  int rows = size / bytes;

  if (rows > 1000000)
    rows = 1000000;

  ndbout_c("Loading %u rows into %s", rows, tmp.c_str());
  Uint64 now = NdbTick_CurrentMillisecond();
  HugoTransactions hugoTrans(*pTab);
  int cnt = 0;
  do {
    hugoTrans.loadTableStartFrom(GETNDB(step), cnt, 10000);
    cnt += 10000;
  } while (cnt < rows && (NdbTick_CurrentMillisecond() - now) < 30000); //30s
  ndbout_c("Loaded %u rows in %llums", cnt, 
           NdbTick_CurrentMillisecond() - now);

  return NDBT_OK;
}

int
runDropBigTable(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbDictionary::Table tab = *ctx->getTab();
  BaseString tmp;
  tmp.assfmt("_%s", tab.getName());
  GETNDB(step)->getDictionary()->dropTable(tmp.c_str());
  return NDBT_OK;
}

int
runBug31525(NDBT_Context* ctx, NDBT_Step* step)
{
  //int result = NDBT_OK;
  //int loops = ctx->getNumLoops();
  //int records = ctx->getNumRecords();
  //Ndb* pNdb = GETNDB(step);
  NdbRestarter res;

  if (res.getNumDbNodes() < 2)
  {
    return NDBT_OK;
  }

  int nodes[2];
  nodes[0] = res.getMasterNodeId();
  nodes[1] = res.getNextMasterNodeId(nodes[0]);
  
  while (res.getNodeGroup(nodes[0]) != res.getNodeGroup(nodes[1]))
  {
    ndbout_c("Restarting %u as it not in same node group as %u",
             nodes[1], nodes[0]);
    if (res.restartOneDbNode(nodes[1], false, true, true))
      return NDBT_FAILED;
    
    if (res.waitNodesNoStart(nodes+1, 1))
      return NDBT_FAILED;
    
    if (res.startNodes(nodes+1, 1))
      return NDBT_FAILED;
    
    if (res.waitClusterStarted())
      return NDBT_FAILED;

    nodes[1] = res.getNextMasterNodeId(nodes[0]);
  }
  
  ndbout_c("nodes[0]: %u nodes[1]: %u", nodes[0], nodes[1]);
  
  int val = DumpStateOrd::DihMinTimeBetweenLCP;
  if (res.dumpStateAllNodes(&val, 1))
    return NDBT_FAILED;

  int val2[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };  
  if (res.dumpStateAllNodes(val2, 2))
    return NDBT_FAILED;
  
  if (res.insertErrorInAllNodes(932))
    return NDBT_FAILED;

  if (res.insertErrorInNode(nodes[1], 7192))
    return NDBT_FAILED;
  
  if (res.insertErrorInNode(nodes[0], 7191))
    return NDBT_FAILED;
  
  if (res.waitClusterNoStart())
    return NDBT_FAILED;

  if (res.startAll())
    return NDBT_FAILED;
  
  if (res.waitClusterStarted())
    return NDBT_FAILED;

  if (res.restartOneDbNode(nodes[1], false, false, true))
    return NDBT_FAILED;

  if (res.waitClusterStarted())
    return NDBT_FAILED;
  
  return NDBT_OK;
}

int
runBug31980(NDBT_Context* ctx, NDBT_Step* step)
{
  //int result = NDBT_OK;
  //int loops = ctx->getNumLoops();
  //int records = ctx->getNumRecords();
  Ndb* pNdb = GETNDB(step);
  NdbRestarter res;

  if (res.getNumDbNodes() < 2)
  {
    return NDBT_OK;
  }


  HugoOperations hugoOps (* ctx->getTab());
  if(hugoOps.startTransaction(pNdb) != 0)
    return NDBT_FAILED;
  
  if(hugoOps.pkInsertRecord(pNdb, 1) != 0)
    return NDBT_FAILED;
  
  if(hugoOps.execute_NoCommit(pNdb) != 0)
    return NDBT_FAILED;
  
  int transNode= hugoOps.getTransaction()->getConnectedNodeId();
  int val2[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };    

  if (res.dumpStateOneNode(transNode, val2, 2))
  {
    return NDBT_FAILED;
  }

  if (res.insertErrorInNode(transNode, 8055))
  {
    return NDBT_FAILED;
  }
    
  hugoOps.execute_Commit(pNdb); // This should hang/fail

  if (res.waitNodesNoStart(&transNode, 1))
    return NDBT_FAILED;

  if (res.startNodes(&transNode, 1))
    return NDBT_FAILED;

  if (res.waitClusterStarted())
    return NDBT_FAILED;

  return NDBT_OK;
}

int
runBug32160(NDBT_Context* ctx, NDBT_Step* step)
{
  //int result = NDBT_OK;
  //int loops = ctx->getNumLoops();
  //int records = ctx->getNumRecords();
  //Ndb* pNdb = GETNDB(step);
  NdbRestarter res;

  if (res.getNumDbNodes() < 2)
  {
    return NDBT_OK;
  }

  int master = res.getMasterNodeId();
  int next = res.getNextMasterNodeId(master);

  if (res.insertErrorInNode(next, 7194))
  {
    return NDBT_FAILED;
  }

  int val2[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };    
  if (res.dumpStateOneNode(master, val2, 2))
    return NDBT_FAILED;

  if (res.insertErrorInNode(master, 7193))
    return NDBT_FAILED;

  int val3[] = { 7099 };
  if (res.dumpStateOneNode(master, val3, 1))
    return NDBT_FAILED;

  if (res.waitNodesNoStart(&master, 1))
    return NDBT_FAILED;

  if (res.startNodes(&master, 1))
    return NDBT_FAILED;

  if (res.waitClusterStarted())
    return NDBT_FAILED;
  
  return NDBT_OK;
}

int
runBug32922(NDBT_Context* ctx, NDBT_Step* step)
{
  //int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  //int records = ctx->getNumRecords();
  //Ndb* pNdb = GETNDB(step);
  NdbRestarter res;

  if (res.getNumDbNodes() < 2)
  {
    return NDBT_OK;
  }

  while (loops--)
  {
    int master = res.getMasterNodeId();    

    int victim = 32768;
    for (Uint32 i = 0; i<(Uint32)res.getNumDbNodes(); i++)
    {
      int node = res.getDbNodeId(i);
      if (node != master && node < victim)
        victim = node;
    }

    int val2[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };    
    if (res.dumpStateOneNode(victim, val2, 2))
      return NDBT_FAILED;
    
    if (res.insertErrorInNode(master, 7200))
      return NDBT_FAILED;
    
    if (res.waitNodesNoStart(&victim, 1))
      return NDBT_FAILED;
    
    if (res.startNodes(&victim, 1))
      return NDBT_FAILED;
    
    if (res.waitClusterStarted())
      return NDBT_FAILED;
  }
  
  return NDBT_OK;
}

int
runBug34216(NDBT_Context* ctx, NDBT_Step* step)
{
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  NdbRestarter restarter;
  int i = 0;
  int lastId = 0;
  HugoOperations hugoOps(*ctx->getTab());
  int records = ctx->getNumRecords();
  Ndb* pNdb = GETNDB(step);

  if (restarter.getNumDbNodes() < 2)
  {
    ctx->stopTest();
    return NDBT_OK;
  }

  if(restarter.waitClusterStarted() != 0){
    g_err << "Cluster failed to start" << endl;
    return NDBT_FAILED;
  }

  char buf[100];
  const char * off = NdbEnv_GetEnv("NDB_ERR_OFFSET", buf, sizeof(buf));
  int offset = off ? atoi(off) : 0;

  while(i<loops && result != NDBT_FAILED && !ctx->isTestStopped())
  {
    if (i > 0 && ctx->closeToTimeout(100 / loops))
      break;

    int id = lastId % restarter.getNumDbNodes();
    int nodeId = restarter.getDbNodeId(id);
    int err = 5048 + ((i+offset) % 2);

    int val2[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };

    if(hugoOps.startTransaction(pNdb) != 0)
      goto err;

    nodeId = hugoOps.getTransaction()->getConnectedNodeId();
    ndbout << "Restart node " << nodeId << " " << err <<endl;

    if (restarter.dumpStateOneNode(nodeId, val2, 2))
      return NDBT_FAILED;

    if(restarter.insertErrorInNode(nodeId, err) != 0){
      g_err << "Failed to restartNextDbNode" << endl;
      result = NDBT_FAILED;
      break;
    }

    if (restarter.insertErrorInNode(nodeId, 8057) != 0)
    {
      g_err << "Failed to insert error 8057" << endl;
      result = NDBT_FAILED;
      break;
    }

    int rows = 25;
    if (rows > records)
      rows = records;

    int batch = 1;
    int row = (records - rows) ? rand() % (records - rows) : 0;
    if (row + rows > records)
      row = records - row;

    /**
     * We should really somehow check that one of the 25 rows
     *   resides in the node we're targeting
     */
    for (int r = row; r < row + rows; r++)
    {
      if(hugoOps.pkUpdateRecord(pNdb, r, batch, rand()) != 0)
        goto err;
      
      for (int l = 1; l<5; l++)
      {
        if (hugoOps.execute_NoCommit(pNdb) != 0)
          goto err;
        
        if(hugoOps.pkUpdateRecord(pNdb, r, batch, rand()) != 0)
          goto err;
      }
    }      

    hugoOps.execute_Commit(pNdb);
    hugoOps.closeTransaction(pNdb);

    if (restarter.waitNodesNoStart(&nodeId, 1))
    {
      g_err << "Failed to waitNodesNoStart" << endl;
      result = NDBT_FAILED;
      break;
    }

    if (restarter.startNodes(&nodeId, 1))
    {
      g_err << "Failed to startNodes" << endl;
      result = NDBT_FAILED;
      break;
    }

    if(restarter.waitClusterStarted() != 0){
      g_err << "Cluster failed to start" << endl;
      result = NDBT_FAILED;
      break;
    }

    lastId++;
    i++;
  }

  ctx->stopTest();

  return result;
err:
  return NDBT_FAILED;
}


int
runNF_commit(NDBT_Context* ctx, NDBT_Step* step)
{
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  NdbRestarter restarter;
  if (restarter.getNumDbNodes() < 2)
  {
    ctx->stopTest();
    return NDBT_OK;
  }

  if(restarter.waitClusterStarted() != 0){
    g_err << "Cluster failed to start" << endl;
    return NDBT_FAILED;
  }

  int i = 0;
  while(i<loops && result != NDBT_FAILED && !ctx->isTestStopped())
  {
    int nodeId = restarter.getDbNodeId(rand() % restarter.getNumDbNodes());
    int err = 5048;

    ndbout << "Restart node " << nodeId << " " << err <<endl;

    int val2[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };
    if (restarter.dumpStateOneNode(nodeId, val2, 2))
      return NDBT_FAILED;

    if(restarter.insertErrorInNode(nodeId, err) != 0){
      g_err << "Failed to restartNextDbNode" << endl;
      result = NDBT_FAILED;
      break;
    }

    if (restarter.waitNodesNoStart(&nodeId, 1))
    {
      g_err << "Failed to waitNodesNoStart" << endl;
      result = NDBT_FAILED;
      break;
    }

    if (restarter.startNodes(&nodeId, 1))
    {
      g_err << "Failed to startNodes" << endl;
      result = NDBT_FAILED;
      break;
    }

    if(restarter.waitClusterStarted() != 0){
      g_err << "Cluster failed to start" << endl;
      result = NDBT_FAILED;
      break;
    }

    i++;
  }

  ctx->stopTest();

  return result;
}

int
runBug34702(NDBT_Context* ctx, NDBT_Step* step)
{
  //int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  //int records = ctx->getNumRecords();
  //Ndb* pNdb = GETNDB(step);
  NdbRestarter res;

  if (res.getNumDbNodes() < 2)
  {
    return NDBT_OK;
  }

  while (loops--)
  {
    int victim = res.getDbNodeId(rand()%res.getNumDbNodes());
    res.restartOneDbNode(victim,
                         /** initial */ true, 
                         /** nostart */ true,
                         /** abort   */ true);

    if (res.waitNodesNoStart(&victim, 1))
      return NDBT_FAILED;

    res.insertErrorInAllNodes(7204);
    res.insertErrorInNode(victim, 7203);

    res.startNodes(&victim, 1);
    
    if (res.waitClusterStarted())
      return NDBT_FAILED;
  }
  return NDBT_OK;
}

int
runMNF(NDBT_Context* ctx, NDBT_Step* step)
{
  //int result = NDBT_OK;
  NdbRestarter res;
  
  if (res.getNumDbNodes() < 2)
  {
    return NDBT_OK;
  }

  Vector<int> part0;
  Vector<int> part1;
  Bitmask<255> part0mask;
  Bitmask<255> part1mask;
  Bitmask<255> ngmask;
  for (int i = 0; i<res.getNumDbNodes(); i++)
  {
    int nodeId = res.getDbNodeId(i);
    int ng = res.getNodeGroup(nodeId);
    if (ngmask.get(ng))
    {
      part1.push_back(nodeId);
      part1mask.set(nodeId);
    }
    else
    {
      ngmask.set(ng);
      part0.push_back(nodeId);
      part0mask.set(nodeId);
    }
  }

  printf("part0: ");
  for (size_t i = 0; i<part0.size(); i++)
    printf("%u ", part0[i]);
  printf("\n");

  printf("part1: ");
  for (size_t i = 0; i<part1.size(); i++)
    printf("%u ", part1[i]);
  printf("\n");

  int loops = ctx->getNumLoops();
  while (loops-- && !ctx->isTestStopped())
  {
    int cnt, *nodes;
    int master = res.getMasterNodeId();
    int nextMaster = res.getNextMasterNodeId(master);

    bool cmf = false;
    if (part0mask.get(master) && part0mask.get(nextMaster))
    {
      cmf = true;
      cnt = part0.size();
      nodes = part0.getBase();
      printf("restarting part0");
    }
    else if(part1mask.get(master) && part1mask.get(nextMaster))
    {
      cmf = true;
      cnt = part1.size();
      nodes = part1.getBase();
      printf("restarting part1");
    }
    else
    {
      cmf = false;
      if (loops & 1)
      {
        cnt = part0.size();
        nodes = part0.getBase();
        printf("restarting part0");
      } 
      else 
      {
        cnt = part1.size();
        nodes = part0.getBase();
        printf("restarting part0");
      }
    }
    
    int val2[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };    
    for (int i = 0; i<cnt; i++)
      if (res.dumpStateOneNode(nodes[i], val2, 2))
        return NDBT_FAILED;
    
    int type = loops;
    char buf[100];
    if (NdbEnv_GetEnv("MNF", buf, sizeof(buf)))
    {
      type = atoi(buf);
    }
    if (cmf)
    {
      type = type % 7;
    }
    else
    {
      type = type % 4;
    }
    ndbout_c(" type: %u (cmf: %u)", type, cmf);
    switch(type){
    case 0:
      for (int i = 0; i<cnt; i++)
      {
        if (res.restartOneDbNode(nodes[i],
                                 /** initial */ false, 
                                 /** nostart */ true,
                                 /** abort   */ true))
          return NDBT_FAILED;
        
        NdbSleep_MilliSleep(10);
      }
      break;
    case 1:
      for (int i = 0; i<cnt; i++)
      {
        if (res.restartOneDbNode(nodes[i],
                                 /** initial */ false, 
                                 /** nostart */ true,
                                 /** abort   */ true))
          return NDBT_FAILED;
        
      }
      break;
    case 2:
      for (int i = 0; i<cnt; i++)
      {
        res.insertErrorInNode(nodes[i], 8058);
      }
      res.restartOneDbNode(nodes[0],
                           /** initial */ false, 
                           /** nostart */ true,
                           /** abort   */ true);
      break;
    case 3:
      for (int i = 0; i<cnt; i++)
      {
        res.insertErrorInNode(nodes[i], 8059);
      }
      res.restartOneDbNode(nodes[0],
                           /** initial */ false, 
                           /** nostart */ true,
                           /** abort   */ true);
      break;
    case 4:
    {
      for (int i = 0; i<cnt; i++)
      {
        if (res.getNextMasterNodeId(master) == nodes[i])
          res.insertErrorInNode(nodes[i], 7180);
        else
          res.insertErrorInNode(nodes[i], 7205);
      }

      int lcp = 7099;
      res.insertErrorInNode(master, 7193);
      res.dumpStateOneNode(master, &lcp, 1);
      break;
    }
    case 5:
    {
      for (int i = 0; i<cnt; i++)
      {
        res.insertErrorInNode(nodes[i], 7206);
      }

      int lcp = 7099;
      res.insertErrorInNode(master, 7193);
      res.dumpStateOneNode(master, &lcp, 1);
      break;
    }
    case 6:
    {
      for (int i = 0; i<cnt; i++)
      {
        res.insertErrorInNode(nodes[i], 5008);
      }
      
      int lcp = 7099;
      res.insertErrorInNode(master, 7193);
      res.dumpStateOneNode(master, &lcp, 1);
      break;
    }
    }
    
    if (res.waitNodesNoStart(nodes, cnt))
      return NDBT_FAILED;
    
    if (res.startNodes(nodes, cnt))
      return NDBT_FAILED;
    
    if (res.waitClusterStarted())
      return NDBT_FAILED; 
  }

  ctx->stopTest();
  return NDBT_OK;
}

int 
runBug36199(NDBT_Context* ctx, NDBT_Step* step)
{
  //int result = NDBT_OK;
  //int loops = ctx->getNumLoops();
  NdbRestarter res;

  if (res.getNumDbNodes() < 4)
    return NDBT_OK;

  int master = res.getMasterNodeId();
  int nextMaster = res.getNextMasterNodeId(master);
  int victim = res.getRandomNodeSameNodeGroup(nextMaster, rand());
  if (victim == master)
  {
    victim = res.getRandomNodeOtherNodeGroup(nextMaster, rand());
  }

  ndbout_c("master: %u next master: %u victim: %u",
           master, nextMaster, victim);

  int val2[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };    
  res.dumpStateOneNode(master, val2, 2);
  res.dumpStateOneNode(victim, val2, 2);

  res.insertErrorInNode(victim, 7205);
  res.insertErrorInNode(master, 7014);
  int lcp = 7099;
  res.dumpStateOneNode(master, &lcp, 1);
  
  int nodes[2];
  nodes[0] = master;
  nodes[1] = victim;
  if (res.waitNodesNoStart(nodes, 2))
  {
    return NDBT_FAILED;
  }

  if (res.startNodes(nodes, 2))
  {
    return NDBT_FAILED;
  }
  
  if (res.waitClusterStarted())
    return NDBT_FAILED;

  return NDBT_OK;
}

int 
runBug36246(NDBT_Context* ctx, NDBT_Step* step)
{ 
  //int result = NDBT_OK;
  //int loops = ctx->getNumLoops();
  NdbRestarter res;
  Ndb* pNdb = GETNDB(step);

  if (res.getNumDbNodes() < 4)
    return NDBT_OK;

  HugoOperations hugoOps(*ctx->getTab());
restartloop:
  int tryloop = 0;
  int master = res.getMasterNodeId();
  int nextMaster = res.getNextMasterNodeId(master);

loop:
  if(hugoOps.startTransaction(pNdb) != 0)
    return NDBT_FAILED;
      
  if(hugoOps.pkUpdateRecord(pNdb, 1, 1) != 0)
    return NDBT_FAILED;
  
  if(hugoOps.execute_NoCommit(pNdb) != 0)
    return NDBT_FAILED;
  
  int victim = hugoOps.getTransaction()->getConnectedNodeId();
  printf("master: %u nextMaster: %u victim: %u",
         master, nextMaster, victim);
  if (victim == master || victim == nextMaster ||
      res.getNodeGroup(victim) == res.getNodeGroup(master) ||
      res.getNodeGroup(victim) == res.getNodeGroup(nextMaster))
  {
    hugoOps.execute_Rollback(pNdb);
    hugoOps.closeTransaction(pNdb);
    tryloop++;
    if (tryloop == 10)
    {
      ndbout_c(" -> restarting next master: %u", nextMaster);
      res.restartOneDbNode(nextMaster,
                           /** initial */ false, 
                           /** nostart */ true,
                           /** abort   */ true);
    
      res.waitNodesNoStart(&nextMaster, 1);
      res.startNodes(&nextMaster, 1);
      if (res.waitClusterStarted())
        return NDBT_FAILED;
      goto restartloop;
    }
    else
    {
      ndbout_c(" -> loop");
      goto loop;
    }
  }
  ndbout_c(" -> go go gadget skates");

  int val2[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };    
  res.dumpStateOneNode(master, val2, 2);
  res.dumpStateOneNode(victim, val2, 2);

  res.insertErrorInNode(master, 8060);
  res.insertErrorInNode(victim, 9999);
  
  int nodes[2];
  nodes[0] = master;
  nodes[1] = victim;
  if (res.waitNodesNoStart(nodes, 2))
  {
    return NDBT_FAILED;
  }
  
  if (res.startNodes(nodes, 2))
  {
    return NDBT_FAILED;
  }
  
  if (res.waitClusterStarted())
    return NDBT_FAILED;

  hugoOps.execute_Rollback(pNdb);
  hugoOps.closeTransaction(pNdb);

  return NDBT_OK;
}

int 
runBug36247(NDBT_Context* ctx, NDBT_Step* step)
{ 
  //int result = NDBT_OK;
  //int loops = ctx->getNumLoops();
  NdbRestarter res;
  Ndb* pNdb = GETNDB(step);

  if (res.getNumDbNodes() < 4)
    return NDBT_OK;

  HugoOperations hugoOps(*ctx->getTab());

restartloop:
  int tryloop = 0;
  int master = res.getMasterNodeId();
  int nextMaster = res.getNextMasterNodeId(master);

loop:
  if(hugoOps.startTransaction(pNdb) != 0)
    return NDBT_FAILED;
      
  if(hugoOps.pkUpdateRecord(pNdb, 1, 100) != 0)
    return NDBT_FAILED;
  
  if(hugoOps.execute_NoCommit(pNdb) != 0)
    return NDBT_FAILED;
  
  int victim = hugoOps.getTransaction()->getConnectedNodeId();
  printf("master: %u nextMaster: %u victim: %u",
         master, nextMaster, victim);
  if (victim == master || victim == nextMaster ||
      res.getNodeGroup(victim) == res.getNodeGroup(master) ||
      res.getNodeGroup(victim) == res.getNodeGroup(nextMaster))
  {
    hugoOps.execute_Rollback(pNdb);
    hugoOps.closeTransaction(pNdb);
    tryloop++;
    if (tryloop == 10)
    {
      ndbout_c(" -> restarting next master: %u", nextMaster);
      res.restartOneDbNode(nextMaster,
                           /** initial */ false, 
                           /** nostart */ true,
                           /** abort   */ true);
      
      res.waitNodesNoStart(&nextMaster, 1);
      res.startNodes(&nextMaster, 1);
      if (res.waitClusterStarted())
        return NDBT_FAILED;
      goto restartloop;
    }
    else
    {
      ndbout_c(" -> loop");
      goto loop;
    }
  }
  ndbout_c(" -> go go gadget skates");
  
  int val2[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };    
  res.dumpStateOneNode(master, val2, 2);
  res.dumpStateOneNode(victim, val2, 2);

  int err5050[] = { 5050 };
  res.dumpStateAllNodes(err5050, 1);

  res.insertErrorInNode(victim, 9999);

  int nodes[2];
  nodes[0] = master;
  nodes[1] = victim;
  if (res.waitNodesNoStart(nodes, 2))
  {
    return NDBT_FAILED;
  }
  
  if (res.startNodes(nodes, 2))
  {
    return NDBT_FAILED;
  }
  
  if (res.waitClusterStarted())
    return NDBT_FAILED;
  
  hugoOps.execute_Rollback(pNdb);
  hugoOps.closeTransaction(pNdb);
  
  return NDBT_OK;
}

int 
runBug36276(NDBT_Context* ctx, NDBT_Step* step)
{ 
  //int result = NDBT_OK;
  //int loops = ctx->getNumLoops();
  NdbRestarter res;
  //Ndb* pNdb = GETNDB(step);
  
  if (res.getNumDbNodes() < 4)
    return NDBT_OK;
  
  int master = res.getMasterNodeId();
  int nextMaster = res.getNextMasterNodeId(master);
  int victim = res.getRandomNodeSameNodeGroup(nextMaster, rand());
  if (victim == master)
  {
    victim = res.getRandomNodeOtherNodeGroup(nextMaster, rand());
  }

  ndbout_c("master: %u nextMaster: %u victim: %u",
           master, nextMaster, victim);

  int val2[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };    
  res.dumpStateOneNode(master, val2, 2);
  res.insertErrorInNode(victim, 7209);

  int lcp = 7099;
  res.dumpStateOneNode(master, &lcp, 1);
  
  if (res.waitNodesNoStart(&master, 1))
  {
    return NDBT_FAILED;
  }
  
  if (res.startNodes(&master, 1))
  {
    return NDBT_FAILED;
  }

  if (res.waitClusterStarted())
    return NDBT_FAILED;

  return NDBT_OK;
}

int 
runBug36245(NDBT_Context* ctx, NDBT_Step* step)
{ 
  //int result = NDBT_OK;
  //int loops = ctx->getNumLoops();
  NdbRestarter res;
  Ndb* pNdb = GETNDB(step);

  if (res.getNumDbNodes() < 4)
    return NDBT_OK;

  /**
   * Make sure master and nextMaster is in different node groups
   */
loop1:
  int master = res.getMasterNodeId();
  int nextMaster = res.getNextMasterNodeId(master);
  
  printf("master: %u nextMaster: %u", master, nextMaster);
  if (res.getNodeGroup(master) == res.getNodeGroup(nextMaster))
  {
    ndbout_c(" -> restarting next master: %u", nextMaster);
    res.restartOneDbNode(nextMaster,
                         /** initial */ false, 
                         /** nostart */ true,
                         /** abort   */ true);
    
    res.waitNodesNoStart(&nextMaster, 1);
    res.startNodes(&nextMaster, 1);
    if (res.waitClusterStarted())
    {
      ndbout_c("cluster didnt restart!!");
      return NDBT_FAILED;
    }
    goto loop1;
  }
  ndbout_c(" -> go go gadget skates");

  int val2[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };    
  res.dumpStateOneNode(master, val2, 2);
  res.dumpStateOneNode(nextMaster, val2, 2);

  res.insertErrorInNode(master, 8063);
  res.insertErrorInNode(nextMaster, 936);


  int err = 0;
  HugoOperations hugoOps(*ctx->getTab());
loop2:
  if((err = hugoOps.startTransaction(pNdb)) != 0)
  {
    ndbout_c("failed to start transaction: %u", err);
    return NDBT_FAILED;
  }
  
  int victim = hugoOps.getTransaction()->getConnectedNodeId();
  if (victim != master)
  {
    ndbout_c("transnode: %u != master: %u -> loop",
             victim, master);
    hugoOps.closeTransaction(pNdb);
    goto loop2;
  }

  if((err = hugoOps.pkUpdateRecord(pNdb, 1)) != 0)
  {
    ndbout_c("failed to update: %u", err);
    return NDBT_FAILED;
  }
  
  if((err = hugoOps.execute_Commit(pNdb)) != 4010)
  {
    ndbout_c("incorrect error code: %u", err);
    return NDBT_FAILED;
  }
  hugoOps.closeTransaction(pNdb);
  
  int nodes[2];
  nodes[0] = master;
  nodes[1] = nextMaster;
  if (res.waitNodesNoStart(nodes, 2))
  {
    return NDBT_FAILED;
  }
  
  if (res.startNodes(nodes, 2))
  {
    return NDBT_FAILED;
  }
  
  if (res.waitClusterStarted())
    return NDBT_FAILED;
  
  return NDBT_OK;
}

int 
runHammer(NDBT_Context* ctx, NDBT_Step* step)
{ 
  int records = ctx->getNumRecords();
  Ndb* pNdb = GETNDB(step);
  HugoOperations hugoOps(*ctx->getTab());
  while (!ctx->isTestStopped())
  {
    int r = rand() % records;
    if (hugoOps.startTransaction(pNdb) != 0)
      continue;
    
    if ((rand() % 100) < 50)
    {
      if (hugoOps.pkUpdateRecord(pNdb, r, 1, rand()) != 0)
        goto err;
    }
    else
    {
      if (hugoOps.pkWriteRecord(pNdb, r, 1, rand()) != 0)
        goto err;
    }
    
    if (hugoOps.execute_NoCommit(pNdb) != 0)
      goto err;
    
    if (hugoOps.pkDeleteRecord(pNdb, r, 1) != 0)
      goto err;
    
    if (hugoOps.execute_NoCommit(pNdb) != 0)
      goto err;
    
    if ((rand() % 100) < 50)
    {
      if (hugoOps.pkInsertRecord(pNdb, r, 1, rand()) != 0)
        goto err;
    }
    else
    {
      if (hugoOps.pkWriteRecord(pNdb, r, 1, rand()) != 0)
        goto err;
    }
    
    if ((rand() % 100) < 90)
    {
      hugoOps.execute_Commit(pNdb);
    }
    else
    {
  err:
      hugoOps.execute_Rollback(pNdb);
    }
    
    hugoOps.closeTransaction(pNdb);
  }
  return NDBT_OK;
}

int 
runMixedLoad(NDBT_Context* ctx, NDBT_Step* step)
{ 
  int res = 0;
  int records = ctx->getNumRecords();
  Ndb* pNdb = GETNDB(step);
  HugoOperations hugoOps(*ctx->getTab());
  unsigned id = (unsigned)rand();
  while (!ctx->isTestStopped())
  {
    if (ctx->getProperty("Pause", (Uint32)0))
    {
      ndbout_c("thread %u stopped", id);
      ctx->sync_down("WaitThreads");
      while (ctx->getProperty("Pause", (Uint32)0) && !ctx->isTestStopped())
        NdbSleep_MilliSleep(15);
      
      if (ctx->isTestStopped())
        break;
      ndbout_c("thread %u continue", id);
    }

    if ((res = hugoOps.startTransaction(pNdb)) != 0)
    {
      if (res == 4009)
        return NDBT_FAILED;
      continue;
    }
    
    for (int i = 0; i < 10; i++)
    {
      int r = rand() % records;
      if ((rand() % 100) < 50)
      {
        if (hugoOps.pkUpdateRecord(pNdb, r, 1, rand()) != 0)
          goto err;
      }
      else
      {
        if (hugoOps.pkWriteRecord(pNdb, r, 1, rand()) != 0)
          goto err;
      }
    }      
    
    if ((rand() % 100) < 90)
    {
      res = hugoOps.execute_Commit(pNdb);
    }
    else
    {
  err:
      res = hugoOps.execute_Rollback(pNdb);
    }
    
    hugoOps.closeTransaction(pNdb);

    if (res == 4009)
    {
      return NDBT_FAILED;
    }
  }
  return NDBT_OK;
}

int
runBug41295(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbRestarter res;

  if (res.getNumDbNodes() < 2)
  {
    ctx->stopTest();
    return NDBT_OK;
  }


  int leak = 4002;
  const int cases = 1;
  int loops = ctx->getNumLoops();
  if (loops <= cases)
    loops = cases + 1;

  for (int i = 0; i<loops; i++)
  {
    int master = res.getMasterNodeId();
    int next = res.getNextMasterNodeId(master);
    
    int val2[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };
    if (res.dumpStateOneNode(next, val2, 2))
      return NDBT_FAILED;
    
    ndbout_c("stopping %u, err 8073", next);
    res.insertErrorInNode(next, 8073);
    ndbout_c("waiting for %u", next);
    res.waitNodesNoStart(&next, 1);
    
    ndbout_c("pausing all threads");
    ctx->setProperty("Pause", 1);
    ctx->sync_up_and_wait("WaitThreads", ctx->getProperty("Threads", 1));
    ndbout_c("all threads paused");
    NdbSleep_MilliSleep(5000);
    res.dumpStateAllNodes(&leak, 1);
    NdbSleep_MilliSleep(1000);
    if (res.checkClusterAlive(&next, 1))
    {
      return NDBT_FAILED;
    }
    ndbout_c("restarting threads");
    ctx->setProperty("Pause", (Uint32)0);
    
    ndbout_c("starting %u", next);
    res.startNodes(&next, 1);
    ndbout_c("waiting for cluster started");
    if (res.waitClusterStarted())
    {
      return NDBT_FAILED;
    }

    ndbout_c("pausing all threads");
    ctx->setProperty("Pause", 1);
    ctx->sync_up_and_wait("WaitThreads", ctx->getProperty("Threads", 1));
    ndbout_c("all threads paused");
    NdbSleep_MilliSleep(5000);
    res.dumpStateAllNodes(&leak, 1);
    NdbSleep_MilliSleep(1000);
    ndbout_c("restarting threads");
    ctx->setProperty("Pause", (Uint32)0);
  }
  
  ctx->stopTest();
  return NDBT_OK;
}

int
runBug41469(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbRestarter res;

  if (res.getNumDbNodes() < 4)
  {
    ctx->stopTest();
    return NDBT_OK;
  }

  int loops = ctx->getNumLoops();

  int val0[] = { 7216, 0 }; 
  int val2[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };
  for (int i = 0; i<loops; i++)
  {
    int master = res.getMasterNodeId();
    int next = res.getNextMasterNodeId(master);
    
    if (res.dumpStateOneNode(master, val2, 2))
      return NDBT_FAILED;
    
    ndbout_c("stopping %u, err 7216 (next: %u)", master, next);
    val0[1] = next;
    if (res.dumpStateOneNode(master, val0, 2))
      return NDBT_FAILED;
    
    res.waitNodesNoStart(&master, 1);
    res.startNodes(&master, 1);
    ndbout_c("waiting for cluster started");
    if (res.waitClusterStarted())
    {
      return NDBT_FAILED;
    }
  }
  ctx->stopTest();
  return NDBT_OK;
}

int
runBug42422(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbRestarter res;
  
  if (res.getNumDbNodes() < 4)
  {
    ctx->stopTest();
    return NDBT_OK;
  }
  
  int loops = ctx->getNumLoops();
  while (--loops >= 0)
  {
    int master = res.getMasterNodeId();
    ndbout_c("master: %u", master);
    int nodeId = res.getRandomNodeSameNodeGroup(master, rand()); 
    ndbout_c("target: %u", nodeId);
    int node2 = res.getRandomNodeOtherNodeGroup(nodeId, rand());
    ndbout_c("node 2: %u", node2);
    
    res.restartOneDbNode(nodeId,
                         /** initial */ false, 
                         /** nostart */ true,
                         /** abort   */ true);
    
    res.waitNodesNoStart(&nodeId, 1);
    
    int dump[] = { 9000, 0 };
    dump[1] = node2;
    
    if (res.dumpStateOneNode(nodeId, dump, 2))
      return NDBT_FAILED;
    
    int val2[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };
    if (res.dumpStateOneNode(nodeId, val2, 2))
      return NDBT_FAILED;
    
    res.insertErrorInNode(nodeId, 937);
    ndbout_c("%u : starting %u", __LINE__, nodeId);
    res.startNodes(&nodeId, 1);
    NdbSleep_SecSleep(3);
    ndbout_c("%u : waiting for %u to not get not-started", __LINE__, nodeId);
    res.waitNodesNoStart(&nodeId, 1);
    
    ndbout_c("%u : starting %u", __LINE__, nodeId);
    res.startNodes(&nodeId, 1);
    
    ndbout_c("%u : waiting for cluster started", __LINE__);
    if (res.waitClusterStarted())
    {
      return NDBT_FAILED;
    }
  }

  ctx->stopTest();
  return NDBT_OK;
}

int
runBug43224(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbRestarter res;
  
  if (res.getNumDbNodes() < 2)
  {
    ctx->stopTest();
    return NDBT_OK;
  }
  
  int loops = ctx->getNumLoops();
  while (--loops >= 0)
  {
    int nodeId = res.getNode(NdbRestarter::NS_RANDOM);
    res.restartOneDbNode(nodeId,
                         /** initial */ false, 
                         /** nostart */ true,
                         /** abort   */ true);
    
    res.waitNodesNoStart(&nodeId, 1);

    NdbSleep_SecSleep(10);
    
    int val2[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };
    if (res.dumpStateOneNode(nodeId, val2, 2))
      return NDBT_FAILED;
    
    res.insertErrorInNode(nodeId, 9994);
    res.startNodes(&nodeId, 1);
    NdbSleep_SecSleep(3);
    ndbout_c("%u : waiting for %u to not get not-started", __LINE__, nodeId);
    res.waitNodesNoStart(&nodeId, 1);

    if (res.dumpStateOneNode(nodeId, val2, 2))
      return NDBT_FAILED;
    
    res.insertErrorInNode(nodeId, 9994);
    res.startNodes(&nodeId, 1);
    NdbSleep_SecSleep(3);
    ndbout_c("%u : waiting for %u to not get not-started", __LINE__, nodeId);
    res.waitNodesNoStart(&nodeId, 1);
    
    NdbSleep_SecSleep(20); // Hardcoded in ndb_mgmd (alloc timeout)

    ndbout_c("%u : starting %u", __LINE__, nodeId);
    res.startNodes(&nodeId, 1);
    
    ndbout_c("%u : waiting for cluster started", __LINE__);
    if (res.waitClusterStarted())
    {
      return NDBT_FAILED;
    }
  }

  ctx->stopTest();
  return NDBT_OK;
}

int
runBug43888(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbRestarter res;
  
  if (res.getNumDbNodes() < 2)
  {
    ctx->stopTest();
    return NDBT_OK;
  }
  
  int loops = ctx->getNumLoops();
  while (--loops >= 0)
  {
    int master = res.getMasterNodeId();
    ndbout_c("master: %u", master);
    int nodeId = master;
    do {
      nodeId = res.getNode(NdbRestarter::NS_RANDOM);
    } while (nodeId == master);

    ndbout_c("target: %u", nodeId);
    
    res.restartOneDbNode(nodeId,
                         /** initial */ false, 
                         /** nostart */ true,
                         /** abort   */ true);
    
    res.waitNodesNoStart(&nodeId, 1);
    
    int val2[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };
    if (res.dumpStateOneNode(nodeId, val2, 2))
      return NDBT_FAILED;
    
    res.insertErrorInNode(master, 7217);
    res.startNodes(&nodeId, 1);
    NdbSleep_SecSleep(3);
    ndbout_c("%u : waiting for %u to not get not-started", __LINE__, nodeId);
    res.waitNodesNoStart(&nodeId, 1);
    
    ndbout_c("%u : starting %u", __LINE__, nodeId);
    res.startNodes(&nodeId, 1);
    
    ndbout_c("%u : waiting for cluster started", __LINE__);
    if (res.waitClusterStarted())
    {
      return NDBT_FAILED;
    }
  }

  ctx->stopTest();
  return NDBT_OK;
}

#define CHECK(b, m) { int _xx = b; if (!(_xx)) { \
  ndbout << "ERR: "<< m \
           << "   " << "File: " << __FILE__ \
           << " (Line: " << __LINE__ << ")" << "- " << _xx << endl; \
  return NDBT_FAILED; } }

int
runBug44952(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbRestarter res;
  NdbDictionary::Dictionary* pDict = GETNDB(step)->getDictionary();

  const int codes [] = {
    5051, 5052, 5053, 0
  }; (void)codes;

  //int randomId = myRandom48(res.getNumDbNodes());
  //int nodeId = res.getDbNodeId(randomId);

  int loops = ctx->getNumLoops();
  const int val[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 } ;
  for (int l = 0; l < loops; l++)
  {
    int randomId = myRandom48(res.getNumDbNodes());
    int nodeId = res.getDbNodeId(randomId);

    ndbout_c("killing node %u error 5051 loop %u/%u", nodeId, l+1, loops);
    CHECK(res.dumpStateOneNode(nodeId, val, 2) == 0,
          "failed to set RestartOnErrorInsert");

    CHECK(res.insertErrorInNode(nodeId, 5051) == 0,
          "failed to insert error 5051");

    while (res.waitNodesNoStart(&nodeId, 1, 1 /* seconds */) != 0)
    {
      pDict->forceGCPWait();
    }

    ndbout_c("killing node %u during restart error 5052", nodeId);
    for (int j = 0; j < 3; j++)
    {
      ndbout_c("loop: %d - killing node %u during restart error 5052",
               j, nodeId);
      int val[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 } ;
      CHECK(res.dumpStateOneNode(nodeId, val, 2) == 0,
            "failed to set RestartOnErrorInsert");

      CHECK(res.insertErrorInNode(nodeId, 5052) == 0,
            "failed to set error insert");

      NdbSleep_SecSleep(3); // ...

      CHECK(res.startNodes(&nodeId, 1) == 0,
            "failed to start node");

      NdbSleep_SecSleep(3);

      CHECK(res.waitNodesNoStart(&nodeId, 1) == 0,
            "waitNodesNoStart failed");
    }

    CHECK(res.startNodes(&nodeId, 1) == 0,
          "failed to start node");

    CHECK(res.waitNodesStarted(&nodeId, 1) == 0,
          "waitNodesStarted failed");
  }

  ctx->stopTest();
  return NDBT_OK;
}

static BaseString tab_48474;

int
initBug48474(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbDictionary::Table tab = * ctx->getTab();
  NdbDictionary::Dictionary* pDict = GETNDB(step)->getDictionary();

  const NdbDictionary::Table * pTab = pDict->getTable(tab.getName());
  if (pTab == 0)
    return NDBT_FAILED;

  /**
   * Create a table with tableid > ctx->getTab()
   */
  Uint32 cnt = 0;
  Vector<BaseString> tables;
  do
  {
    BaseString tmp;
    tmp.assfmt("%s_%u", tab.getName(), cnt);
    tab.setName(tmp.c_str());

    pDict->dropTable(tab.getName());
    if (pDict->createTable(tab) != 0)
      return NDBT_FAILED;

    const NdbDictionary::Table * pTab2 = pDict->getTable(tab.getName());
    if (pTab2->getObjectId() < pTab->getObjectId())
    {
      tables.push_back(tmp);
    }
    else
    {
      tab_48474 = tmp;
      HugoTransactions hugoTrans(* pTab2);
      if (hugoTrans.loadTable(GETNDB(step), 1000) != 0)
      {
        return NDBT_FAILED;
      }
      break;
    }
    cnt++;
  } while(true);

  // Now delete the extra one...
  for (Uint32 i = 0; i<tables.size(); i++)
  {
    pDict->dropTable(tables[i].c_str());
  }

  tables.clear();

  return NDBT_OK;
}

int
runBug48474(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbRestarter res;
  NdbDictionary::Dictionary* pDict = GETNDB(step)->getDictionary();
  const NdbDictionary::Table * pTab = pDict->getTable(tab_48474.c_str());
  Ndb* pNdb = GETNDB(step);
  HugoOperations hugoOps(* pTab);

  int nodeId = res.getNode(NdbRestarter::NS_RANDOM);
  ndbout_c("restarting %d", nodeId);
  res.restartOneDbNode(nodeId, false, true, true);
  res.waitNodesNoStart(&nodeId, 1);

  int minlcp[] = { 7017, 1 };
  res.dumpStateAllNodes(minlcp, 1); // Set min time between LCP

  ndbout_c("starting %d", nodeId);
  res.startNodes(&nodeId, 1);

  Uint32 minutes = 5;
  ndbout_c("starting uncommitted transaction %u minutes", minutes);
  for (Uint32 m = 0; m < minutes; m++)
  {
    if (hugoOps.startTransaction(pNdb) != 0)
      return NDBT_FAILED;

    if (hugoOps.pkUpdateRecord(pNdb, 0, 50, rand()) != 0)
      return NDBT_FAILED;

    if (hugoOps.execute_NoCommit(pNdb) != 0)
      return NDBT_FAILED;


    ndbout_c("sleeping 60s");
    for (Uint32 i = 0; i<600 && !ctx->isTestStopped(); i++)
    {
      hugoOps.getTransaction()->refresh();
      NdbSleep_MilliSleep(100);
    }

    if (hugoOps.execute_Commit(pNdb) != 0)
      return NDBT_FAILED;

    hugoOps.closeTransaction(pNdb);

    if (ctx->isTestStopped())
      break;
  }


  res.dumpStateAllNodes(minlcp, 2); // reset min time between LCP

  ctx->stopTest();
  return NDBT_OK;
}

int
cleanupBug48474(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbDictionary::Dictionary* pDict = GETNDB(step)->getDictionary();
  pDict->dropTable(tab_48474.c_str());
  return NDBT_OK;
}

int
runBug56044(NDBT_Context* ctx, NDBT_Step* step)
{
  int loops = ctx->getNumLoops();
  NdbRestarter res;

  if (res.getNumDbNodes() < 2)
    return NDBT_OK;

  for (int i = 0; i<loops; i++)
  {
    int master = res.getMasterNodeId();
    int next = res.getNextMasterNodeId(master);
    ndbout_c("master: %u next: %u", master, next);

    int val2[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };

    if (res.dumpStateOneNode(master, val2, 2))
      return NDBT_FAILED;

    if (res.insertErrorInNode(next, 7224))
      return NDBT_FAILED;

    res.waitNodesNoStart(&master, 1);
    res.startNodes(&master, 1);
    if (res.waitClusterStarted() != 0)
      return NDBT_FAILED;
  }

  return NDBT_OK;
}

int
runBug57767(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbRestarter res;

  if (res.getNumDbNodes() < 2)
    return NDBT_OK;

  int node0 = res.getNode(NdbRestarter::NS_RANDOM);
  int node1 = res.getRandomNodeSameNodeGroup(node0, rand());
  ndbout_c("%u %u", node0, node1);

  res.restartOneDbNode(node0, false, true, true);
  res.waitNodesNoStart(&node0, 1);
  res.insertErrorInNode(node0, 1000);

  HugoTransactions hugoTrans(*ctx->getTab());
  hugoTrans.scanUpdateRecords(GETNDB(step), 0);

  res.insertErrorInNode(node1, 5060);
  res.startNodes(&node0, 1);
  res.waitClusterStarted();
  return NDBT_OK;
}

int
runBug57522(NDBT_Context* ctx, NDBT_Step* step)
{
  int loops = ctx->getNumLoops();
  NdbRestarter res;

  if (res.getNumDbNodes() < 4)
    return NDBT_OK;

  for (int i = 0; i<loops; i++)
  {
    int master = res.getMasterNodeId();
    int next0 = res.getNextMasterNodeId(master);
    int next1 = res.getNextMasterNodeId(next0);
    ndbout_c("master: %d next0: %d next1: %d", master, next0, next1);

    int val2[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };

    if (res.dumpStateOneNode(master, val2, 2))
      return NDBT_FAILED;

    int val3[] = { 7999, 7226, next1 };
    if (res.dumpStateOneNode(master, val3, 3))
      return NDBT_FAILED;

    res.waitNodesNoStart(&master, 1);
    res.startNodes(&master, 1);
    if (res.waitClusterStarted() != 0)
      return NDBT_FAILED;
  }

  return NDBT_OK;
}

int
runForceStopAndRestart(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbRestarter res;
  if (res.getNumDbNodes() < 2)
    return NDBT_OK;

  Vector<int> group1;
  Vector<int> group2;
  Bitmask<256/32> nodeGroupMap;
  for (int j = 0; j<res.getNumDbNodes(); j++)
  {
    int node = res.getDbNodeId(j);
    int ng = res.getNodeGroup(node);
    if (nodeGroupMap.get(ng))
    {
      group2.push_back(node);
    }
    else
    {
      group1.push_back(node);
      nodeGroupMap.set(ng);
    }
  }

  printf("group1: ");
  for (size_t i = 0; i<group1.size(); i++)
    printf("%d ", group1[i]);
  printf("\n");

  printf("group2: ");
  for (size_t i = 0; i<group2.size(); i++)
    printf("%d ", group2[i]);
  printf("\n");

  // Stop half of the cluster
  res.restartNodes(group1.getBase(), (int)group1.size(),
                   NdbRestarter::NRRF_NOSTART | NdbRestarter::NRRF_ABORT);
  res.waitNodesNoStart(group1.getBase(), (int)group1.size());

  ndbout_c("%u", __LINE__);
  // Try to stop first node in second half without force, should return error
  if (res.restartOneDbNode(group2[0],
                           false, /* initial */
                           true,  /* nostart  */
                           false, /* abort */
                           false  /* force */) != -1)
  {
    ndbout_c("%u", __LINE__);
    g_err << "Restart suceeded without force" << endl;
    return NDBT_FAILED;
  }

  ndbout_c("%u", __LINE__);

  // Now stop with force
  if (res.restartOneDbNode(group2[0],
                           false, /* initial */
                           true,  /* nostart  */
                           false, /* abort */
                           true   /* force */) != 0)
  {
    ndbout_c("%u", __LINE__);
    g_err << "Could not restart with force" << endl;
    return NDBT_FAILED;
  }

  ndbout_c("%u", __LINE__);

  // All nodes should now be in nostart, the above stop force
  // cvaused the remainig nodes to be stopped(and restarted nostart)
  res.waitClusterNoStart();

  ndbout_c("%u", __LINE__);

  // Start second half back up again
  res.startNodes(group2.getBase(), (int)group2.size());
  res.waitNodesStarted(group2.getBase(), (int)group2.size());

  ndbout_c("%u", __LINE__);

  // Try to stop remaining half without force, should return error
  if (res.restartNodes(group2.getBase(), (int)group2.size(),
                       NdbRestarter::NRRF_NOSTART) != -1)
  {
    g_err << "Restart suceeded without force" << endl;
    return NDBT_FAILED;
  }

  ndbout_c("%u", __LINE__);

  // Now stop with force
  if (res.restartNodes(group2.getBase(), (int)group2.size(),
                       NdbRestarter::NRRF_NOSTART |
                       NdbRestarter::NRRF_FORCE) != 0)
  {
    g_err << "Could not restart with force" << endl;
    return NDBT_FAILED;
  }

  ndbout_c("%u", __LINE__);

  if (res.waitNodesNoStart(group2.getBase(), (int)group2.size()))
  {
    g_err << "Failed to waitNodesNoStart" << endl;
    return NDBT_FAILED;
  }

  // Start all nodes again
  res.startAll();
  res.waitClusterStarted();

  return NDBT_OK;
}

int
runBug58453(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbRestarter res;
  if (res.getNumDbNodes() < 4)
    return NDBT_OK;

  Ndb* pNdb = GETNDB(step);
  HugoOperations hugoOps(*ctx->getTab());

  int loops = ctx->getNumLoops();
  while (loops--)
  {
    if (hugoOps.startTransaction(pNdb) != 0)
      return NDBT_FAILED;

    if (hugoOps.pkInsertRecord(pNdb, 0, 128 /* records */) != 0)
      return NDBT_FAILED;

    int err = 5062;
    switch(loops & 1){
    case 0:
      err = 5062;
      break;
    case 1:
      err = 5063;
      break;
    }
    int node = (int)hugoOps.getTransaction()->getConnectedNodeId();
    int node0 = res.getRandomNodeOtherNodeGroup(node, rand());
    int node1 = res.getRandomNodeSameNodeGroup(node0, rand());

    ndbout_c("node %u err: %u, node: %u err: %u",
             node0, 5061, node1, err);

    int val2[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };

    res.dumpStateOneNode(node, val2, 2);
    res.insertErrorInNode(node0, 5061);
    res.insertErrorInNode(node1, err);

    hugoOps.execute_Commit(pNdb);
    hugoOps.closeTransaction(pNdb);

    res.waitNodesNoStart(&node, 1);
    res.startNodes(&node, 1);
    res.waitClusterStarted();
    hugoOps.clearTable(pNdb);
  }

  return NDBT_OK;
}



int runRestartToDynamicOrder(NDBT_Context* ctx, NDBT_Step* step)
{
  /* Here we perform node restarts to get the various node's
   * dynamic ids in a particular order
   * This affects which nodes heartbeat which (low -> high)
   * and which is the president (lowest).
   * Each restarting node gets a higher dynamic id, so the
   * first node restarted will eventually become president
   * Note that we're assuming NoOfReplicas == 2 here.
   */
  /* TODO :
   * Refactor into
   *   1) Get current cluster dynorder info
   *   2) Choose a dynorder info
   *   3) Restart to given dynorder if necessary
   */
  Uint32 dynOrder = ctx->getProperty("DynamicOrder", Uint32(0));
  NdbRestarter restarter;
  Uint32 numNodes = restarter.getNumDbNodes();

  Vector<Uint32> currOrder;
  Vector<Uint32> newOrder;
  Vector<Uint32> odds;
  Vector<Uint32> evens;

  if (numNodes == 2)
  {
    ndbout_c("No Dynamic reordering possible with 2 nodes");
    return NDBT_OK;
  }
  if (numNodes & 1)
  {
    ndbout_c("Non multiple-of-2 number of nodes.  Not supported");
    return NDBT_FAILED;
  }

  Uint32 master = restarter.getMasterNodeId();

  for (Uint32 n=0; n < numNodes; n++)
  {
    currOrder.push_back(master);
    master = restarter.getNextMasterNodeId(master);
  }

  for (Uint32 n=0; n < numNodes; n++)
  {
    Uint32 nodeId = restarter.getDbNodeId(n);
    if (nodeId & 1)
    {
      odds.push_back(nodeId);
    }
    else
    {
      evens.push_back(nodeId);
    }
  }

  if (odds.size() != evens.size())
  {
    ndbout_c("Failed - odds.size() (%u) != evens.size() (%u)",
             odds.size(),
             evens.size());
    return NDBT_FAILED;
  }

  ndbout_c("Current dynamic ordering : ");
  for (Uint32 n=0; n<numNodes; n++)
  {
    ndbout_c("  %u %s", currOrder[n], ((n==0)?"*":""));
  }

  if (dynOrder == 0)
  {
    ndbout_c("No change in dynamic order");
    return NDBT_OK;
  }

  Uint32 control= dynOrder - 1;

  bool oddPresident = control & 1;
  bool interleave = control & 2;
  bool reverseSideA = control & 4;
  bool reverseSideB = control & 8;

  /*     Odds first    Interleave O/E  Reverse A  Reverse B
   * 1       N              N              N         N
   * 2       Y              N              N         N
   * 3       N              Y              N         N
   * 4       Y              Y              N         N
   * 5       N              N              Y         N
   * 6       Y              N              Y         N
   * 7       N              Y              Y         N
   * 8       Y              Y              Y         N
   * 9       N              N              N         Y
   * 10      Y              N              N         Y
   * 11      N              Y              N         Y
   * 12      Y              Y              N         Y
   * 13      N              N              Y         Y
   * 14      Y              N              Y         Y
   * 15      N              Y              Y         Y
   * 16      Y              Y              Y         Y
   *
   * Interesting values
   *   1) Even first, no interleave, no reverse
   *      e.g. 2->4->6->3->5->7
   *   2) Odd first, no interleave, no reverse
   *      e.g. 3->5->7->2->4->6
   *   3) Even first, interleave, no reverse
   *      e.g. 2->3->4->5->6->7
   *   9) Even first, no interleave, reverse B
   *      e.g. 2->4->6->7->5->3
   *
   *  'First' node becomes president.
   *  Which node(s) monitor president affects when
   *  arbitration may be required
   */

  ndbout_c("Generating ordering with %s president, sides %sinterleaved",
           (oddPresident?"odd": "even"),
           (interleave?"":"not "));
  if (reverseSideA)
    ndbout_c("  %s reversed", (oddPresident?"odds": "evens"));

    if (reverseSideB)
    ndbout_c("  %s reversed", (oddPresident?"evens": "odds"));

  Vector<Uint32>* sideA;
  Vector<Uint32>* sideB;

  if (oddPresident)
  {
    sideA = &odds;
    sideB = &evens;
  }
  else
  {
    sideA = &evens;
    sideB = &odds;
  }

  if (interleave)
  {
    for (Uint32 n=0; n < sideA->size(); n++)
    {
      Uint32 indexA = reverseSideA? (sideA->size() - (n+1)) : n;
      newOrder.push_back((*sideA)[indexA]);
      Uint32 indexB = reverseSideB? (sideB->size() - (n+1)) : n;
      newOrder.push_back((*sideB)[indexB]);
    }
  }
  else
  {
    for (Uint32 n=0; n < sideA->size(); n++)
    {
      Uint32 indexA = reverseSideA? (sideA->size() - (n+1)) : n;
      newOrder.push_back((*sideA)[indexA]);
    }
    for (Uint32 n=0; n < sideB->size(); n++)
    {
      Uint32 indexB = reverseSideB? (sideB->size() - (n+1)) : n;
      newOrder.push_back((*sideB)[indexB]);
    }
  }


  bool diff = false;
  for (Uint32 n=0; n < newOrder.size(); n++)
  {
    ndbout_c("  %u %s",
             newOrder[n],
             ((n==0)?"*":" "));

    diff |= (newOrder[n] != currOrder[n]);
  }

  if (!diff)
  {
    ndbout_c("Cluster already in correct configuration");
    return NDBT_OK;
  }

  for (Uint32 n=0; n < newOrder.size(); n++)
  {
    ndbout_c("Now restarting node %u", newOrder[n]);
    if (restarter.restartOneDbNode(newOrder[n],
                                   false, // initial
                                   true,  // nostart
                                   true)  // abort
        != NDBT_OK)
    {
      ndbout_c("Failed to restart node");
      return NDBT_FAILED;
    }
    if (restarter.waitNodesNoStart((const int*) &newOrder[n], 1) != NDBT_OK)
    {
      ndbout_c("Failed waiting for node to enter NOSTART state");
      return NDBT_FAILED;
    }
    if (restarter.startNodes((const int*) &newOrder[n], 1) != NDBT_OK)
    {
      ndbout_c("Failed to start node");
      return NDBT_FAILED;
    }
    if (restarter.waitNodesStarted((const int*) &newOrder[n], 1) != NDBT_OK)
    {
      ndbout_c("Failed waiting for node to start");
      return NDBT_FAILED;
    }
    ndbout_c("  Done.");
  }

  ndbout_c("All restarts completed.  NdbRestarter says master is %u",
           restarter.getMasterNodeId());
  if (restarter.getMasterNodeId() != (int) newOrder[0])
  {
    ndbout_c("  Should be %u, failing", newOrder[0]);
    return NDBT_FAILED;
  }

  return NDBT_OK;
}

struct NodeGroupMembers
{
  Uint32 ngid;
  Uint32 membCount;
  Uint32 members[4];
};

template class Vector<NodeGroupMembers>;

int analyseDynamicOrder(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbRestarter restarter;
  Uint32 numNodes = restarter.getNumDbNodes();
  Uint32 master = restarter.getMasterNodeId();
  Vector<Uint32> dynamicOrder;
  Vector<Uint32> nodeGroup;
  Vector<Uint32> monitorsNode;
  Vector<Uint32> monitoredByNode;
  Vector<Uint32> monitorsRemote;
  Vector<Uint32> remoteMonitored;
  Vector<Uint32> sameNGMonitored;
  Vector<Uint32> distanceToRemote;
  Vector<Uint32> nodeIdToDynamicIndex;
  Uint32 maxDistanceToRemoteLink = 0;

  /* TODO :
   * Refactor into :
   *   1) Determine dynorder from running cluster
   *   2) Analyse dynorder in general
   *   3) Analyse dynorder from point of view of latency split
   *
   *   4) Support splits other than odd/even total
   *      - Partial split
   *      - Some link failures
   */

  /* Determine dynamic order from running cluster */
  for (Uint32 n=0; n < numNodes; n++)
  {
    dynamicOrder.push_back(master);
    nodeGroup.push_back(restarter.getNodeGroup(master));
    master = restarter.getNextMasterNodeId(master);
    Uint32 zero=0;
    nodeIdToDynamicIndex.set(n, master, zero);
  }

  /* Look at implied HB links */
  for (Uint32 n=0; n < numNodes; n++)
  {
    Uint32 nodeId = dynamicOrder[n];
    Uint32 monitoredByIndex = (n+1) % numNodes;
    Uint32 monitorsIndex = (n+ numNodes - 1) % numNodes;
    monitoredByNode.push_back(dynamicOrder[monitoredByIndex]);
    monitorsNode.push_back(dynamicOrder[monitorsIndex]);
    remoteMonitored.push_back((nodeId & 1) != (monitoredByNode[n] & 1));
    monitorsRemote.push_back((nodeId & 1) != (monitorsNode[n] & 1));
    sameNGMonitored.push_back(nodeGroup[n] == nodeGroup[monitoredByIndex]);
  }

  /* Look at split implications */
  for (Uint32 n=0; n < numNodes; n++)
  {
    Uint32 distanceToRemoteHBLink = 0;
    for (Uint32 m=0; m < numNodes; m++)
    {
      if (remoteMonitored[n+m])
        break;
      distanceToRemoteHBLink++;
    }

    distanceToRemote.push_back(distanceToRemoteHBLink);
    maxDistanceToRemoteLink = MAX(maxDistanceToRemoteLink, distanceToRemoteHBLink);
  }


  ndbout_c("Dynamic order analysis");

  for (Uint32 n=0; n < numNodes; n++)
  {
    ndbout_c("  %u %s %u%s%u%s%u \t Monitored by %s nodegroup, Dist to remote link : %u",
             dynamicOrder[n],
             ((n==0)?"*":" "),
             monitorsNode[n],
             ((monitorsRemote[n])?"  >":"-->"),
             dynamicOrder[n],
             ((remoteMonitored[n])?"  >":"-->"),
             monitoredByNode[n],
             ((sameNGMonitored[n])?"same":"other"),
             distanceToRemote[n]);
  }

  ndbout_c("\n");

  Vector<NodeGroupMembers> nodeGroupMembers;

  for (Uint32 n=0; n < numNodes; n++)
  {
    Uint32 ng = nodeGroup[n];

    bool ngfound = false;
    for (Uint32 m = 0; m < nodeGroupMembers.size(); m++)
    {
      if (nodeGroupMembers[m].ngid == ng)
      {
        NodeGroupMembers& ngInfo = nodeGroupMembers[m];
        ngInfo.members[ngInfo.membCount++] = dynamicOrder[n];
        ngfound = true;
        break;
      }
    }

    if (!ngfound)
    {
      NodeGroupMembers newGroupInfo;
      newGroupInfo.ngid = ng;
      newGroupInfo.membCount = 1;
      newGroupInfo.members[0] = dynamicOrder[n];
      nodeGroupMembers.push_back(newGroupInfo);
    }
  }

  ndbout_c("Nodegroups");

  for (Uint32 n=0; n < nodeGroupMembers.size(); n++)
  {
    ndbout << "  " << nodeGroupMembers[n].ngid << " (";
    bool allRemoteMonitored = true;
    for (Uint32 m=0; m < nodeGroupMembers[n].membCount; m++)
    {
      Uint32 nodeId = nodeGroupMembers[n].members[m];
      ndbout << nodeId;
      if ((m+1) < nodeGroupMembers[n].membCount)
        ndbout << ",";
      Uint32 dynamicIndex = nodeIdToDynamicIndex[nodeId];
      allRemoteMonitored &= remoteMonitored[dynamicIndex];
    }
    ndbout << ") Entirely remote monitored NGs risk : "
           << (allRemoteMonitored?"Y":"N") << "\n";
  }
  ndbout_c("\n");

  ndbout_c("Cluster-split latency behaviour");

  Uint32 oddPresident = dynamicOrder[0];
  Uint32 evenPresident = dynamicOrder[0];

  for (Uint32 n=0; n <= maxDistanceToRemoteLink; n++)
  {
    Vector<Uint32> failedNodeGroups;
    ndbout << "  " << n <<" HB latency period(s), nodes (";
    bool useComma = false;
    bool presidentFailed = false;
    for (Uint32 m=0; m < numNodes; m++)
    {
      if (distanceToRemote[m] == n)
      {
        Uint32 failingNodeId = dynamicOrder[m];
        if (useComma)
          ndbout << ",";

        useComma = true;
        ndbout << failingNodeId;

        if ((failingNodeId == evenPresident) ||
            (failingNodeId == oddPresident))
        {
          ndbout << "*";
          presidentFailed = true;
        }

        {
          Uint32 ng = nodeGroup[m];
          for (Uint32 i=0; i< nodeGroupMembers.size(); i++)
          {
            if (nodeGroupMembers[i].ngid == ng)
            {
              if ((--nodeGroupMembers[i].membCount) == 0)
              {
                failedNodeGroups.push_back(ng);
              }
            }
          }
        }
      }
    }
    ndbout << ") will be declared failed." << endl;
    if (failedNodeGroups.size() != 0)
    {
      ndbout << "    NG failure risk on reconnect for nodegroups : ";
      for (Uint32 i=0; i< failedNodeGroups.size(); i++)
      {
        if (i > 0)
          ndbout << ",";
        ndbout << failedNodeGroups[i];
      }
      ndbout << endl;
    }
    if (presidentFailed)
    {
      /* A president (even/odd/both) has failed, we should
       * calculate the new president(s) from the p.o.v.
       * of both sides
       */
      Uint32 newOdd=0;
      Uint32 newEven=0;
      for (Uint32 i=0; i< numNodes; i++)
      {
        /* Each side finds either the first node on their
         * side, or the first node on the other side which
         * is still 'alive' from their point of view
         */
        bool candidateIsOdd = dynamicOrder[i] & 1;

        if (!newOdd)
        {
          if (candidateIsOdd ||
              (distanceToRemote[i] > n))
          {
            newOdd = dynamicOrder[i];
          }
        }
        if (!newEven)
        {
          if ((!candidateIsOdd) ||
              (distanceToRemote[i] > n))
          {
            newEven = dynamicOrder[i];
          }
        }
      }

      bool oddPresidentFailed = (oddPresident != newOdd);
      bool evenPresidentFailed = (evenPresident != newEven);

      if (oddPresidentFailed)
      {
        ndbout_c("    Odd president (%u) failed, new odd president : %u",
                 oddPresident, newOdd);
        oddPresident = newOdd;
      }
      if (evenPresidentFailed)
      {
        ndbout_c("    Even president (%u) failed, new even president : %u",
                 evenPresident, newEven);
        evenPresident = newEven;
      }

      if (oddPresident != evenPresident)
      {
        ndbout_c("    President role duplicated, Odd (%u), Even (%u)",
                 oddPresident, evenPresident);
      }

    }
  }

  ndbout << endl << endl;

  return NDBT_OK;
}

int runSplitLatency25PctFail(NDBT_Context* ctx, NDBT_Step* step)
{
  /* Use dump commands to inject artificial inter-node latency
   * Use an error insert to cause latency to disappear when
   * a node observes > 25% of nodes failed.
   * This should trigger a race of FAIL_REQs from both sides
   * of the cluster, and can result in cluster failure
   */
  NdbRestarter restarter;

  /*
   * First set the ConnectCheckIntervalDelay to 1500
   */
  {
    int dump[] = { 9994, 1500 };
    restarter.dumpStateAllNodes(dump, 2);
  }

  {
    int val2[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };
    restarter.dumpStateAllNodes(val2, 2);
  }

  /* First the error insert which will drop latency (QMGR) */
  restarter.insertErrorInAllNodes(938);

  /* Now the dump code which causes the system to experience
   * latency along odd/even lines (CMVMI)
   *
   */
  int dumpStateArgs[] = {9990, 1};
  restarter.dumpStateAllNodes(dumpStateArgs, 2);

  /**
   * Now wait for half of cluster to die...
   */
  const int node_count = restarter.getNumDbNodes();
  ndbout_c("Waiting for half of cluster (%u/%u) to die", node_count/2, node_count);
  int not_started = 0;
  do
  {
    not_started = 0;
    for (int i = 0; i < node_count; i++)
    {
      int nodeId = restarter.getDbNodeId(i);
      int status = restarter.getNodeStatus(nodeId);
      ndbout_c("Node %u status %u", nodeId, status);
      if (status == NDB_MGM_NODE_STATUS_NOT_STARTED)
        not_started++;
    }
    NdbSleep_MilliSleep(2000);
    ndbout_c("%u / %u in state NDB_MGM_NODE_STATUS_NOT_STARTED(%u)",
             not_started, node_count, NDB_MGM_NODE_STATUS_NOT_STARTED);
  } while (2 * not_started != node_count);

  ndbout_c("Restarting cluster");
  restarter.restartAll(false, true, true);
  ndbout_c("Waiting cluster not started");
  restarter.waitClusterNoStart();

  ndbout_c("Starting");
  restarter.startAll();
  restarter.waitClusterStarted();

  return NDBT_OK;
}

int
runMasterFailSlowLCP(NDBT_Context* ctx, NDBT_Step* step)
{
  /* Motivated by bug# 13323589 */
  NdbRestarter res;

  if (res.getNumDbNodes() < 4)
  {
    return NDBT_OK;
  }

  int master = res.getMasterNodeId();
  int otherVictim = res.getRandomNodeOtherNodeGroup(master, rand());
  int nextMaster = res.getNextMasterNodeId(master);
  nextMaster = (nextMaster == otherVictim) ? res.getNextMasterNodeId(otherVictim) :
    nextMaster;
  assert(nextMaster != master);
  assert(nextMaster != otherVictim);

  /* Get a node which is not current or next master */
  int slowNode= nextMaster;
  while ((slowNode == nextMaster) ||
         (slowNode == otherVictim) ||
         (slowNode == master))
  {
    slowNode = res.getRandomNotMasterNodeId(rand());
  }

  ndbout_c("master: %d otherVictim : %d nextMaster: %d slowNode: %d",
           master,
           otherVictim,
           nextMaster,
           slowNode);

  /* Steps :
   * 1. Insert slow LCP frag error in slowNode
   * 2. Start LCP
   * 3. Wait for LCP to start
   * 4. Kill at least two nodes including Master
   * 5. Wait for killed nodes to attempt to rejoin
   * 6. Remove slow LCP error
   * 7. Allow system to stabilise + check no errors
   */
  // 5073 = Delay on handling BACKUP_FRAGMENT_CONF in LQH
  if (res.insertErrorInNode(slowNode, 5073))
  {
    return NDBT_FAILED;
  }

  {
    int req[1] = {DumpStateOrd::DihStartLcpImmediately};
    if (res.dumpStateOneNode(master, req, 1))
    {
      return NDBT_FAILED;
    }
  }

  ndbout_c("Giving LCP time to start...");

  NdbSleep_SecSleep(10);

  ndbout_c("Killing other victim node (%u)...", otherVictim);

  if (res.restartOneDbNode(otherVictim, false, false, true))
  {
    return NDBT_FAILED;
  }

  ndbout_c("Killing Master node (%u)...", master);

  if (res.restartOneDbNode(master, false, false, true))
  {
    return NDBT_FAILED;
  }

  /*
     ndbout_c("Waiting for old Master node to enter NoStart state...");
     if (res.waitNodesNoStart(&master, 1, 10))
     return NDBT_FAILED;

     ndbout_c("Starting old Master...");
     if (res.startNodes(&master, 1))
     return NDBT_FAILED;

  */
  ndbout_c("Waiting for some progress on old Master and other victim restart");
  NdbSleep_SecSleep(15);

  ndbout_c("Now removing error insert on slow node (%u)", slowNode);

  if (res.insertErrorInNode(slowNode, 0))
  {
    return NDBT_FAILED;
  }

  ndbout_c("Now wait a while to check stability...");
  NdbSleep_SecSleep(30);

  if (res.getNodeStatus(master) == NDB_MGM_NODE_STATUS_NOT_STARTED)
  {
    ndbout_c("Old Master needs kick to restart");
    if (res.startNodes(&master, 1))
    {
      return NDBT_FAILED;
    }
  }

  ndbout_c("Wait for cluster recovery...");
  if (res.waitClusterStarted())
  {
    return NDBT_FAILED;
  }


  ndbout_c("Done");
  return NDBT_OK;
}

int
runBug13464664(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbRestarter res;
  if (res.getNumDbNodes() < 4)
    return NDBT_OK;

  /**
   * m = master
   * o = node in other node-group than next master
   * p = not master and node o
   *
   * o error 7230 - responde to MASTER_LCPREQ quickly and die
   * p error 7231 - responde slowly to MASTER_LCPREQ
   * m error 7025 - die during LCP_FRAG_REP
   * m dump 7099  - force LCP
   *
   */
loop:
  int m = res.getMasterNodeId();
  int n = res.getNextMasterNodeId(m);
  int o = res.getRandomNodeOtherNodeGroup(n, rand());
  ndbout_c("m: %u n: %u o: %u", m, n, o);
  if (res.getNodeGroup(o) == res.getNodeGroup(m))
  {
    ndbout_c("=> restart n(%u)", n);
    res.restartOneDbNode(n,
                         /** initial */ false, 
                         /** nostart */ true,
                         /** abort   */ true);
    res.waitNodesNoStart(&n, 1);
    res.startNodes(&n, 1);
    res.waitClusterStarted();
    goto loop;
  }

  ndbout_c("search p");
loop2:
  int p = res.getNode(NdbRestarter::NS_RANDOM);
  while (p == n || p == o || p == m)
    goto loop2;
  ndbout_c("p: %u\n", p);

  int val2[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };
  res.dumpStateOneNode(o, val2, 2);
  res.dumpStateOneNode(m, val2, 2);

  res.insertErrorInNode(o, 7230);
  res.insertErrorInNode(p, 7231);
  res.insertErrorInNode(m, 7025);
  int val1[] = { 7099 };
  res.dumpStateOneNode(m, val1, 1);

  int list[2] = { m, o };
  res.waitNodesNoStart(list, 2);
  res.startNodes(list, 2);
  res.waitClusterStarted();

  return NDBT_OK;
}

int master_err[] =
{
  7025, // LCP_FRG_REP in DIH
  5056, // LCP complete rep from LQH
  7191, // execLCP_COMPLETE_REP in DIH
  7015, // execSTART_LCP_CONF in DIH
  0
};

int other_err[] =
{
  7205, // execMASTER_LCPREQ
  7206, // execEMPTY_LCP_CONF
  7230, // sendMASTER_LCPCONF and die
  7232, // Die after sending MASTER_LCPCONF
  0
};

int
runLCPTakeOver(NDBT_Context* ctx, NDBT_Step* step)
{
  {
    NdbRestarter res;
    if (res.getNumDbNodes() < 4)
    {
      ctx->stopTest();
      return NDBT_OK;
    }
  }

  for (int i = 0; master_err[i] != 0; i++)
  {
    int errno1 = master_err[i];
    for (int j = 0; other_err[j] != 0; j++)
    {
      int errno2 = other_err[j];

      /**
       * we want to kill master,
       *   and kill another node during LCP take-ove (not new master)
       */
      NdbRestarter res;
      int master = res.getMasterNodeId();
      int next = res.getNextMasterNodeId(master);
  loop:
      int victim = res.getRandomNodeOtherNodeGroup(master, rand());
      while (next == victim)
        goto loop;

      ndbout_c("master: %u next: %u victim: %u master-err: %u victim-err: %u",
               master, next, victim, errno1, errno2);

      int val2[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };
      res.dumpStateOneNode(master, val2, 2);
      res.dumpStateOneNode(victim, val2, 2);
      res.insertErrorInNode(next, 7233);
      res.insertErrorInNode(victim, errno2);
      res.insertErrorInNode(master, errno1);

      int val1[] = { 7099 };
      res.dumpStateOneNode(master, val1, 1);
      int list[] = { master, victim };
      res.waitNodesNoStart(list, 2);
      res.startNodes(list, 2);
      res.waitClusterStarted();
    }
  }

  ctx->stopTest();
  return NDBT_OK;
}

int
runTestScanFragWatchdog(NDBT_Context* ctx, NDBT_Step* step)
{
  /* Setup an error insert, then start a checkpoint */
  NdbRestarter restarter;
  if (restarter.getNumDbNodes() < 2)
  {
    g_err << "Insufficient nodes for test." << endl;
    ctx->stopTest();
    return NDBT_OK;
  }
  
  do
  {
    g_err << "Injecting fault to suspend LCP frag scan..." << endl;
    Uint32 victim = restarter.getNode(NdbRestarter::NS_RANDOM);
    Uint32 otherNode = 0;
    do
    {
      otherNode = restarter.getNode(NdbRestarter::NS_RANDOM);
    } while (otherNode == victim);

    if (restarter.insertErrorInNode(victim, 10039) != 0) /* Cause LCP/backup frag scan to halt */
    {
      g_err << "Error insert failed." << endl;
      break;
    }
    if (restarter.insertErrorInNode(victim, 5075) != 0) /* Treat watchdog fail as test success */
    {
      g_err << "Error insert failed." << endl;
      break;
    }
    
    g_err << "Triggering LCP..." << endl;
    /* Now trigger LCP, in case the concurrent updates don't */
    {
      int startLcpDumpCode = 7099;
      if (restarter.dumpStateOneNode(victim, &startLcpDumpCode, 1))
      {
        g_err << "Dump state failed." << endl;
        break;
      }
    }
    
    g_err << "Subscribing to MGMD events..." << endl;

    NdbMgmd mgmd;
    
    if (!mgmd.connect())
    {
      g_err << "Failed to connect to MGMD" << endl;
      break;
    }

    if (!mgmd.subscribe_to_events())
    {
      g_err << "Failed to subscribe to events" << endl;
      break;
    }

    g_err << "Waiting to hear of LCP completion..." << endl;
    Uint32 completedLcps = 0;
    Uint64 maxWaitSeconds = 240;
    Uint64 endTime = NdbTick_CurrentMillisecond() + 
      (maxWaitSeconds * 1000);
    
    while (NdbTick_CurrentMillisecond() < endTime)
    {
      char buff[512];
      
      if (!mgmd.get_next_event_line(buff,
                                    sizeof(buff),
                                    10 * 1000))
      {
        g_err << "Failed to get event line " << endl;
        break;
      }

      // g_err << "Event : " << buff;

      if (strstr(buff, "Local checkpoint") &&
          strstr(buff, "completed"))
      {
        completedLcps++;
        g_err << "LCP " << completedLcps << " completed." << endl;
        
        if (completedLcps == 2)
          break;

        /* Request + wait for another... */
        {
          int startLcpDumpCode = 7099;
          if (restarter.dumpStateOneNode(otherNode, &startLcpDumpCode, 1))
          {
            g_err << "Dump state failed." << endl;
            break;
          }
        }
      } 
    }
    
    if (completedLcps != 2)
    {
      g_err << "Some problem while waiting for LCP completion" << endl;
      break;
    }

    /* Now wait for the node to recover */
    if (restarter.waitNodesStarted((const int*) &victim, 1, 120) != 0)
    {
      g_err << "Failed waiting for node " << victim << "to start" << endl;
      break;
    }
    
    ctx->stopTest();
    return NDBT_OK;
  } while (0);
  
  ctx->stopTest();
  return NDBT_FAILED;
}


NDBT_TESTSUITE(testNodeRestart);
TESTCASE("NoLoad", 
	 "Test that one node at a time can be stopped and then restarted "\
	 "when there are no load on the system. Do this loop number of times"){ 
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runRestarter);
  FINALIZER(runClearTable);
}
TESTCASE("PkRead", 
	 "Test that one node at a time can be stopped and then restarted "\
	 "perform pk read while restarting. Do this loop number of times"){ 
  TC_PROPERTY("ReadLockMode", NdbOperation::LM_Read);
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runRestarter);
  STEP(runPkReadUntilStopped);
  FINALIZER(runClearTable);
}
TESTCASE("PkReadCommitted", 
	 "Test that one node at a time can be stopped and then restarted "\
	 "perform pk read while restarting. Do this loop number of times"){ 
  TC_PROPERTY("ReadLockMode", NdbOperation::LM_CommittedRead);
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runRestarter);
  STEP(runPkReadUntilStopped);
  FINALIZER(runClearTable);
}
TESTCASE("MixedPkRead", 
	 "Test that one node at a time can be stopped and then restarted "\
	 "perform pk read while restarting. Do this loop number of times"){ 
  TC_PROPERTY("ReadLockMode", Uint32(-1));
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runRestarter);
  STEP(runPkReadUntilStopped);
  FINALIZER(runClearTable);
}
TESTCASE("PkReadPkUpdate", 
	 "Test that one node at a time can be stopped and then restarted "\
	 "perform pk read and pk update while restarting. Do this loop number of times"){ 
  TC_PROPERTY("ReadLockMode", NdbOperation::LM_Read);
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runRestarter);
  STEP(runPkReadUntilStopped);
  STEP(runPkUpdateUntilStopped);
  STEP(runPkReadPkUpdateUntilStopped);
  STEP(runPkReadUntilStopped);
  STEP(runPkUpdateUntilStopped);
  STEP(runPkReadPkUpdateUntilStopped);
  FINALIZER(runClearTable);
}
TESTCASE("MixedPkReadPkUpdate", 
	 "Test that one node at a time can be stopped and then restarted "\
	 "perform pk read and pk update while restarting. Do this loop number of times"){ 
  TC_PROPERTY("ReadLockMode", Uint32(-1));
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runRestarter);
  STEP(runPkReadUntilStopped);
  STEP(runPkUpdateUntilStopped);
  STEP(runPkReadPkUpdateUntilStopped);
  STEP(runPkReadUntilStopped);
  STEP(runPkUpdateUntilStopped);
  STEP(runPkReadPkUpdateUntilStopped);
  FINALIZER(runClearTable);
}
TESTCASE("ReadUpdateScan", 
	 "Test that one node at a time can be stopped and then restarted "\
	 "perform pk read, pk update and scan reads while restarting. Do this loop number of times"){ 
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runRestarter);
  STEP(runPkReadUntilStopped);
  STEP(runPkUpdateUntilStopped);
  STEP(runPkReadPkUpdateUntilStopped);
  STEP(runScanReadUntilStopped);
  STEP(runScanUpdateUntilStopped);
  FINALIZER(runClearTable);
}
TESTCASE("MixedReadUpdateScan", 
	 "Test that one node at a time can be stopped and then restarted "\
	 "perform pk read, pk update and scan reads while restarting. Do this loop number of times"){ 
  TC_PROPERTY("ReadLockMode", Uint32(-1));
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runRestarter);
  STEP(runPkReadUntilStopped);
  STEP(runPkUpdateUntilStopped);
  STEP(runPkReadPkUpdateUntilStopped);
  STEP(runScanReadUntilStopped);
  STEP(runScanUpdateUntilStopped);
  FINALIZER(runClearTable);
}
TESTCASE("Terror", 
	 "Test that one node at a time can be stopped and then restarted "\
	 "perform all kind of transactions while restarting. Do this loop number of times"){ 
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runRestarter);
  STEP(runPkReadUntilStopped);
  STEP(runPkUpdateUntilStopped);
  STEP(runScanReadUntilStopped);
  STEP(runScanUpdateUntilStopped);
  FINALIZER(runClearTable);
}
TESTCASE("FullDb", 
	 "Test that one node at a time can be stopped and then restarted "\
	 "when db is full. Do this loop number of times"){ 
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runFillTable);
  STEP(runRestarter);
}
TESTCASE("RestartRandomNode", 
	 "Test that we can execute the restart RestartRandomNode loop\n"\
	 "number of times"){ 
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runRestarts);
  FINALIZER(runScanReadVerify);
  FINALIZER(runClearTable);
}
TESTCASE("RestartRandomNodeError", 
	 "Test that we can execute the restart RestartRandomNodeError loop\n"\
	 "number of times"){ 
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runRestarts);
  FINALIZER(runScanReadVerify);
  FINALIZER(runClearTable);
}
TESTCASE("RestartRandomNodeInitial", 
	 "Test that we can execute the restart RestartRandomNodeInitial loop\n"\
	 "number of times"){ 
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runRestarts);
  FINALIZER(runScanReadVerify);
  FINALIZER(runClearTable);
}
TESTCASE("RestartNFDuringNR", 
	 "Test that we can execute the restart RestartNFDuringNR loop\n"\
	 "number of times"){ 
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runRestarts);
  STEP(runPkUpdateUntilStopped);
  STEP(runScanUpdateUntilStopped);
  FINALIZER(runScanReadVerify);
  FINALIZER(runClearTable);
}
TESTCASE("RestartMasterNodeError", 
	 "Test that we can execute the restart RestartMasterNodeError loop\n"\
	 "number of times"){ 
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runRestarts);
  FINALIZER(runScanReadVerify);
  FINALIZER(runClearTable);
}

TESTCASE("TwoNodeFailure", 
	 "Test that we can execute the restart TwoNodeFailure\n"\
	 "(which is a multiple node failure restart) loop\n"\
	 "number of times"){ 
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runRestarts);
  FINALIZER(runScanReadVerify);
  FINALIZER(runClearTable);
}
TESTCASE("TwoMasterNodeFailure", 
	 "Test that we can execute the restart TwoMasterNodeFailure\n"\
	 "(which is a multiple node failure restart) loop\n"\
	 "number of times"){ 
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runRestarts);
  FINALIZER(runScanReadVerify);
  FINALIZER(runClearTable);
}
TESTCASE("FiftyPercentFail", 
	 "Test that we can execute the restart FiftyPercentFail\n"\
	 "(which is a multiple node failure restart) loop\n"\
	 "number of times"){ 
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runRestarts);
  FINALIZER(runScanReadVerify);
  FINALIZER(runClearTable);
}
TESTCASE("RestartAllNodes", 
	 "Test that we can execute the restart RestartAllNodes\n"\
	 "(which is a system  restart) loop\n"\
	 "number of times"){ 
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runRestarts);
  FINALIZER(runScanReadVerify);
  FINALIZER(runClearTable);
}
TESTCASE("RestartAllNodesAbort", 
	 "Test that we can execute the restart RestartAllNodesAbort\n"\
	 "(which is a system  restart) loop\n"\
	 "number of times"){ 
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runRestarts);
  FINALIZER(runScanReadVerify);
  FINALIZER(runClearTable);
}
TESTCASE("RestartAllNodesError9999", 
	 "Test that we can execute the restart RestartAllNodesError9999\n"\
	 "(which is a system  restart) loop\n"\
	 "number of times"){ 
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runRestarts);
  FINALIZER(runScanReadVerify);
  FINALIZER(runClearTable);
}
TESTCASE("FiftyPercentStopAndWait", 
	 "Test that we can execute the restart FiftyPercentStopAndWait\n"\
	 "(which is a system  restart) loop\n"\
	 "number of times"){
  INITIALIZER(runCheckAllNodesStarted); 
  INITIALIZER(runLoadTable);
  STEP(runRestarts);
  FINALIZER(runScanReadVerify);
  FINALIZER(runClearTable);
}
TESTCASE("RestartNodeDuringLCP", 
	 "Test that we can execute the restart RestartRandomNode loop\n"\
	 "number of times"){ 
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runRestarts);
  STEP(runPkUpdateUntilStopped);
  STEP(runScanUpdateUntilStopped);
  FINALIZER(runScanReadVerify);
  FINALIZER(runClearTable);
}
TESTCASE("StopOnError", 
	 "Test StopOnError. A node that has StopOnError set to false "\
	 "should restart automatically when an error occurs"){ 
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runRestarts);
  FINALIZER(runScanReadVerify);
  FINALIZER(runClearTable);
}
TESTCASE("CommittedRead", 
	 "Test committed read"){ 
  INITIALIZER(runLoadTable);
  STEP(runDirtyRead);
  FINALIZER(runClearTable);
}
TESTCASE("LateCommit",
	 "Test commit after node failure"){
  INITIALIZER(runLoadTable);
  STEP(runLateCommit);
  FINALIZER(runClearTable);
}
TESTCASE("Bug15587",
	 "Test bug with NF during NR"){
  INITIALIZER(runLoadTable);
  STEP(runScanUpdateUntilStopped);
  STEP(runBug15587);
  FINALIZER(runClearTable);
}
TESTCASE("Bug15632",
	 "Test bug with NF during NR"){
  INITIALIZER(runLoadTable);
  STEP(runBug15632);
  FINALIZER(runClearTable);
}
TESTCASE("Bug15685",
	 "Test bug with NF during abort"){
  STEP(runBug15685);
  FINALIZER(runClearTable);
}
TESTCASE("Bug16772",
	 "Test bug with restarting before NF handling is complete"){
  STEP(runBug16772);
}
TESTCASE("Bug18414",
	 "Test bug with NF during NR"){
  INITIALIZER(runLoadTable);
  STEP(runBug18414);
  FINALIZER(runClearTable);
}
TESTCASE("Bug18612",
	 "Test bug with partitioned clusters"){
  INITIALIZER(runLoadTable);
  STEP(runBug18612);
  FINALIZER(runClearTable);
}
TESTCASE("Bug18612SR",
	 "Test bug with partitioned clusters"){
  INITIALIZER(runLoadTable);
  STEP(runBug18612SR);
  FINALIZER(runClearTable);
}
TESTCASE("Bug20185",
	 ""){
  INITIALIZER(runLoadTable);
  STEP(runBug20185);
  FINALIZER(runClearTable);
}
TESTCASE("Bug24543", "")
{
  INITIALIZER(runBug24543);
}
TESTCASE("Bug21271",
	 ""){
  INITIALIZER(runLoadTable);
  STEP(runBug21271);
  STEP(runPkUpdateUntilStopped);
  FINALIZER(runClearTable);
}
TESTCASE("Bug24717", ""){
  INITIALIZER(runBug24717);
}
TESTCASE("Bug25364", ""){
  INITIALIZER(runBug25364);
}
TESTCASE("Bug25468", ""){
  INITIALIZER(runBug25468);
}
TESTCASE("Bug25554", ""){
  INITIALIZER(runBug25554);
}
TESTCASE("Bug25984", ""){
  INITIALIZER(runBug25984);
}
TESTCASE("Bug26457", ""){
  INITIALIZER(runBug26457);
}
TESTCASE("Bug26481", ""){
  INITIALIZER(runBug26481);
}
TESTCASE("Bug26450", ""){
  INITIALIZER(runLoadTable);
  INITIALIZER(runBug26450);
}
TESTCASE("Bug27003", ""){
  INITIALIZER(runBug27003);
}
TESTCASE("Bug27283", ""){
  INITIALIZER(runBug27283);
}
TESTCASE("Bug27466", ""){
  INITIALIZER(runBug27466);
}
TESTCASE("Bug28023", ""){
  INITIALIZER(runBug28023);
}
TESTCASE("Bug28717", ""){
  INITIALIZER(runBug28717);
}
TESTCASE("Bug31980", ""){
  INITIALIZER(runBug31980);
}
TESTCASE("Bug29364", ""){
  INITIALIZER(runBug29364);
}
TESTCASE("GCP", ""){
  INITIALIZER(runLoadTable);
  STEP(runGCP);
  STEP(runScanUpdateUntilStopped);
  FINALIZER(runClearTable);
}
TESTCASE("CommitAck", ""){
  INITIALIZER(runCommitAck);
  FINALIZER(runClearTable);
}
TESTCASE("Bug32160", ""){
  INITIALIZER(runBug32160);
}
TESTCASE("pnr", "Parallel node restart")
{
  TC_PROPERTY("ScanUpdateNoRowCountCheck", 1);
  INITIALIZER(runLoadTable);
  INITIALIZER(runCreateBigTable);
  STEP(runScanUpdateUntilStopped);
  STEP(runDeleteInsertUntilStopped);
  STEP(runPnr);
  FINALIZER(runClearTable);
  FINALIZER(runDropBigTable);
}
TESTCASE("pnr_lcp", "Parallel node restart")
{
  TC_PROPERTY("LCP", 1);
  TC_PROPERTY("ScanUpdateNoRowCountCheck", 1);
  INITIALIZER(runLoadTable);
  INITIALIZER(runCreateBigTable);
  STEP(runScanUpdateUntilStopped);
  STEP(runDeleteInsertUntilStopped);
  STEP(runPnr);
  FINALIZER(runClearTable);
  FINALIZER(runDropBigTable);
}
TESTCASE("Bug32922", ""){
  INITIALIZER(runBug32922);
}
TESTCASE("Bug34216", ""){
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runBug34216);
  FINALIZER(runClearTable);
}
TESTCASE("mixedmultiop", ""){
  TC_PROPERTY("MULTI_OP", 5);
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runNF_commit);
  STEP(runPkUpdateUntilStopped);
  STEP(runPkUpdateUntilStopped);
  FINALIZER(runClearTable);
}
TESTCASE("Bug34702", ""){
  INITIALIZER(runBug34702);  
}
TESTCASE("MNF", ""){
  INITIALIZER(runLoadTable);
  STEP(runMNF);
  STEP(runScanUpdateUntilStopped);
}
TESTCASE("Bug36199", ""){
  INITIALIZER(runBug36199);
}
TESTCASE("Bug36246", ""){
  INITIALIZER(runLoadTable);
  STEP(runBug36246);
  VERIFIER(runClearTable);
}
TESTCASE("Bug36247", ""){
  INITIALIZER(runLoadTable);
  STEP(runBug36247);
  VERIFIER(runClearTable);
}
TESTCASE("Bug36276", ""){
  INITIALIZER(runLoadTable);
  STEP(runBug36276);
  VERIFIER(runClearTable);
}
TESTCASE("Bug36245", ""){
  INITIALIZER(runLoadTable);
  STEP(runBug36245);
  VERIFIER(runClearTable);
}
TESTCASE("NF_Hammer", ""){
  TC_PROPERTY("Sleep0", 9000);
  TC_PROPERTY("Sleep1", 3000);
  TC_PROPERTY("Rand", 1);
  INITIALIZER(runLoadTable);
  STEPS(runHammer, 25);
  STEP(runRestarter);
  VERIFIER(runClearTable);
}
TESTCASE("Bug41295", "")
{
  TC_PROPERTY("Threads", 25);
  INITIALIZER(runLoadTable);
  STEPS(runMixedLoad, 25);
  STEP(runBug41295);
  FINALIZER(runClearTable);
}
TESTCASE("Bug41469", ""){
  INITIALIZER(runLoadTable);
  STEP(runBug41469);
  STEP(runScanUpdateUntilStopped);
  FINALIZER(runClearTable);
}
TESTCASE("Bug42422", ""){
  INITIALIZER(runBug42422);
}
TESTCASE("Bug43224", ""){
  INITIALIZER(runBug43224);
}
TESTCASE("Bug58453", "")
{
  INITIALIZER(runBug58453);
}
TESTCASE("Bug43888", ""){
  INITIALIZER(runBug43888);
}
TESTCASE("Bug44952",
	 "Test that we can execute the restart RestartNFDuringNR loop\n" \
	 "number of times"){
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runBug44952);
  STEP(runPkUpdateUntilStopped);
  STEP(runScanUpdateUntilStopped);
  FINALIZER(runScanReadVerify);
  FINALIZER(runClearTable);
}
TESTCASE("Bug48474", "")
{
  INITIALIZER(runLoadTable);
  INITIALIZER(initBug48474);
  STEP(runBug48474);
  STEP(runScanUpdateUntilStopped);
  FINALIZER(cleanupBug48474);
}
TESTCASE("MixReadUnlockRestart",
         "Run mixed read+unlock and update transactions"){
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runPkReadPkUpdateUntilStopped);
  STEP(runPkReadPkUpdatePkUnlockUntilStopped);
  STEP(runPkReadPkUpdatePkUnlockUntilStopped);
  STEP(runRestarter);
  FINALIZER(runClearTable);
}
TESTCASE("Bug56044", "")
{
  INITIALIZER(runBug56044);
}
TESTCASE("Bug57767", "")
{
  INITIALIZER(runLoadTable);
  INITIALIZER(runBug57767)
}
TESTCASE("Bug57522", "")
{
  INITIALIZER(runBug57522);
}
TESTCASE("MasterFailSlowLCP",
         "DIH Master failure during a slow LCP can cause a crash.")
{
  INITIALIZER(runMasterFailSlowLCP);
}
TESTCASE("ForceStopAndRestart", "Test restart and stop -with force flag")
{
  STEP(runForceStopAndRestart);
}
TESTCASE("ClusterSplitLatency",
         "Test behaviour of 2-replica cluster with latency between halves")
{
  TC_PROPERTY("DynamicOrder", Uint32(9));
  INITIALIZER(runRestartToDynamicOrder);
  INITIALIZER(analyseDynamicOrder);
  INITIALIZER(runSplitLatency25PctFail);
}
TESTCASE("Bug13464664", "")
{
  INITIALIZER(runBug13464664);
}
TESTCASE("LCPTakeOver", "")
{
  INITIALIZER(runCheckAllNodesStarted);
  INITIALIZER(runLoadTable);
  STEP(runLCPTakeOver);
  STEP(runPkUpdateUntilStopped);
  STEP(runScanUpdateUntilStopped);
}
TESTCASE("LCPScanFragWatchdog", 
         "Test LCP scan watchdog")
{
  INITIALIZER(runLoadTable);
  STEP(runPkUpdateUntilStopped);
  STEP(runTestScanFragWatchdog);
}

NDBT_TESTSUITE_END(testNodeRestart);

int main(int argc, const char** argv){
  ndb_init();
  NDBT_TESTSUITE_INSTANCE(testNodeRestart);
#if 0
  // It might be interesting to have longer defaults for num
  // loops in this test
  // Just performing 100 node restarts would not be enough?
  // We can have initialisers in the NDBT_Testcase class like 
  // this...
  testNodeRestart.setDefaultLoops(1000);
#endif
  return testNodeRestart.execute(argc, argv);
}
