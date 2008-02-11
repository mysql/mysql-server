/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

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
#include <NdbRestarts.hpp>
#include <Vector.hpp>
#include <signaldata/DumpStateOrd.hpp>
#include <Bitmask.hpp>
#include <RefConvert.hpp>
#include <NdbEnv.h>

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

int runPkReadPkUpdateUntilStopped(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
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

int runScanUpdateUntilStopped(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  int records = ctx->getNumRecords();
  int parallelism = ctx->getProperty("Parallelism", 1);
  int abort = ctx->getProperty("AbortProb", (Uint32)0);
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
  
  loops *= restarter.getNumDbNodes();
  while(i<loops && result != NDBT_FAILED && !ctx->isTestStopped()){

    int id = lastId % restarter.getNumDbNodes();
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

  while(i<loops && result != NDBT_FAILED && !ctx->isTestStopped()){
    
    if(restarts.executeRestart(pCase->getName(), timeout) != 0){
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

int runLateCommit(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
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
    while((nodeId = restarter.getDbNodeId(id)) == transNode)
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

int runBug15587(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
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

int runBug15632(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
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
      if (restarter.insertErrorInNode(node1, 8050))
	goto err;
    }
    
    int val2[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };
    if (restarter.dumpStateOneNode(node2, val2, 2))
      goto err;
    
    if (restarter.insertErrorInNode(node2, 5003))
      goto err;
    
    int res= hugoOps.execute_Rollback(pNdb);
  
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
    bzero(partition0, sizeof(partition0));
    bzero(partition1, sizeof(partition1));
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
    bzero(partition0, sizeof(partition0));
    bzero(partition1, sizeof(partition1));
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

int runBug20185(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  NdbRestarter restarter;
  HugoOperations hugoOps(*ctx->getTab());
  Ndb* pNdb = GETNDB(step);
  
  const int masterNode = restarter.getMasterNodeId();

  int dump[] = { 7090, 20 } ;
  if (restarter.dumpStateAllNodes(dump, 2))
    return NDBT_FAILED;
  
  NdbSleep_MilliSleep(3000);
  Vector<int> nodes;
  for (Uint32 i = 0; i<restarter.getNumDbNodes(); i++)
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

int runBug24717(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
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
runBug29364(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
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

int runBug25364(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  NdbRestarter restarter;
  Ndb* pNdb = GETNDB(step);
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
runBug21271(NDBT_Context* ctx, NDBT_Step* step){
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  NdbRestarter restarter;
  HugoOperations hugoOps(*ctx->getTab());
  Ndb* pNdb = GETNDB(step);
  
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

int runBug25468(NDBT_Context* ctx, NDBT_Step* step){
  
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
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

int runBug25554(NDBT_Context* ctx, NDBT_Step* step){
  
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
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

int runBug25984(NDBT_Context* ctx, NDBT_Step* step){
  
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  NdbRestarter restarter;

  if (restarter.getNumDbNodes() < 2)
    return NDBT_OK;

  if (restarter.restartAll(true, true, true))
    return NDBT_FAILED;

  if (restarter.waitClusterNoStart())
    return NDBT_FAILED;

  if (restarter.startAll())
    return NDBT_FAILED;

  if (restarter.waitClusterStarted())
    return NDBT_FAILED;

  int val2[] = { DumpStateOrd::CmvmiSetRestartOnErrorInsert, 1 };    
  int master = restarter.getMasterNodeId();
  int victim = restarter.getRandomNodeOtherNodeGroup(master, rand());
  if (victim == -1)
    victim = restarter.getRandomNodeSameNodeGroup(master, rand());

  restarter.restartOneDbNode(victim, false, true, true);

  for (Uint32 i = 0; i<6; i++)
  {
    ndbout_c("Loop: %d", i);
    if (restarter.waitNodesNoStart(&victim, 1))
      return NDBT_FAILED;
    
    if (restarter.dumpStateOneNode(victim, val2, 2))
      return NDBT_FAILED;
    
    if (restarter.insertErrorInNode(victim, 7016))
      return NDBT_FAILED;
    
    if (restarter.startNodes(&victim, 1))
      return NDBT_FAILED;

    NdbSleep_SecSleep(3);
  }

  if (restarter.waitNodesNoStart(&victim, 1))
    return NDBT_FAILED;

  if (restarter.dumpStateOneNode(victim, val2, 2))
    return NDBT_FAILED;
  
  if (restarter.insertErrorInNode(victim, 7170))
    return NDBT_FAILED;

  if (restarter.startNodes(&victim, 1))
    return NDBT_FAILED;

  if (restarter.waitNodesNoStart(&victim, 1))
    return NDBT_FAILED;
  
  if (restarter.restartAll(false, true, true))
    return NDBT_FAILED;

  if (restarter.insertErrorInAllNodes(932))
    return NDBT_FAILED;

  if (restarter.insertErrorInNode(master, 7170))
    return NDBT_FAILED;

  if (restarter.dumpStateAllNodes(val2, 2))
    return NDBT_FAILED;
  
  restarter.startNodes(&master, 1);
  NdbSleep_MilliSleep(3000);
  restarter.startAll();

  if (restarter.waitClusterNoStart())
    return NDBT_FAILED;

  if (restarter.restartOneDbNode(victim, true, true, true))
    return NDBT_FAILED;

  if (restarter.startAll())
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
  
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
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
  Uint32 i;
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  NdbRestarter res;
  Ndb* pNdb = GETNDB(step);
  
  int node = res.getRandomNotMasterNodeId(rand());
  Vector<int> nodes;
  for (unsigned i = 0; i<res.getNumDbNodes(); i++)
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

  for (i = 0; i < 2; i++)
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
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  NdbRestarter res;
  
  static const int errnos[] = { 4025, 4026, 4027, 4028, 0 };

  int node = res.getRandomNotMasterNodeId(rand());
  ndbout_c("node: %d", node);
  if (res.restartOneDbNode(node, true, true, true))
    return NDBT_FAILED;

  Uint32 pos = 0;
  for (Uint32 i = 0; i<loops; i++)
  {
    while (errnos[pos] != 0)
    {
      ndbout_c("Tesing err: %d", errnos[pos]);
      
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
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  NdbRestarter res;

  if (res.getNumDbNodes() < 2)
  {
    return NDBT_OK;
  }

  static const int errnos[] = { 7181, 7182, 0 };
  
  Uint32 pos = 0;
  for (Uint32 i = 0; i<loops; i++)
  {
    while (errnos[pos] != 0)
    {
      int master = res.getMasterNodeId();
      int next = res.getNextMasterNodeId(master);
      int next2 = res.getNextMasterNodeId(next);
      
      int node = (i & 1) ? next : next2;
      ndbout_c("Tesing err: %d", errnos[pos]);
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
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  NdbRestarter res;

  if (res.getNumDbNodes() < 2)
  {
    return NDBT_OK;
  }

  Uint32 pos = 0;
  for (Uint32 i = 0; i<loops; i++)
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
  int result = NDBT_OK;
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

  for (Uint32 i = 0; i<loops; i++)
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
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  Ndb* pNdb = GETNDB(step);
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
  7005, 7006, 7007, 7008, 5000, 0
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
  int result = NDBT_OK;
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

    if (runerrors(res, NdbRestarter::NS_MASTER, f_master_failure))
    {
      return NDBT_FAILED;
    }
  }
  ctx->stopTest();
  return NDBT_OK;
}

int
runBug31525(NDBT_Context* ctx, NDBT_Step* step)
{
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  Ndb* pNdb = GETNDB(step);
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
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
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
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  Ndb* pNdb = GETNDB(step);
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
  int result = NDBT_OK;
  int loops = ctx->getNumLoops();
  int records = ctx->getNumRecords();
  Ndb* pNdb = GETNDB(step);
  NdbRestarter res;

  if (res.getNumDbNodes() < 2)
  {
    return NDBT_OK;
  }

  while (loops--)
  {
    int master = res.getMasterNodeId();    

    int victim = 32768;
    for (Uint32 i = 0; i<res.getNumDbNodes(); i++)
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

    int rows = 10;
    int batch = 1;
    int row = (records - rows) ? rand() % (records - rows) : 0;

    if(hugoOps.pkUpdateRecord(pNdb, row, batch, rand()) != 0)
      goto err;

    for (int l = 1; l<5; l++)
    {
      if (hugoOps.execute_NoCommit(pNdb) != 0)
        goto err;

      if(hugoOps.pkUpdateRecord(pNdb, row, batch, rand()) != 0)
        goto err;
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
  TC_PROPERTY("ReadLockMode", -1);
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
  TC_PROPERTY("ReadLockMode", -1);
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
  TC_PROPERTY("ReadLockMode", -1);
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
  FINALIZER(runClearTable);
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
TESTCASE("Bug31525", ""){
  INITIALIZER(runBug31525);
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
TESTCASE("Bug32160", ""){
  INITIALIZER(runBug32160);
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
NDBT_TESTSUITE_END(testNodeRestart);

int main(int argc, const char** argv){
  ndb_init();
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

